// net.h - Wi-Fi + Home Assistant access in a background task, so loop() never blocks.
#pragma once
#include <Arduino.h>

struct ThermoState {
  bool valid;               // true once a reading has arrived
  char mode[12];            // heat, cool, heat_cool, off
  char action[12];          // heating, cooling, idle, off
  char preset[8];           // none, eco
  float cur, target;        // current temperature, single target
  float low, high;          // heat_cool targets (NAN if unused)
  float minT, maxT;         // thermostat limits
  int humidity;             // percent, -1 if unknown
  uint32_t updatedMs;
};

enum NetStatus : uint8_t { NET_OFF, NET_CONNECTING, NET_ONLINE, NET_WIFI_FAIL, NET_AUTH_FAIL, NET_HA_FAIL };

void netBegin();                               // create shared state (call once in setup)
void netSetActive(bool on);                    // start / stop polling Home Assistant (~every 10 s)
bool netThermo(int idx, ThermoState &out);     // copy of the latest reading; false if none yet
NetStatus netStatus();
const char *netStatusText();                   // short message for the screen

// Commands to a thermostat (queued, sent by the background task). Return false if the queue is full.
bool netSetTemp(int idx, float temp);                 // single target
bool netSetRange(int idx, float low, float high);     // heat/cool targets
bool netSetMode(int idx, const char *mode);           // heat, cool, heat_cool, off
bool netSetPreset(int idx, const char *preset);       // eco or none
bool netPopResult(bool &ok);                          // true once per finished command; ok = it succeeded

// One-shot connectivity check printed to Serial (the "test" command).
void netTest();
