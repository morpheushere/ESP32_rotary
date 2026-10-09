// config.h - pins and timing for the knob controller.
// Pins are for the Elecrow CrowPanel 1.28" rotary display (from the vendor wiki).
// They are UNVERIFIED on this unit until the encoder test passes.
#pragma once

// ---- Rotary encoder ----
#define ENCODER_A        45   // CLK
#define ENCODER_B        42   // DT
#define ENCODER_BUTTON   41   // SW (active low)

// Quadrature transitions per physical detent (usually 4, sometimes 2).
#define ENCODER_STEPS_PER_DETENT 4

// ---- Display (GC9A01, SPI) - from the vendor wiki, verified when the screen lights ----
#define DISPLAY_SCLK  10
#define DISPLAY_MOSI  11
#define DISPLAY_DC     3
#define DISPLAY_CS     9
#define DISPLAY_RST   14
#define DISPLAY_BL    46
#define LCD_PWR_EN1    1   // LCD 3V3 rail enable (drive HIGH)
#define LCD_PWR_EN2    2   // backlight (LEDA) 3V3 rail enable (drive HIGH)

// ---- Touch (CST816D, I2C) - from the vendor wiki, verified when the chip answers ----
#define TOUCH_SDA      6
#define TOUCH_SCL      7
#define TOUCH_INT      5
#define TOUCH_RST     13
#define TOUCH_ADDR  0x15
// If taps land in the wrong place, flip these (coordinates are mapped to the 240x240 screen).
#define TOUCH_SWAP_XY  0
#define TOUCH_FLIP_X   0
#define TOUCH_FLIP_Y   0
#define TOUCH_TAP_MAX_MOVE   25    // px: less than this between down and up = tap
#define TOUCH_TAP_MAX_MS    600
#define TOUCH_SWIPE_MIN      40    // px: at least this far = swipe

// ---- RGB LED ring under the knob (5x WS2812) - kept off ----
#define LED_PIN        48
#define LED_COUNT       5
#define LED_PEAK        0.10f   // click-pulse peak brightness (0..1): subtle
#define LED_PULSE_MS    260     // pulse length
#define LED_FRAME_MS    20      // min time between LED updates

// ---- Home Assistant (not secret; Wi-Fi password and token are entered over serial and stored in flash) ----
#define HA_URL_DEFAULT "http://10.0.0.111:8123"
#define THERMOSTAT_COUNT 2
static const char *const THERMOSTAT_IDS[THERMOSTAT_COUNT]   = { "climate.living_room", "climate.upstairs" };
static const char *const THERMOSTAT_NAMES[THERMOSTAT_COUNT] = { "Living Room", "Upstairs" };

// ---- Thermostat control safety (degrees F) ----
#define THERMO_STEP            1.0f     // change per knob detent
#define THERMO_RANGE_GAP       3.0f     // minimum gap between heat/cool targets
#define THERMO_COMMIT_DELAY_MS 800      // wait this long after the last turn before sending
#define THERMO_MIN_SEND_GAP_MS 1500     // never send setpoints more often than this
#define THERMO_REVERT_MS       9000     // give up waiting for Home Assistant to confirm after this
#define THERMO_OFF_CONFIRM_MS  3000     // second press within this window confirms turning OFF

// ---- Button timing (ms) ----
#define BUTTON_DEBOUNCE_MS    25
#define BUTTON_LONG_MS        600    // long press: next mode
#define BUTTON_XLONG_MS       1800   // extra-long press: switch app profile

// ---- HID timing (ms) ----
#define HID_KEY_HOLD_MS       12     // how long a chord stays pressed
#define HID_KEY_GAP_MS        8      // gap before the next queued chord
#define HID_QUEUE_SIZE        16
