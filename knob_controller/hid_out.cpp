#include "hid_out.h"
#include "config.h"

static USBHIDKeyboard Keyboard;

static Chord queue_[HID_QUEUE_SIZE];
static uint8_t qHead = 0, qCount = 0;

static bool pressed = false;      // a chord is currently held down
static uint32_t nextAt = 0;       // time of the next press/release action

static void pressChord(const Chord &c) {
  if (c.mods & M_CMD)   Keyboard.press(KEY_LEFT_GUI);
  if (c.mods & M_OPT)   Keyboard.press(KEY_LEFT_ALT);
  if (c.mods & M_SHIFT) Keyboard.press(KEY_LEFT_SHIFT);
  if (c.mods & M_CTRL)  Keyboard.press(KEY_LEFT_CTRL);
  if (c.key)            Keyboard.press(c.key);
}

void hidBegin() {
  Keyboard.begin();
  USB.begin();
}

void hidSend(const Chord &c) {
  if (qCount >= HID_QUEUE_SIZE) return;  // fast spin: drop rather than lag behind
  queue_[(qHead + qCount) % HID_QUEUE_SIZE] = c;
  qCount++;
}

void hidService() {
  uint32_t now = millis();
  if ((int32_t)(now - nextAt) < 0) return;

  if (pressed) {
    Keyboard.releaseAll();
    pressed = false;
    nextAt = now + HID_KEY_GAP_MS;
  } else if (qCount > 0) {
    pressChord(queue_[qHead]);
    qHead = (qHead + 1) % HID_QUEUE_SIZE;
    qCount--;
    pressed = true;
    nextAt = now + HID_KEY_HOLD_MS;
  }
}
