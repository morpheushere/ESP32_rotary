#include "input.h"
#include "config.h"

// Gray-code transition table, indexed by (prevAB << 2) | newAB.
// Invalid transitions (bounce) score 0, so rotation needs no millis debounce.
static const int8_t kTable[16] = {
   0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

static uint8_t prevAB = 0;
static int16_t accum = 0;

void inputBegin() {
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  prevAB = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
}

int encoderRead() {
  uint8_t ab = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
  if (ab != prevAB) {
    accum += kTable[(prevAB << 2) | ab];
    prevAB = ab;
  }
  int detents = accum / ENCODER_STEPS_PER_DETENT;
  accum -= detents * ENCODER_STEPS_PER_DETENT;
  return detents;
}

ButtonEvent buttonRead() {
  static bool stable = false;          // debounced state: true = pressed
  static bool lastRaw = false;
  static uint32_t rawChangedAt = 0;
  static uint32_t pressedAt = 0;
  static bool longFired = false, xlongFired = false;

  uint32_t now = millis();
  bool raw = (digitalRead(ENCODER_BUTTON) == LOW);

  if (raw != lastRaw) { lastRaw = raw; rawChangedAt = now; }

  if (raw != stable && (now - rawChangedAt) >= BUTTON_DEBOUNCE_MS) {
    stable = raw;
    if (stable) {
      pressedAt = now;
      longFired = xlongFired = false;
    } else if (!longFired && !xlongFired) {
      return BTN_SHORT;                // released before any long threshold
    }
  }

  if (stable) {
    uint32_t held = now - pressedAt;
    if (!xlongFired && held >= BUTTON_XLONG_MS) { xlongFired = true; return BTN_XLONG; }
    if (!longFired && !xlongFired && held >= BUTTON_LONG_MS) { longFired = true; return BTN_LONG; }
  }
  return BTN_NONE;
}
