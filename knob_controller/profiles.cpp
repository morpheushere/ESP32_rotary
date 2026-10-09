#include "profiles.h"

static const Chord NONE = {0, 0};

// ---------------- Figma ----------------
static const ModeDef kFigma[] = {
  { "CANVAS ZOOM",
    {M_CMD, '='},               {M_CMD, '-'},        // zoom in / out
    NONE, NONE, false,
    {M_SHIFT, '1'}, "Zoom to fit",                   // Shift+1
    { {M_CMD | M_OPT, 'k'}, {M_SHIFT, 'a'}, {0, 'f'} },
    { "Create component", "Add auto layout", "Frame tool" }, ICON_ZOOM },

  { "FRAMES",
    {0, 'n'},                   {M_SHIFT, 'n'},      // next / previous frame (verify)
    NONE, NONE, false,
    {M_SHIFT, '2'}, "Zoom to selection",             // Shift+2
    { {M_CMD | M_OPT, 'k'}, {M_SHIFT, 'a'}, {0, 'f'} },
    { "Create component", "Add auto layout", "Frame tool" }, ICON_FRAMES },

  { "NUDGE",
    {0, KEY_RIGHT_ARROW},       {0, KEY_LEFT_ARROW}, // horizontal
    {0, KEY_DOWN_ARROW},        {0, KEY_UP_ARROW},   // vertical (axis toggled)
    true,
    NONE, "Toggle X/Y axis",
    { {M_SHIFT, KEY_RIGHT_ARROW}, {M_CMD, 'd'}, {M_CMD, 'z'} },
    { "Nudge +10", "Duplicate", "Undo" }, ICON_NUDGE },
};

// ---------------- iMovie ----------------
static const ModeDef kIMovie[] = {
  { "SCRUB",
    {0, KEY_RIGHT_ARROW},       {0, KEY_LEFT_ARROW}, // one frame
    NONE, NONE, false,
    {0, ' '}, "Play/Pause",
    { {0, 'j'}, {0, 'k'}, {0, 'l'} },                // reverse / pause / forward
    { "Reverse (J)", "Pause (K)", "Forward (L)" }, ICON_SCRUB },

  { "EDIT",
    {0, KEY_RIGHT_ARROW},       {0, KEY_LEFT_ARROW},
    NONE, NONE, false,
    {M_CMD, 'b'}, "Split clip",
    { {M_CMD, 'z'}, {M_CMD | M_SHIFT, 'z'}, {0, KEY_BACKSPACE} },
    { "Undo", "Redo", "Delete clip" }, ICON_EDIT },

  { "TIMELINE ZOOM",
    {M_CMD, '='},               {M_CMD, '-'},
    NONE, NONE, false,
    {M_SHIFT, 'z'}, "Fit timeline",
    { {0, 's'}, {M_SHIFT, 's'}, {0, 'f'} },
    { "Skimming", "Audio skimming", "Favorite" }, ICON_TIMELINE },
};

// ---------------- GarageBand ----------------
static const ModeDef kGarageBand[] = {
  { "TRANSPORT",
    {0, KEY_RIGHT_ARROW},       {0, KEY_LEFT_ARROW},   // move playhead (verify step size)
    NONE, NONE, false,
    {0, ' '}, "Play/Pause",
    { {0, 'r'}, {0, 'c'}, {0, 'k'} },                  // record / cycle / metronome
    { "Record", "Cycle", "Metronome" }, ICON_TRANSPORT },

  { "TRACKS",
    {0, KEY_DOWN_ARROW},        {0, KEY_UP_ARROW},     // next / previous track
    NONE, NONE, false,
    {0, 'm'}, "Mute track",
    { {0, 's'}, {M_CMD | M_OPT, 'n'}, {M_CMD, 'd'} },  // solo / new track / duplicate
    { "Solo", "New track", "Duplicate" }, ICON_TRACKS },

  { "EDIT & ZOOM",
    {M_CMD, KEY_RIGHT_ARROW},   {M_CMD, KEY_LEFT_ARROW}, // zoom in / out horizontally
    NONE, NONE, false,
    {M_CMD, 't'}, "Split at playhead",
    { {M_CMD, 'z'}, {M_CMD | M_SHIFT, 'z'}, {0, KEY_BACKSPACE} },
    { "Undo", "Redo", "Delete" }, ICON_EDIT },
};

const AppProfile kProfiles[] = {
  { "FIGMA",  kFigma,  sizeof(kFigma)  / sizeof(kFigma[0]),  ICON_FIGMA, "SARIT'S FIGMA" },
  { "IMOVIE", kIMovie, sizeof(kIMovie) / sizeof(kIMovie[0]), ICON_IMOVIE },
  { "GARAGEBAND", kGarageBand, sizeof(kGarageBand) / sizeof(kGarageBand[0]), ICON_GUITAR },
};
const uint8_t kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);
