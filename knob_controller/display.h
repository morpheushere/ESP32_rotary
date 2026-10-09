// display.h - GC9A01 round screen UI (LovyanGFX): home carousel, app menu, active mode page.
#pragma once
#include <Arduino.h>
#include "profiles.h"

enum Level : uint8_t { LV_HOME, LV_MENU, LV_ACTIVE };

// Everything the renderer needs to draw one screen.
struct Scene {
  Level level;
  uint8_t profile;      // selected app (valid for MENU and ACTIVE)
  uint8_t index;        // carousel position: app index (HOME) or mode index / Back (MENU)
  uint8_t mode;         // active mode (ACTIVE)
  bool axisVertical;    // for modes with altAxis
};

void displayBegin();

// Show a scene. dir = -1 / +1 slides the carousel left / right, 0 = no animation.
void displaySetScene(const Scene &s, int dir);

// Call every loop(); advances the slide animation and redraws when needed.
void displayLoop();

// Optimistic values shown on the dial while a change is being sent (editLow picks which heat/cool handle).
void displayThermoPending(bool pending, float target, float low, float high, bool editLow);

// A short notice drawn on the dial (e.g. "Press again: turn OFF").
void displayNotice(const char *msg, uint32_t ms);

// Flash a one-line message near the bottom of the ring (e.g. last action).
void displayToast(const char *msg);
