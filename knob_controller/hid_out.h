// hid_out.h - non-blocking USB HID keyboard output (macOS bindings).
#pragma once
#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

// Modifier bit flags for a chord.
#define M_CMD   0x01   // Command  (KEY_LEFT_GUI)
#define M_OPT   0x02   // Option   (KEY_LEFT_ALT)
#define M_SHIFT 0x04   // Shift
#define M_CTRL  0x08   // Control

struct Chord {
  uint8_t mods;  // OR of M_* flags
  uint8_t key;   // ASCII char or a KEY_* constant; 0 = modifiers only
};

void hidBegin();
void hidSend(const Chord &c);   // queue a chord; dropped if the queue is full
void hidService();              // call every loop(); presses/releases on schedule
