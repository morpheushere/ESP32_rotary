// leds.h - the 5 WS2812 LEDs at the base. Kept off, except for a brief dim pulse as click feedback.
// WS2812s keep their last colour until power is removed, so ledsBegin() writes "off" at start-up.
#pragma once
#include <Arduino.h>

void ledsBegin();                          // clear all LEDs
void ledsPulse(uint8_t r, uint8_t g, uint8_t b);   // start a short fade-out pulse in this colour
void ledsService();                        // call every loop(); non-blocking
