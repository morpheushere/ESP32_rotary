# ESP32 rotary knob controller

A USB macro knob and smart-home dial for the **Elecrow CrowPanel 1.28" ESP32-S3 rotary display**
(240x240 round GC9A01 screen, CST816D touch, rotary encoder with push button, 5 WS2812 LEDs).

- **Figma, iMovie and GarageBand** profiles: the knob is a USB keyboard that sends macOS shortcuts.
- **Thermostat** tile: a Nest-style dial that reads and sets Google Nest thermostats through Home Assistant.

## Controls

| Input | Action |
|---|---|
| Turn | Browse menus, or the mode's action (zoom, scrub, set temperature...) |
| Press | Select, or the mode's press action |
| Long press | Back one level |
| Extra-long press (~2 s) | Straight to Home |
| Touch | Tap to select, swipe left/right to browse, swipe down to go back, bottom zones fire macros |

Menus never send keystrokes; HID output only happens inside an app mode.

## Build and flash

Uses `arduino-cli` with the esp32 core 3.x, plus the libraries LovyanGFX, Adafruit NeoPixel and ArduinoJson.

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,FlashMode=qio --output-dir build knob_controller
```

USB mode is **USB-OTG (TinyUSB)** so the board enumerates as a keyboard. In this mode the usual auto-reset into the
bootloader does not work. Either hold BOOT while plugging in, or send the single character `b` over the serial port
(the firmware then reboots into the bootloader), and upload to the port that appears.

## Thermostat setup

Credentials are never stored in source. Type them over the serial port (e.g. `screen <port> 115200`):

```
set ssid <wifi name>
set pass <wifi password>
set token <Home Assistant long-lived access token>
show        # prints what is set, never the secrets
test        # joins Wi-Fi and reads each thermostat
```

Edit `config.h` for your Home Assistant address and thermostat entity IDs. Setpoint changes are debounced, rate
limited, clamped to the thermostat's range, and turning a thermostat OFF needs a second press.

## Layout

`knob_controller/`: `profiles.*` (shortcut tables), `hid_out.*` (USB keyboard), `input.*` (encoder and button),
`touch.*`, `display.*` (UI and dial), `leds.*`, `net.*` and `settings.*` (Wi-Fi and Home Assistant).

Shortcuts for iMovie and GarageBand are from memory of the apps' defaults; check them against each app's own list.
