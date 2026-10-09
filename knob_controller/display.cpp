#include "display.h"
#include "config.h"
#include "net.h"
#include <LovyanGFX.hpp>

// Panel wiring for the CrowPanel 1.28" (GC9A01 over SPI, PWM backlight).
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel_;
  lgfx::Bus_SPI      bus_;
  lgfx::Light_PWM    light_;
public:
  LGFX() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.pin_sclk = DISPLAY_SCLK;
      cfg.pin_mosi = DISPLAY_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc   = DISPLAY_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = DISPLAY_CS;
      cfg.pin_rst = DISPLAY_RST;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.invert = true;     // GC9A01 modules are normally inverted
      cfg.rgb_order = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = DISPLAY_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }
};

static LGFX tft;
static LGFX_Sprite cv(&tft);   // off-screen frame; pushed in one go so animation doesn't flicker

static const uint16_t kAppColor[] = { 0xA2BF /* Figma purple */, 0x3D3F /* iMovie blue */, 0xFCA0 /* GarageBand orange */, 0x2EB7 /* thermostat teal */ };
static const int CX = 120;
static const int TILE_W = 104;          // distance between carousel tile centres
static const uint32_t ANIM_MS = 140;

static Scene scene;
static bool dirty = true;
static int animDir = 0;
static uint32_t animStart = 0;

// ---------------------------------------------------------------- icons
// Thin line-art glyphs (crisp 1px strokes, no anti-aliasing), set inside a round badge.
// R is the badge radius; the glyph occupies about half of it, leaving a clear margin.
static const uint16_t kBadgeBg = 0x2104;   // dark grey

static void drawIcon(IconId id, int cx, int cy, int R, uint16_t c, bool dim) {
  const float g = R * 0.46f;                         // glyph half-size
  auto X = [&](float f) { return cx + g * f; };
  auto Y = [&](float f) { return cy + g * f; };
  auto line = [&](float x0, float y0, float x1, float y1) { cv.drawLine(lroundf(X(x0)), lroundf(Y(y0)), lroundf(X(x1)), lroundf(Y(y1)), c); };
  auto ring = [&](float x, float y, float rad) { cv.drawCircle(lroundf(X(x)), lroundf(Y(y)), lroundf(g * rad), c); };

  // Badge: dark disc with a thin 1px ring
  cv.fillCircle(cx, cy, R, kBadgeBg);
  cv.drawCircle(cx, cy, R, dim ? (uint16_t)TFT_DARKGREY : c);

  switch (id) {
    case ICON_FIGMA:    // frame tool: the "#" frame mark
      line(-0.35f, -0.9f, -0.35f, 0.9f);  line(0.35f, -0.9f, 0.35f, 0.9f);
      line(-0.9f, -0.35f, 0.9f, -0.35f);  line(-0.9f, 0.35f, 0.9f, 0.35f);
      break;
    case ICON_IMOVIE: { // star
      float px[5], py[5];
      for (int i = 0; i < 5; i++) { float a = (-90 + 72 * i) * DEG_TO_RAD; px[i] = cosf(a); py[i] = sinf(a); }
      for (int i = 0; i < 5; i++) { int j = (i + 2) % 5; line(px[i], py[i], px[j], py[j]); }
      break; }
    case ICON_ZOOM:     // magnifier with plus
      ring(-0.15f, -0.15f, 0.62f);
      line(0.32f, 0.32f, 0.9f, 0.9f);
      line(-0.4f, -0.15f, 0.1f, -0.15f);  line(-0.15f, -0.4f, -0.15f, 0.1f);
      break;
    case ICON_FRAMES:   // two frames and a step arrow
      line(-0.95f, -0.55f, -0.15f, -0.55f); line(-0.15f, -0.55f, -0.15f, 0.55f);
      line(-0.15f, 0.55f, -0.95f, 0.55f);   line(-0.95f, 0.55f, -0.95f, -0.55f);
      line(0.15f, -0.55f, 0.95f, -0.55f);   line(0.95f, -0.55f, 0.95f, 0.55f);
      line(0.95f, 0.55f, 0.15f, 0.55f);     line(0.15f, 0.55f, 0.15f, -0.55f);
      break;
    case ICON_NUDGE:    // four arrows
      line(0, -0.9f, 0, 0.9f);  line(-0.9f, 0, 0.9f, 0);
      line(0, -0.9f, -0.3f, -0.55f);  line(0, -0.9f, 0.3f, -0.55f);
      line(0, 0.9f, -0.3f, 0.55f);    line(0, 0.9f, 0.3f, 0.55f);
      line(-0.9f, 0, -0.55f, -0.3f);  line(-0.9f, 0, -0.55f, 0.3f);
      line(0.9f, 0, 0.55f, -0.3f);    line(0.9f, 0, 0.55f, 0.3f);
      break;
    case ICON_SCRUB:    // dial with a pointer
      ring(0, 0, 0.9f);
      line(0, 0, 0.45f, -0.45f);
      cv.fillCircle(cx, cy, 1, c);
      break;
    case ICON_EDIT:     // scissors
      line(-0.7f, -0.85f, 0.4f, 0.3f);  line(0.7f, -0.85f, -0.4f, 0.3f);
      ring(-0.55f, 0.62f, 0.3f);        ring(0.55f, 0.62f, 0.3f);
      break;
    case ICON_TIMELINE: // clip bars with a playhead
      line(-0.9f, -0.5f, 0.1f, -0.5f);
      line(-0.4f, 0.0f, 0.9f, 0.0f);
      line(-0.9f, 0.5f, 0.3f, 0.5f);
      cv.drawLine(lroundf(X(0.55f)), lroundf(Y(-0.95f)), lroundf(X(0.55f)), lroundf(Y(0.95f)),
                  dim ? (uint16_t)TFT_DARKGREY : (uint16_t)TFT_WHITE);
      break;
    case ICON_GUITAR:   // guitar: body, sound hole, neck, headstock
      ring(0.3f, 0.35f, 0.55f);  ring(0.3f, 0.35f, 0.16f);
      line(-0.09f, -0.04f, -0.75f, -0.7f);
      line(-0.62f, -0.83f, -0.88f, -0.57f);
      break;
    case ICON_TRANSPORT: // play triangle
      line(-0.45f, -0.75f, -0.45f, 0.75f);
      line(-0.45f, 0.75f, 0.8f, 0.0f);
      line(0.8f, 0.0f, -0.45f, -0.75f);
      break;
    case ICON_TRACKS:   // track lanes with dots
      for (int i = -1; i <= 1; i++) {
        ring(-0.72f, i * 0.55f, 0.13f);
        line(-0.35f, i * 0.55f, 0.9f, i * 0.55f);
      }
      break;
    case ICON_THERMO:   // thermometer: stem, bulb, scale ticks
      line(-0.2f, -0.9f, -0.2f, 0.25f);  line(0.2f, -0.9f, 0.2f, 0.25f);
      line(-0.2f, -0.9f, 0.2f, -0.9f);
      ring(0.0f, 0.62f, 0.42f);
      line(0.0f, -0.5f, 0.0f, 0.3f);
      line(0.45f, -0.6f, 0.78f, -0.6f);  line(0.45f, -0.25f, 0.78f, -0.25f);
      break;
    case ICON_BACK:     // left arrow
      line(0.85f, 0, -0.85f, 0);
      line(-0.85f, 0, -0.2f, -0.6f);
      line(-0.85f, 0, -0.2f, 0.6f);
      break;
  }
}

// ---------------------------------------------------------------- carousel
struct Tile { const char *title; const char *subtitle; IconId icon; uint16_t color; };

static int tileCount() {
  if (scene.level == LV_HOME) return kProfileCount + 1;                 // apps + thermostat
  if (isThermo(scene.profile)) return THERMOSTAT_COUNT + 1;             // zones + Back
  return kProfiles[scene.profile].modeCount + 1;                        // modes + Back
}

static Tile tileAt(int i) {
  if (scene.level == LV_HOME && i == kProfileCount) {
    static char tsub[24];
    snprintf(tsub, sizeof(tsub), "%d zones", THERMOSTAT_COUNT);
    return { "THERMOSTAT", tsub, ICON_THERMO, kAppColor[3] };
  }
  if (scene.level == LV_MENU && isThermo(scene.profile)) {
    if (i >= THERMOSTAT_COUNT) return { "BACK", "Return to apps", ICON_BACK, TFT_LIGHTGREY };
    static char zsub[28];
    ThermoState t;
    if (netThermo(i, t)) snprintf(zsub, sizeof(zsub), "%.0f now, %s", t.cur, t.mode);
    else                 snprintf(zsub, sizeof(zsub), "%s", netStatusText());
    return { THERMOSTAT_NAMES[i], zsub, ICON_THERMO, kAppColor[3] };
  }
  if (scene.level == LV_HOME) {
    static char sub[24];
    snprintf(sub, sizeof(sub), "%d modes", kProfiles[i].modeCount);
    return { kProfiles[i].homeName ? kProfiles[i].homeName : kProfiles[i].name, sub, kProfiles[i].icon, kAppColor[i % 4] };
  }
  const AppProfile &a = kProfiles[scene.profile];
  if (i >= a.modeCount) return { "BACK", "Return to apps", ICON_BACK, TFT_LIGHTGREY };
  return { a.modes[i].name, a.modes[i].pressName, a.modes[i].icon, kAppColor[scene.profile % 4] };
}

static void drawCarousel(int slide) {
  const int n = tileCount();
  const uint16_t accent = (scene.level == LV_HOME) ? kAppColor[scene.index % 4]
                                                   : kAppColor[scene.profile % 4];
  cv.fillScreen(TFT_BLACK);
  cv.drawCircle(CX, CX, 118, accent);
  cv.drawCircle(CX, CX, 117, accent);

  // Header
  cv.setTextDatum(textdatum_t::top_center);
  cv.setFont(&fonts::Font2);
  cv.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  cv.drawString(scene.level == LV_HOME ? "SELECT APP"
                : isThermo(scene.profile) ? "THERMOSTAT" : kProfiles[scene.profile].name, CX, 22);

  // Position dots
  const int gap = 12, x0 = CX - (n - 1) * gap / 2;
  for (int i = 0; i < n; i++) {
    if (i == scene.index) cv.fillCircle(x0 + i * gap, 46, 3, accent);
    else                  cv.drawCircle(x0 + i * gap, 46, 3, TFT_DARKGREY);
  }

  // Tiles: centre one large, neighbours small and dim. They slide by `slide` pixels.
  for (int k = -2; k <= 2; k++) {
    int i = scene.index + k;
    if (i < 0 || i >= n) continue;
    Tile t = tileAt(i);
    int x = CX + k * TILE_W + slide;
    if (x < -30 || x > 270) continue;
    float closeness = 1.0f - fabsf((float)(x - CX)) / TILE_W;
    if (closeness < 0) closeness = 0;
    int r = 11 + (int)(21 * closeness);              // badge radius: 11 (side) .. 32 (centre)
    bool dim = closeness <= 0.6f;
    drawIcon(t.icon, x, 96, r, dim ? (uint16_t)TFT_DARKGREY : t.color, dim);
  }

  // Selected tile's labels
  Tile sel = tileAt(scene.index);
  cv.setTextColor(TFT_WHITE, TFT_BLACK);
  cv.setTextDatum(textdatum_t::middle_center);
  cv.setFont(&fonts::FreeSansBold12pt7b);
  cv.drawString(sel.title, CX, 160);
  cv.setFont(&fonts::Font2);
  cv.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  cv.drawString(sel.subtitle, CX, 186);

  // Hint
  cv.setFont(&fonts::Font0);
  cv.setTextColor(TFT_DARKGREY, TFT_BLACK);
  cv.drawString(scene.level == LV_HOME ? "turn: browse   press: open"
                                       : "turn: browse   press: select   hold: back", CX, 210);
}

// ---------------------------------------------------------------- active page
static void drawLabel(const char *s, int cx, int y) {
  String str(s);
  int sp = str.indexOf(' ');
  cv.setTextDatum(textdatum_t::top_center);
  cv.setFont(&fonts::Font0);
  cv.setTextSize(1);
  if (sp > 0) {
    cv.drawString(str.substring(0, sp), cx, y);
    cv.drawString(str.substring(sp + 1), cx, y + 10);
  } else {
    cv.drawString(str, cx, y + 5);
  }
}

static void drawActive() {
  const AppProfile &app = kProfiles[scene.profile];
  const ModeDef &m = app.modes[scene.mode];
  const uint16_t accent = kAppColor[scene.profile % 4];

  cv.fillScreen(TFT_BLACK);
  cv.drawCircle(CX, CX, 118, accent);
  cv.drawCircle(CX, CX, 117, accent);

  cv.setTextColor(accent, TFT_BLACK);
  cv.setTextDatum(textdatum_t::top_center);
  cv.setFont(&fonts::Font2);
  cv.drawString(app.name, CX, 22);

  const int n = app.modeCount, gap = 14, x0 = CX - (n - 1) * gap / 2;
  for (int i = 0; i < n; i++) {
    if (i == scene.mode) cv.fillCircle(x0 + i * gap, 50, 4, accent);
    else                 cv.drawCircle(x0 + i * gap, 50, 4, TFT_DARKGREY);
  }

  cv.setTextColor(TFT_WHITE, TFT_BLACK);
  cv.setTextDatum(textdatum_t::middle_center);
  cv.setFont(&fonts::FreeSansBold12pt7b);
  cv.drawString(m.name, CX, 88);

  cv.setFont(&fonts::Font2);
  cv.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String hint = m.altAxis ? String("Press: ") + (scene.axisVertical ? "axis = VERTICAL" : "axis = HORIZONTAL")
                          : String("Press: ") + m.pressName;
  cv.drawString(hint, CX, 122);

  cv.drawFastHLine(32, 150, 176, TFT_DARKGREY);
  cv.drawFastVLine(80, 150, 56, TFT_DARKGREY);
  cv.drawFastVLine(160, 150, 56, TFT_DARKGREY);
  cv.setTextColor(accent, TFT_BLACK);
  const int cxs[3] = { 56, 120, 184 };
  for (int i = 0; i < 3; i++) drawLabel(m.touchName[i], cxs[i], 168);
}

// ---------------------------------------------------------------- thermostat dial
struct RGB { uint8_t r, g, b; };
static RGB mixc(RGB a, RGB b, float t) {
  return { (uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.b + (b.b - a.b) * t) };
}
static RGB scaleC(RGB c, float k) {
  auto cl = [](float v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
  return { cl(c.r * k), cl(c.g * k), cl(c.b * k) };
}
static uint16_t c565(RGB c) { return cv.color565(c.r, c.g, c.b); }

// Background colours by state: warm while heating, cool blue while cooling, dark tinted by mode when idle.
static void thermoPalette(const ThermoState &s, bool valid, RGB &top, RGB &bot, bool &working) {
  working = false;
  top = {40, 42, 46}; bot = {12, 13, 15};
  if (!valid) return;
  if (!strcmp(s.action, "heating"))      { top = {246, 112, 40}; bot = {168, 48, 18}; working = true; }
  else if (!strcmp(s.action, "cooling")) { top = {52, 134, 255}; bot = {14, 50, 150}; working = true; }
  else if (!strcmp(s.preset, "eco"))     { top = {30, 68, 50};   bot = {9, 24, 18}; }
  else if (!strcmp(s.mode, "cool"))      { top = {30, 46, 70};   bot = {9, 14, 26}; }
  else if (!strcmp(s.mode, "heat"))      { top = {70, 42, 30};   bot = {26, 14, 10}; }
  else if (!strcmp(s.mode, "heat_cool")) { top = {46, 42, 66};   bot = {14, 12, 26}; }
}

// The dial is a 270-degree arc open at the bottom: 135 deg (bottom-left) clockwise to 405 deg (bottom-right).
static float dialAngle(float v, float mn, float mx) {
  return (135.0f + 270.0f * (v - mn) / (mx - mn)) * DEG_TO_RAD;
}
static void tickAt(float v, float mn, float mx, float rIn, float rOut, uint16_t col, float dAng = 0) {
  float a = dialAngle(v, mn, mx) + dAng, ca = cosf(a), sa = sinf(a);
  cv.drawLine(lroundf(CX + rIn * ca), lroundf(CX + rIn * sa), lroundf(CX + rOut * ca), lroundf(CX + rOut * sa), col);
}

static bool pendOn = false, pendEditLow = false;
static float pendTarget = NAN, pendLow = NAN, pendHigh = NAN;
static char noticeMsg[32] = "";
static uint32_t noticeUntil = 0;

void displayThermoPending(bool pending, float target, float low, float high, bool editLow) {
  pendOn = pending; pendTarget = target; pendLow = low; pendHigh = high; pendEditLow = editLow;
}
void displayNotice(const char *msg, uint32_t ms) {
  strlcpy(noticeMsg, msg, sizeof(noticeMsg));
  noticeUntil = millis() + ms;
}

static void drawThermo() {
  ThermoState s;
  bool valid = netThermo(scene.mode, s);
  if (valid && pendOn) {                         // show what the knob has set, before Home Assistant confirms
    if (!isnan(pendTarget)) s.target = pendTarget;
    if (!isnan(pendLow))    s.low = pendLow;
    if (!isnan(pendHigh))   s.high = pendHigh;
  }
  RGB top, bot; bool working;
  thermoPalette(s, valid, top, bot, working);

  // Background gradient, with a slow "breathing" while the system is working.
  float breathe = working ? 1.0f + 0.07f * sinf(millis() / 550.0f) : 1.0f;
  for (int y = 0; y < 240; y++) cv.drawFastHLine(0, y, 240, c565(scaleC(mixc(top, bot, y / 239.0f), breathe)));
  const RGB mid = scaleC(mixc(top, bot, 0.5f), breathe);
  const RGB WHITE = {255, 255, 255};

  const bool range = valid && !strcmp(s.mode, "heat_cool");
  const bool off   = valid && !strcmp(s.mode, "off");
  float mn = valid ? s.minT : 50, mx = valid ? s.maxT : 90;
  if (mx <= mn) { mn = 50; mx = 90; }

  // Ticks every half degree. Those between the current and target temperature are brighter.
  float lo = 0, hi = -1;
  if (valid && !off) {
    if (range && !isnan(s.low) && !isnan(s.high)) { lo = s.low; hi = s.high; }
    else if (!isnan(s.cur) && !isnan(s.target))   { lo = fminf(s.cur, s.target); hi = fmaxf(s.cur, s.target); }
  }
  for (float v = mn; v <= mx + 0.01f; v += 0.5f) {
    bool between = v >= lo && v <= hi;
    tickAt(v, mn, mx, 106, 114, c565(mixc(mid, WHITE, between ? 0.8f : 0.26f)));
  }
  if (valid && !isnan(s.cur)) tickAt(constrain(s.cur, mn, mx), mn, mx, 98, 115, c565(WHITE));          // current
  if (valid && !off) {                                                                                  // target(s)
    float tg[2] = { range ? s.low : s.target, range ? s.high : NAN };
    for (int h = 0; h < 2; h++) {
      float t = tg[h];
      if (isnan(t)) continue;
      bool sel = range && ((h == 0) == pendEditLow);          // the heat/cool handle being edited is longer and bolder
      int kk = sel ? 2 : 1;
      for (int k = -kk; k <= kk; k++) tickAt(constrain(t, mn, mx), mn, mx, sel ? 86 : 92, 118, c565(WHITE), k * 0.011f);
    }
  }

  // Thermostat name
  cv.setTextDatum(textdatum_t::middle_center);
  cv.setFont(&fonts::DejaVu12);
  cv.setTextColor(c565(mixc(mid, WHITE, 0.8f)));
  String nm = THERMOSTAT_NAMES[scene.mode]; nm.toUpperCase();
  cv.drawString(nm, CX, 52);

  if (!valid) {                                            // nothing received yet
    cv.setFont(&fonts::DejaVu18);
    cv.setTextColor(c565(mixc(mid, WHITE, 0.7f)));
    cv.drawString(netStatusText(), CX, 118);
    return;
  }

  // Target temperature, large. Heat/cool shows "low-high" in a smaller size.
  char num[12];
  if (off)        snprintf(num, sizeof(num), "--");
  else if (range) snprintf(num, sizeof(num), "%ld-%ld", lroundf(s.low), lroundf(s.high));
  else            snprintf(num, sizeof(num), "%ld", lroundf(s.target));
  cv.setFont(range ? (const lgfx::IFont *)&fonts::DejaVu40 : (const lgfx::IFont *)&fonts::DejaVu72);
  cv.setTextColor(TFT_WHITE);
  cv.drawString(num, CX - 4, 114);
  if (!off) {                                              // degree ring
    int w = cv.textWidth(num), dx = CX - 4 + w / 2 + 9, dy = range ? 96 : 84;
    cv.drawCircle(dx, dy, 4, TFT_WHITE);
    cv.drawCircle(dx, dy, 3, TFT_WHITE);
  }

  // Status line
  char st[24];
  if      (!strcmp(s.action, "heating")) snprintf(st, sizeof(st), "Heating to %ld", lroundf(s.target));
  else if (!strcmp(s.action, "cooling")) snprintf(st, sizeof(st), "Cooling to %ld", lroundf(s.target));
  else if (off)                          snprintf(st, sizeof(st), "OFF");
  else if (!strcmp(s.preset, "eco"))     snprintf(st, sizeof(st), "ECO");
  else if (range)                        snprintf(st, sizeof(st), "HEAT / COOL");
  else { snprintf(st, sizeof(st), "%s", s.mode); for (char *c = st; *c; c++) *c = toupper(*c); }
  cv.setFont(&fonts::DejaVu18);
  cv.setTextColor(c565(mixc(mid, WHITE, 0.92f)));
  cv.drawString(st, CX, 166);

  // Current temperature, labelled beside its tick
  if (!isnan(s.cur)) {
    float a = dialAngle(constrain(s.cur, mn, mx), mn, mx);
    char cn[8]; snprintf(cn, sizeof(cn), "%ld", lroundf(s.cur));
    cv.setFont(&fonts::DejaVu18);
    cv.setTextColor(TFT_WHITE);
    cv.drawString(cn, lroundf(CX + 80 * cosf(a)), lroundf(CX + 80 * sinf(a)));
  }

  // Notice (if any) replaces the humidity line for a moment
  bool notice = millis() < noticeUntil;
  if (notice) {
    cv.setFont(&fonts::DejaVu12);
    cv.setTextColor(TFT_WHITE);
    cv.drawString(noticeMsg, CX, 202);
  } else if (s.humidity >= 0) {   // Humidity, in the gap at the bottom of the dial
    char hu[20]; snprintf(hu, sizeof(hu), "%d%% humidity", s.humidity);
    cv.setFont(&fonts::DejaVu12);
    cv.setTextColor(c565(mixc(mid, WHITE, 0.6f)));
    cv.drawString(hu, CX, 202);
  }

  cv.setFont(&fonts::Font0);
  cv.setTextColor(c565(mixc(mid, WHITE, 0.35f)));
  cv.drawString(range ? "turn: set   press: mode   tap: low/high" : "turn: set   press: mode", CX, 222);
}

// ---------------------------------------------------------------- public API
void displayBegin() {
  // The panel and backlight sit behind switched rails; power them before init.
  pinMode(LCD_PWR_EN1, OUTPUT);
  pinMode(LCD_PWR_EN2, OUTPUT);
  digitalWrite(LCD_PWR_EN1, HIGH);
  digitalWrite(LCD_PWR_EN2, HIGH);
  delay(50);   // let the rails settle (setup-time only)
  tft.init();
  tft.setBrightness(200);
  tft.fillScreen(TFT_BLACK);
  cv.setColorDepth(16);
  cv.createSprite(240, 240);
}

void displaySetScene(const Scene &s, int dir) {
  scene = s;
  animDir = (s.level == LV_ACTIVE) ? 0 : dir;
  animStart = millis();
  dirty = true;
}

void displayLoop() {
  // The thermostat dial animates and follows live data, so redraw it on a timer (~15 fps).
  static uint32_t nextDial = 0;
  const bool dialUp = (scene.level == LV_ACTIVE && isThermo(scene.profile));
  if (dialUp && millis() >= nextDial) { dirty = true; nextDial = millis() + 66; }

  if (!dirty) return;

  int slide = 0;
  if (animDir != 0) {
    float t = (float)(millis() - animStart) / ANIM_MS;
    if (t >= 1.0f) { animDir = 0; }
    else { float e = 1.0f - (1.0f - t) * (1.0f - t);          // ease-out
           slide = (int)((1.0f - e) * TILE_W) * (animDir > 0 ? 1 : -1); }
  }

  if (scene.level == LV_ACTIVE) { if (dialUp) drawThermo(); else drawActive(); }
  else                          drawCarousel(slide);

  tft.startWrite();
  cv.pushSprite(0, 0);
  tft.endWrite();

  if (animDir == 0) dirty = false;   // keep redrawing only while sliding
}

void displayToast(const char *msg) {
  tft.startWrite();
  tft.fillRect(40, 208, 160, 18, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(textdatum_t::top_center);
  tft.setFont(&fonts::Font2);
  tft.drawString(msg, CX, 210);
  tft.endWrite();
}
