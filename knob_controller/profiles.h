// profiles.h - per-app, per-mode shortcut tables (macOS bindings).
// Shortcuts marked "verify" should be checked against each app's own shortcut list.
#pragma once
#include "hid_out.h"

enum IconId : uint8_t {
  ICON_FIGMA, ICON_IMOVIE, ICON_ZOOM, ICON_FRAMES, ICON_NUDGE,
  ICON_SCRUB, ICON_EDIT, ICON_TIMELINE, ICON_BACK,
  ICON_GUITAR, ICON_TRANSPORT, ICON_TRACKS, ICON_THERMO
};

struct ModeDef {
  const char *name;
  Chord cw, ccw;          // encoder clockwise / counter-clockwise
  Chord cwAlt, ccwAlt;    // used instead of cw/ccw while the axis flag is set (if altAxis)
  bool altAxis;           // short press toggles axis instead of sending `press`
  Chord press;            // short press action (when !altAxis)
  const char *pressName;
  Chord touch[3];         // touch regions 1..3
  const char *touchName[3];
  IconId icon;            // glyph shown on the menu tile
};

struct AppProfile {
  const char *name;
  const ModeDef *modes;
  uint8_t modeCount;
  IconId icon;            // glyph shown on the home tile
  const char *homeName;   // optional label on the home tile (falls back to name)
};

extern const AppProfile kProfiles[];
extern const uint8_t kProfileCount;

// The thermostat tile sits after the HID apps; its profile index is kProfileCount.
inline bool isThermo(uint8_t p) { return p == kProfileCount; }
