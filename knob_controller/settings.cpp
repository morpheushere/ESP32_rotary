#include "settings.h"
#include "config.h"
#include <Preferences.h>

static Preferences np;
static NetSettings cur;

void settingsLoad() {
  np.begin("net", true);
  cur.ssid  = np.getString("ssid", "");
  cur.pass  = np.getString("pass", "");
  cur.url   = np.getString("url", HA_URL_DEFAULT);
  cur.token = np.getString("token", "");
  np.end();
}

const NetSettings &settings() { return cur; }

bool settingsSet(const String &key, const String &value) {
  const char *nk = nullptr;
  if (key == "ssid") { nk = "ssid"; cur.ssid = value; }
  else if (key == "pass") { nk = "pass"; cur.pass = value; }
  else if (key == "ha") { nk = "url"; cur.url = value; }
  else if (key == "token") { nk = "token"; cur.token = value; }
  else return false;

  np.begin("net", false);
  np.putString(nk, value);
  np.end();
  return true;
}

void settingsClear() {
  np.begin("net", false);
  np.clear();
  np.end();
  settingsLoad();
}

void settingsPrint() {
  Serial.printf("[NET] ssid : %s\n", cur.ssid.length() ? cur.ssid.c_str() : "(not set)");
  Serial.printf("[NET] pass : %s\n", cur.pass.length() ? ("set, " + String(cur.pass.length()) + " chars").c_str() : "(not set)");
  Serial.printf("[NET] ha   : %s\n", cur.url.c_str());
  Serial.printf("[NET] token: %s\n", cur.token.length() ? ("set, " + String(cur.token.length()) + " chars").c_str() : "(not set)");
}
