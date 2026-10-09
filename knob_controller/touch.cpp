#include "touch.h"
#include "config.h"
#include <Wire.h>

static bool present = false;

static bool readRegs(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)TOUCH_ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool touchBegin() {
  pinMode(TOUCH_INT, INPUT);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);  delay(10);
  digitalWrite(TOUCH_RST, HIGH); delay(60);   // setup-time only
  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);

  Wire.beginTransmission(TOUCH_ADDR);
  present = (Wire.endTransmission() == 0);

  if (present) {
    writeReg(0xFE, 0x01);   // disable auto-sleep so the chip keeps answering between touches
    uint8_t id = 0;
    readRegs(0xA7, &id, 1);
    Serial.printf("[TOUCH] CST816 found at 0x%02X, chip id 0x%02X\n", TOUCH_ADDR, id);
  } else {
    Serial.printf("[TOUCH] no answer at 0x%02X; scanning I2C (SDA %d, SCL %d)...\n", TOUCH_ADDR, TOUCH_SDA, TOUCH_SCL);
    int found = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) { Serial.printf("[TOUCH]   device at 0x%02X\n", a); found++; }
    }
    if (!found) Serial.println("[TOUCH]   nothing found: SDA/SCL pins are probably wrong");
  }
  return present;
}

// Map raw chip coordinates to the 240x240 screen.
static void mapXY(int rx, int ry, int &x, int &y) {
  x = rx; y = ry;
  if (TOUCH_SWAP_XY) { int t = x; x = y; y = t; }
  if (TOUCH_FLIP_X) x = 239 - x;
  if (TOUCH_FLIP_Y) y = 239 - y;
  x = constrain(x, 0, 239);
  y = constrain(y, 0, 239);
}

TouchEvent touchRead() {
  static uint32_t lastPoll = 0;
  static bool down = false;
  static int sx, sy, lx, ly;
  static uint32_t downAt;

  TouchEvent ev = { TOUCH_NONE, 0, 0 };
  if (!present) return ev;

  uint32_t now = millis();
  if (now - lastPoll < 10) return ev;      // 100 Hz is plenty
  lastPoll = now;

  uint8_t r[6];
  bool ok = readRegs(0x01, r, 6);
  bool touching = ok && (r[1] & 0x0F) > 0;

  if (touching) {
    int rx = ((r[2] & 0x0F) << 8) | r[3];
    int ry = ((r[4] & 0x0F) << 8) | r[5];
    int x, y; mapXY(rx, ry, x, y);
    if (!down) { down = true; sx = x; sy = y; downAt = now; }
    lx = x; ly = y;
    return ev;
  }

  if (down) {                              // finger lifted: classify the gesture
    down = false;
    int dx = lx - sx, dy = ly - sy;
    ev.x = sx; ev.y = sy;
    if (abs(dx) < TOUCH_TAP_MAX_MOVE && abs(dy) < TOUCH_TAP_MAX_MOVE && (now - downAt) < TOUCH_TAP_MAX_MS) {
      ev.type = TOUCH_TAP;
    } else if (abs(dx) >= abs(dy) && abs(dx) >= TOUCH_SWIPE_MIN) {
      ev.type = dx < 0 ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;
    } else if (abs(dy) >= TOUCH_SWIPE_MIN) {
      ev.type = dy < 0 ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;
    }
  }
  return ev;
}
