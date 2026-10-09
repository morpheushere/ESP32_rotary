// input.h - quadrature encoder (polled state table) and button (debounced, short/long).
#pragma once
#include <Arduino.h>

enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG, BTN_XLONG };

void inputBegin();

// Returns accumulated detents since the last call: +1 per clockwise detent,
// -1 per counter-clockwise. Call every loop() so no transitions are missed.
int encoderRead();

// Fires once on release (short) or once when the hold threshold passes (long, xlong).
ButtonEvent buttonRead();
