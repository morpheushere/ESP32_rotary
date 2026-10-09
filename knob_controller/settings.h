// settings.h - Wi-Fi and Home Assistant credentials, kept in flash (NVS), never in source code.
// They are entered over the serial console, e.g.:  set ssid MyWifi   set pass ...   set token ...
#pragma once
#include <Arduino.h>

struct NetSettings {
  String ssid, pass, url, token;
};

void settingsLoad();
const NetSettings &settings();

// key: "ssid", "pass", "ha" (URL) or "token". Returns false for an unknown key.
bool settingsSet(const String &key, const String &value);
void settingsClear();
void settingsPrint();   // never prints the password or token, only whether they are set
