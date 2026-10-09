// touch.h - CST816D capacitive touch: polled over I2C, converted to tap / swipe events.
#pragma once
#include <Arduino.h>

enum TouchType : uint8_t { TOUCH_NONE, TOUCH_TAP, TOUCH_SWIPE_LEFT, TOUCH_SWIPE_RIGHT,
                           TOUCH_SWIPE_UP, TOUCH_SWIPE_DOWN };

struct TouchEvent {
  TouchType type;
  int x, y;        // screen coordinates (0..239) where the touch started
};

// Resets the chip and probes the bus. Returns false (and scans I2C to Serial) if it doesn't answer.
bool touchBegin();

// Call every loop(). Returns an event once per completed gesture, else TOUCH_NONE.
TouchEvent touchRead();
