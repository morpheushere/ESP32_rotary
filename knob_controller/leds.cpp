#include "leds.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static bool active = false;
static uint8_t pr, pg, pb;
static uint32_t startedAt = 0, lastShow = 0;

static void setAll(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, r, g, b);
  strip.show();
}

void ledsBegin() {
  strip.begin();
  setAll(0, 0, 0);
  delayMicroseconds(300);   // let the data latch
  setAll(0, 0, 0);          // send twice in case the first frame was clipped at power-up
}

void ledsPulse(uint8_t r, uint8_t g, uint8_t b) {
  pr = r; pg = g; pb = b;
  startedAt = millis();
  active = true;
}

void ledsService() {
  if (!active) return;
  uint32_t now = millis();
  if (now - lastShow < LED_FRAME_MS) return;   // limit refresh; show() briefly blocks interrupts
  lastShow = now;

  float t = (float)(now - startedAt) / LED_PULSE_MS;
  if (t >= 1.0f) { setAll(0, 0, 0); active = false; return; }

  float k = (1.0f - t);
  k = k * k * LED_PEAK;                         // quick fade, dim peak
  setAll(pr * k, pg * k, pb * k);
}
