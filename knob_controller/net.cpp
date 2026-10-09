#include "net.h"
#include "config.h"
#include "settings.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const uint32_t POLL_MS = 10000;

static SemaphoreHandle_t mtx = nullptr;
static ThermoState states[THERMOSTAT_COUNT];
static volatile bool active = false, started = false, testBusy = false;
static volatile NetStatus status = NET_OFF;

void netBegin() {
  mtx = xSemaphoreCreateMutex();
  for (int i = 0; i < THERMOSTAT_COUNT; i++) { memset(&states[i], 0, sizeof(ThermoState)); states[i].humidity = -1; }
}

bool netThermo(int idx, ThermoState &out) {
  if (idx < 0 || idx >= THERMOSTAT_COUNT || !mtx) return false;
  xSemaphoreTake(mtx, portMAX_DELAY);
  out = states[idx];
  xSemaphoreGive(mtx);
  return out.valid;
}

NetStatus netStatus() { return status; }

const char *netStatusText() {
  switch (status) {
    case NET_CONNECTING: return "Connecting...";
    case NET_WIFI_FAIL:  return "No Wi-Fi";
    case NET_AUTH_FAIL:  return "Check token";
    case NET_HA_FAIL:    return "Can't reach HA";
    default:             return "Waiting...";
  }
}

// Joins Wi-Fi (blocking, so only call from a task). Gives up after ~20 s or when polling is switched off.
static bool connectWifi(bool needActive) {
  const NetSettings &s = settings();
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!s.ssid.length()) { Serial.println("[NET] no Wi-Fi name set: type  set ssid <name>"); return false; }

  WiFi.mode(WIFI_STA);
  WiFi.begin(s.ssid.c_str(), s.pass.c_str());
  Serial.printf("[NET] joining Wi-Fi \"%s\"...\n", s.ssid.c_str());
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    if (needActive && !active) return false;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[NET] Wi-Fi failed (wrong password, or 5GHz-only network?)"); return false; }
  Serial.printf("[NET] Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// Fetches one thermostat into states[idx]. Returns the HTTP code (or a negative error).
static int fetchInto(int idx) {
  const NetSettings &s = settings();
  String base = s.url;
  while (base.endsWith("/")) base.remove(base.length() - 1);

  HTTPClient http;
  http.setTimeout(6000);
  http.begin(base + "/api/states/" + THERMOSTAT_IDS[idx]);
  http.addHeader("Authorization", "Bearer " + s.token);
  int code = http.GET();

  if (code == 200) {
    JsonDocument filter;
    filter["state"] = true;
    JsonObject fa = filter["attributes"].to<JsonObject>();
    for (const char *k : { "current_temperature", "temperature", "target_temp_low", "target_temp_high",
                           "hvac_action", "current_humidity", "preset_mode", "min_temp", "max_temp" })
      fa[k] = true;

    JsonDocument doc;
    if (!deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter))) {
      JsonObject a = doc["attributes"];
      ThermoState t;
      memset(&t, 0, sizeof(t));
      t.valid = true;
      strlcpy(t.mode, doc["state"] | "off", sizeof(t.mode));
      strlcpy(t.action, a["hvac_action"] | "idle", sizeof(t.action));
      strlcpy(t.preset, a["preset_mode"] | "none", sizeof(t.preset));
      t.cur      = a["current_temperature"] | NAN;
      t.target   = a["temperature"] | NAN;
      t.low      = a["target_temp_low"] | NAN;
      t.high     = a["target_temp_high"] | NAN;
      t.minT     = a["min_temp"] | 50.0f;
      t.maxT     = a["max_temp"] | 90.0f;
      t.humidity = (int)(a["current_humidity"] | -1.0f);
      t.updatedMs = millis();
      xSemaphoreTake(mtx, portMAX_DELAY);
      states[idx] = t;
      xSemaphoreGive(mtx);
    } else {
      code = -2;   // unreadable reply
    }
  }
  http.end();
  return code;
}

// ---- commands ----
enum CmdType : uint8_t { CMD_TEMP, CMD_RANGE, CMD_MODE, CMD_PRESET };
struct NetCmd { CmdType type; uint8_t idx; float a, b; char s[12]; };

static QueueHandle_t cmdQ = nullptr;
static volatile bool resultReady = false, resultOk = false;

static bool enqueue(const NetCmd &c) {
  if (!cmdQ) cmdQ = xQueueCreate(8, sizeof(NetCmd));
  return xQueueSend(cmdQ, &c, 0) == pdTRUE;
}
bool netSetTemp(int idx, float t)               { NetCmd c = { CMD_TEMP,  (uint8_t)idx, t, 0, "" }; return enqueue(c); }
bool netSetRange(int idx, float lo, float hi)   { NetCmd c = { CMD_RANGE, (uint8_t)idx, lo, hi, "" }; return enqueue(c); }
bool netSetMode(int idx, const char *m)         { NetCmd c = { CMD_MODE,  (uint8_t)idx, 0, 0, "" }; strlcpy(c.s, m, sizeof(c.s)); return enqueue(c); }
bool netSetPreset(int idx, const char *p)       { NetCmd c = { CMD_PRESET,(uint8_t)idx, 0, 0, "" }; strlcpy(c.s, p, sizeof(c.s)); return enqueue(c); }

bool netPopResult(bool &ok) {
  if (!resultReady) return false;
  ok = resultOk; resultReady = false; return true;
}

// POST a Home Assistant service call. Returns true on HTTP 200.
static bool callService(const char *service, const String &body) {
  const NetSettings &s = settings();
  String base = s.url;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  HTTPClient http;
  http.setTimeout(8000);
  http.begin(base + "/api/services/climate/" + service);
  http.addHeader("Authorization", "Bearer " + s.token);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  Serial.printf("[NET] climate.%s -> HTTP %d\n", service, code);
  return code == 200;
}

static void runCmd(const NetCmd &c) {
  JsonDocument d;
  d["entity_id"] = THERMOSTAT_IDS[c.idx];
  const char *svc = "set_temperature";
  switch (c.type) {
    case CMD_TEMP:   d["temperature"] = c.a; break;
    case CMD_RANGE:  d["target_temp_low"] = c.a; d["target_temp_high"] = c.b; break;
    case CMD_MODE:   svc = "set_hvac_mode";   d["hvac_mode"] = c.s; break;
    case CMD_PRESET: svc = "set_preset_mode"; d["preset_mode"] = c.s; break;
  }
  String body; serializeJson(d, body);
  resultOk = callService(svc, body);
  resultReady = true;
}

static void pollTask(void *) {
  uint32_t lastPoll = 0, fastUntil = 0;
  bool first = true;
  for (;;) {
    if (!active) { first = true; vTaskDelay(pdMS_TO_TICKS(250)); continue; }

    if (WiFi.status() != WL_CONNECTED) {
      status = NET_CONNECTING;
      if (!connectWifi(true)) {
        if (active) status = NET_WIFI_FAIL;
        for (int t = 0; t < 8000 && active; t += 250) vTaskDelay(pdMS_TO_TICKS(250));
        continue;
      }
    }
    if (!settings().token.length()) { status = NET_AUTH_FAIL; vTaskDelay(pdMS_TO_TICKS(1000)); continue; }

    // Send queued commands first, then re-read soon and a few times, since Nest confirms after a delay.
    bool sent = false;
    NetCmd c;
    while (cmdQ && xQueueReceive(cmdQ, &c, 0) == pdTRUE) { runCmd(c); sent = true; }
    if (sent) fastUntil = millis() + 9000;

    uint32_t now = millis();
    uint32_t interval = (now < fastUntil) ? 2000 : POLL_MS;
    if (first || sent || now - lastPoll >= interval) {
      first = false;
      bool authFail = false, anyOk = false;
      for (int i = 0; i < THERMOSTAT_COUNT && active; i++) {
        int code = fetchInto(i);
        if (code == 401) authFail = true;
        if (code == 200) anyOk = true;
      }
      status = authFail ? NET_AUTH_FAIL : (anyOk ? NET_ONLINE : NET_HA_FAIL);
      lastPoll = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void netSetActive(bool on) {
  if (on == active) return;
  if (!cmdQ) cmdQ = xQueueCreate(8, sizeof(NetCmd));
  if (on && !started) {
    started = true;
    xTaskCreate(pollTask, "net", 10240, nullptr, 1, nullptr);
  }
  active = on;
  if (on) status = (WiFi.status() == WL_CONNECTED) ? NET_ONLINE : NET_CONNECTING;
  else    status = NET_OFF;
}

// ---- "test" command ----
static void testTask(void *) {
  if (!settings().token.length()) {
    Serial.println("[NET] no token set: type  set token <value>");
  } else if (connectWifi(false)) {
    for (int i = 0; i < THERMOSTAT_COUNT; i++) {
      int c = fetchInto(i);
      if (c == 200) {
        ThermoState t; netThermo(i, t);
        Serial.printf("[NET] %s  mode=%s now=%.1f target=%.1f action=%s humidity=%d preset=%s range=%.0f-%.0f\n",
                      THERMOSTAT_IDS[i], t.mode, t.cur, t.target, t.action, t.humidity, t.preset, t.minT, t.maxT);
      } else if (c == 401) Serial.printf("[NET] %s: token rejected (401)\n", THERMOSTAT_IDS[i]);
      else if (c == 404)   Serial.printf("[NET] %s: not found (404)\n", THERMOSTAT_IDS[i]);
      else                 Serial.printf("[NET] %s: request failed (code %d)\n", THERMOSTAT_IDS[i], c);
    }
    Serial.println("[NET] test done");
  }
  testBusy = false;
  vTaskDelete(nullptr);
}

void netTest() {
  if (testBusy) { Serial.println("[NET] already running"); return; }
  testBusy = true;
  xTaskCreate(testTask, "nettest", 8192, nullptr, 1, nullptr);
}
