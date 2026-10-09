// knob_controller.ino - USB HID macro knob for Figma and iMovie on macOS.
//
// Three levels, navigated with the knob:
//   HOME    pick an app          turn: browse    press: open
//   MENU    pick a mode / Back   turn: browse    press: select   hold: back to HOME
//   ACTIVE  controls the app     turn / press: mode action        hold: back to MENU
// Extra-long press from anywhere returns to HOME.
// HID output is only sent in ACTIVE, so browsing menus never types into an app.
//
// Serial helpers: 1/2/3 = simulated touch regions (ACTIVE only), h = home,
// b = reboot into the bootloader (reflash without the BOOT button).
//
// Arduino IDE / arduino-cli settings: ESP32S3 Dev Module, USB Mode = USB-OTG (TinyUSB),
// USB CDC On Boot = Enabled, Flash 16MB.

#include <Preferences.h>
#include <esp_system.h>
#include <soc/rtc_cntl_reg.h>
#include "config.h"
#include "hid_out.h"
#include "input.h"
#include "profiles.h"
#include "display.h"
#include "leds.h"
#include "touch.h"
#include "settings.h"
#include "net.h"

static Preferences prefs;

static Level level = LV_HOME;
static uint8_t profile = 0;          // selected app
static uint8_t homeIdx = 0;          // highlighted app on HOME
static uint8_t menuIdx = 0;          // highlighted entry on MENU (modeCount = Back)
static uint8_t mode = 0;             // active mode
static bool verticalAxis = false;    // for modes with altAxis (Figma nudge)

static const ModeDef &curMode() { return kProfiles[profile].modes[mode]; }

// Draws the current scene on the round screen and mirrors it to Serial.
static void showScene(int dir) {
  Scene s = { level, profile, (uint8_t)(level == LV_HOME ? homeIdx : menuIdx), mode, verticalAxis };
  displaySetScene(s, dir);
  netSetActive(isThermo(profile) && level != LV_HOME);   // poll Home Assistant only while the thermostat is open

  switch (level) {
    case LV_HOME:
      Serial.printf("[SCREEN] HOME > %s\n", homeIdx < kProfileCount ? kProfiles[homeIdx].name : "THERMOSTAT");
      break;
    case LV_MENU:
      if (isThermo(profile))
        Serial.printf("[SCREEN] THERMOSTAT menu > %s\n", menuIdx < THERMOSTAT_COUNT ? THERMOSTAT_NAMES[menuIdx] : "BACK");
      else if (menuIdx >= kProfiles[profile].modeCount)
        Serial.printf("[SCREEN] %s menu > BACK\n", kProfiles[profile].name);
      else
        Serial.printf("[SCREEN] %s menu > %s\n", kProfiles[profile].name, kProfiles[profile].modes[menuIdx].name);
      break;
    case LV_ACTIVE: {
      if (isThermo(profile)) { Serial.printf("[SCREEN] THERMOSTAT > %s\n", THERMOSTAT_NAMES[mode]); break; }
      const ModeDef &d = curMode();
      Serial.printf("[SCREEN] %s > %s (%d/%d)\n", kProfiles[profile].name, d.name,
                    mode + 1, kProfiles[profile].modeCount);
      Serial.printf("         press: %s\n", d.altAxis ? "toggle axis" : d.pressName);
      Serial.printf("         touch: 1=%s  2=%s  3=%s\n", d.touchName[0], d.touchName[1], d.touchName[2]);
      break;
    }
  }
}

// Placeholder for the capacitive touch driver: region 1..3 -> macro (ACTIVE only).
void handleTouchPress(int region) {
  if (level != LV_ACTIVE || isThermo(profile) || region < 1 || region > 3) return;
  hidSend(curMode().touch[region - 1]);
  displayToast(curMode().touchName[region - 1]);
  Serial.printf("[TOUCH] region %d: %s\n", region, curMode().touchName[region - 1]);
}

static void goHome() {
  if (isThermo(profile)) editClear();
  level = LV_HOME;
  homeIdx = profile;
  showScene(0);
}

// ---------------------------------------------------------------- thermostat control
// Turning edits a pending target shown at once on the dial. It is sent only after the knob has been
// still for a moment (and not more often than a minimum gap), then kept on screen until Home Assistant
// reports the same value or we give up and revert.
static struct {
  bool active, sent;
  float target, low, high;
  uint32_t lastTurn, sentAt;
} edit;
static bool editLow = false;               // heat/cool mode: which handle the knob moves
static uint32_t lastSend = 0, offArmedUntil = 0;

static void editClear() {
  edit.active = false;
  displayThermoPending(false, NAN, NAN, NAN, editLow);
}

static void thermoTurn(int detents) {
  ThermoState t;
  if (!netThermo(mode, t))               { displayNotice("Not connected", 1500); return; }
  if (!strcmp(t.mode, "off"))            { displayNotice("Thermostat is off", 1500); return; }
  if (!strcmp(t.preset, "eco"))          { displayNotice("Eco is on (use Nest app)", 2200); return; }

  const bool range = !strcmp(t.mode, "heat_cool");
  if (!edit.active) {                    // start from the thermostat's current setpoints
    edit.active = true;
    edit.target = t.target; edit.low = t.low; edit.high = t.high;
  }
  edit.sent = false;
  edit.lastTurn = millis();

  if (range) {
    if (isnan(edit.low) || isnan(edit.high)) return;
    if (editLow) edit.low  = constrain(edit.low  + detents * THERMO_STEP, t.minT, edit.high - THERMO_RANGE_GAP);
    else         edit.high = constrain(edit.high + detents * THERMO_STEP, edit.low + THERMO_RANGE_GAP, t.maxT);
  } else {
    if (isnan(edit.target)) return;
    edit.target = constrain(edit.target + detents * THERMO_STEP, t.minT, t.maxT);
  }
  displayThermoPending(true, edit.target, edit.low, edit.high, editLow);
}

// Press: cycle heat -> cool -> heat/cool -> off. Turning OFF needs a second press.
static void thermoPress() {
  ThermoState t;
  if (!netThermo(mode, t)) { displayNotice("Not connected", 1500); return; }
  static const char *const order[] = { "heat", "cool", "heat_cool", "off" };
  int cur = -1;
  for (int i = 0; i < 4; i++) if (!strcmp(t.mode, order[i])) cur = i;
  const char *next = order[(cur + 1) % 4];

  if (!strcmp(next, "off") && millis() > offArmedUntil) {
    offArmedUntil = millis() + THERMO_OFF_CONFIRM_MS;
    displayNotice("Press again: turn OFF", THERMO_OFF_CONFIRM_MS);
    return;
  }
  offArmedUntil = 0;
  editClear();
  if (netSetMode(mode, next)) {
    char m[32]; snprintf(m, sizeof(m), "Switching to %s", !strcmp(next, "heat_cool") ? "HEAT/COOL" : next);
    for (char *c = m + 13; *c; c++) *c = toupper(*c);
    displayNotice(m, 2500);
  }
}

// Called every loop: sends the pending change when due, and clears it once confirmed or timed out.
static void thermoService() {
  bool ok;
  if (netPopResult(ok) && !ok) { editClear(); displayNotice("Send failed", 2500); }

  if (!edit.active || !isThermo(profile)) return;
  ThermoState t;
  if (!netThermo(mode, t)) return;
  const bool range = !strcmp(t.mode, "heat_cool");
  uint32_t now = millis();

  if (!edit.sent) {
    if (now - edit.lastTurn >= THERMO_COMMIT_DELAY_MS && now - lastSend >= THERMO_MIN_SEND_GAP_MS) {
      bool queued = range ? netSetRange(mode, edit.low, edit.high) : netSetTemp(mode, edit.target);
      if (queued) { edit.sent = true; edit.sentAt = now; lastSend = now; }
    }
  } else {
    bool match = range ? (fabsf(t.low - edit.low) < 0.6f && fabsf(t.high - edit.high) < 0.6f)
                       : (fabsf(t.target - edit.target) < 0.6f);
    if (match || now - edit.sentAt > THERMO_REVERT_MS) {
      if (!match) displayNotice("Not applied", 2500);
      editClear();
    }
  }
}

static void onRotate(int detents) {
  if (level == LV_ACTIVE) {
    if (isThermo(profile)) { thermoTurn(detents > 0 ? 1 : -1); return; }
    const ModeDef &d = curMode();
    bool cw = detents > 0;
    const Chord &c = (d.altAxis && verticalAxis) ? (cw ? d.cwAlt : d.ccwAlt)
                                                 : (cw ? d.cw : d.ccw);
    for (int i = 0; i < abs(detents); i++) hidSend(c);
    return;
  }

  // Menus: move the highlight (clamped, no wrap) and slide the carousel.
  uint8_t &idx = (level == LV_HOME) ? homeIdx : menuIdx;
  int last = (level == LV_HOME) ? kProfileCount                                   // apps + thermostat tile
           : isThermo(profile) ? THERMOSTAT_COUNT : kProfiles[profile].modeCount;  // MENU has a Back tile
  int next = constrain((int)idx + detents, 0, last);
  if (next == idx) return;
  int dir = next > idx ? 1 : -1;
  idx = next;
  showScene(dir);
}

// Accent colour per app (RGB), matching the on-screen accents.
static const uint8_t kAccentRGB[][3] = { {162, 89, 255}, {59, 130, 246}, {255, 149, 0}, {45, 212, 191} };

// Dim click feedback in the accent of the app the knob is currently on.
static void clickFeedback() {
  uint8_t a = ((level == LV_HOME) ? homeIdx : profile) % 4;
  ledsPulse(kAccentRGB[a][0], kAccentRGB[a][1], kAccentRGB[a][2]);
}

// Short press / tap on the centre: select in menus, mode action in ACTIVE.
static void actSelect() {
  if (level == LV_HOME) {
    profile = homeIdx;
    prefs.putUChar("app", profile);
    level = LV_MENU;
    menuIdx = 0;
    showScene(0);
  } else if (level == LV_MENU) {
    int backIdx = isThermo(profile) ? THERMOSTAT_COUNT : kProfiles[profile].modeCount;
    if (menuIdx >= backIdx) goHome();                                // Back tile
    else { mode = menuIdx; verticalAxis = false; level = LV_ACTIVE; showScene(0); }
  } else {
    if (isThermo(profile)) { thermoPress(); return; }
    const ModeDef &d = curMode();
    if (d.altAxis) { verticalAxis = !verticalAxis; showScene(0); }
    else { hidSend(d.press); displayToast(d.pressName); }
  }
}

// Long press / swipe down: up one level.
static void actBack() {
  if (level == LV_ACTIVE && isThermo(profile)) editClear();
  if (level == LV_ACTIVE)    { level = LV_MENU; menuIdx = mode; showScene(0); }
  else if (level == LV_MENU) { goHome(); }
}

static void onButton(ButtonEvent e) {
  switch (e) {
    case BTN_SHORT: actSelect(); break;
    case BTN_LONG:  actBack();   break;
    case BTN_XLONG: if (level != LV_HOME) goHome(); break;   // straight home
    default: break;
  }
  clickFeedback();   // after the state change, so it shows the app we landed on
}

// Touch: tap bottom zones (ACTIVE) for macros, tap the centre to select, swipe to browse / go back.
static void onTouch(const TouchEvent &t) {
  switch (t.type) {
    case TOUCH_TAP:
      if (level == LV_ACTIVE && isThermo(profile)) {
        ThermoState ts;                                                         // heat/cool: a tap swaps the LOW / HIGH handle
        if (netThermo(mode, ts) && !strcmp(ts.mode, "heat_cool")) {
          editLow = !editLow;
          displayNotice(editLow ? "Editing LOW" : "Editing HIGH", 1500);
          if (edit.active) displayThermoPending(true, edit.target, edit.low, edit.high, editLow);
          else displayThermoPending(false, NAN, NAN, NAN, editLow);
          clickFeedback();
        }
      } else if (level == LV_ACTIVE) {
        if (t.y >= 150 && t.y <= 215) {                 // the three bottom zones
          int region = t.x < 80 ? 1 : (t.x < 160 ? 2 : 3);
          handleTouchPress(region);
          clickFeedback();
        }
      } else if (t.y > 60 && t.y < 175 && abs(t.x - 120) < 60) {   // centre tile
        actSelect();
        clickFeedback();
      }
      break;
    case TOUCH_SWIPE_LEFT:
    case TOUCH_SWIPE_RIGHT:
      if (level == LV_ACTIVE && isThermo(profile)) {                          // switch thermostat
        int n = constrain((int)mode + (t.type == TOUCH_SWIPE_LEFT ? 1 : -1), 0, THERMOSTAT_COUNT - 1);
        if (n != mode) { editClear(); mode = n; showScene(0); }
      } else if (level != LV_ACTIVE) {
        onRotate(t.type == TOUCH_SWIPE_LEFT ? +1 : -1);                       // content moves left = next
      }
      break;
    case TOUCH_SWIPE_DOWN:  actBack(); clickFeedback(); break;
    default: break;
  }
}

static void rebootToBootloader() {   // reflash without the BOOT button
  Serial.println("[SYS] rebooting to bootloader");
  Serial.flush();
  delay(100);
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
  esp_restart();
}

// Typed lines: "set ssid <name>", "set pass <password>", "set ha <url>", "set token <token>",
// "show", "clear", "test". Values are never echoed back.
static void handleLine(String line) {
  line.trim();
  if (!line.length()) return;
  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  String rest = sp < 0 ? "" : line.substring(sp + 1);
  rest.trim();

  if (cmd == "set") {
    int sp2 = rest.indexOf(' ');
    String key = sp2 < 0 ? rest : rest.substring(0, sp2);
    String val = sp2 < 0 ? "" : rest.substring(sp2 + 1);
    val.trim();
    if (!val.length()) { Serial.println("[NET] usage: set ssid|pass|ha|token <value>"); return; }
    if (!settingsSet(key, val)) { Serial.println("[NET] unknown key; use ssid, pass, ha or token"); return; }
    if (key == "ssid" || key == "ha") Serial.printf("[NET] saved %s: %s\n", key.c_str(), val.c_str());
    else                              Serial.printf("[NET] saved %s (%d chars)\n", key.c_str(), val.length());
  } else if (cmd == "show")  { settingsPrint(); }
  else if (cmd == "clear")   { settingsClear(); Serial.println("[NET] all network settings cleared"); }
  else if (cmd == "test")    { netTest(); }
  else Serial.println("[NET] commands: set <ssid|pass|ha|token> <value>, show, clear, test");
}

// Single keys act immediately (1/2/3 = simulated touch, h = home, i = re-probe touch, b = bootloader);
// anything else is collected as a line and run when Enter is pressed.
static void serialCommands() {
  static String line;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { handleLine(line); line = ""; continue; }
    if (line.length() == 0) {
      if (ch >= '1' && ch <= '3') { handleTouchPress(ch - '0'); continue; }
      if (ch == 'h') { goHome(); continue; }
      if (ch == 'i') { touchBegin(); continue; }
      if (ch == 'b') { rebootToBootloader(); continue; }
    }
    if (line.length() < 400) line += ch;
  }
}

void setup() {
  Serial.begin(115200);
  inputBegin();
  ledsBegin();
  displayBegin();
  touchBegin();
  hidBegin();
  settingsLoad();
  netBegin();
  prefs.begin("knob", false);
  homeIdx = profile = min<uint8_t>(prefs.getUChar("app", 0), kProfileCount);   // kProfileCount = thermostat   // last-used app pre-selected
  delay(500);   // one-time startup settle; loop() itself never blocks
  showScene(0);
}

void loop() {
  int d = encoderRead();
  if (d) { onRotate(d); Serial.printf("[ENC] %+d\n", d); }

  ButtonEvent b = buttonRead();
  if (b != BTN_NONE) { onButton(b); Serial.printf("[BTN] %d\n", b); }

  TouchEvent te = touchRead();
  if (te.type != TOUCH_NONE) { Serial.printf("[TOUCH] type %d at (%d,%d)\n", te.type, te.x, te.y); onTouch(te); }

  serialCommands();
  hidService();
  thermoService();
  ledsService();
  displayLoop();
}
