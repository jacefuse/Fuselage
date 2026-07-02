#pragma once

// Fuselage - Free Unrestricted Software Enabling Layered Asset Game Environments
// Top-level orchestrator. Owns the engine lifecycle and coordinates subsystems.
// Game logic lives entirely inside the while(fuselage()) loop.

#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#endif
// Colors: Color type, named colors (BLANK/BLACK/WHITE/etc.), full 256-color palette
// macros (GDMF_COLOR0-GDMF_COLOR255, C64_*, ZX_*, ANSI_*, NES_*), and palette helpers
// (GetColorFromChar, GetCommodoreColor, GetTandyColor, GetANSIColor, etc.).
//#include "GDMF/colors.h"
//#include "GDMF/gdmf_textlayer.h"

#define FUSELAGE_VERSION "0.2.2026070101 BUTTOCKS"

#include "GDMF/gdmf.h"
#include "CAKE/cake.h"
//#include "CAKE/cake_help.h"

// Signals
// FuselageSignal() is the only way game logic communicates intent to the engine.
// The engine processes signals on the next fuselage() tick.

typedef enum {
    FUSELAGE_QUIT,
    FUSELAGE_WINDOWED,
    FUSELAGE_BORDERLESS,
    FUSELAGE_FULLSCREEN_EXCLUSIVE,
} FuselageSignalType;

void FuselageSignal(FuselageSignalType signal);

// Pre-init configuration
// Call any of these before the first fuselage() call.
// After that they have no effect.
// Resolution Changes in the future TBD

void FuselageSetTitle(const char* title);
void FuselageSetResolution(int width, int height);
void FuselageSetAspectRatio(int num, int den);

// Sets the taskbar/title-bar icon from a top-down RGBA8 buffer
// (width*height*4 bytes). Copies the data; caller retains ownership.
void FuselageSetWindowIcon(int width, int height, const unsigned char* rgba);

// Mouse capture / cursor visibility
// Callable any time, e.g. from a keypress. Capture confines the OS cursor to
// the window's client area; cursor visibility shows/hides it. Both are
// automatically suspended while the window is unfocused and reinstated on
// refocus, so neither can leave the user's mouse stuck.

void FuselageSetMouseCapture(bool capture);
bool FuselageGetMouseCapture(void);
bool FuselageToggleMouseCapture(void);

void FuselageSetCursorVisible(bool visible);
bool FuselageGetCursorVisible(void);

// Main loop
// Initializes on first call. Returns false when shutdown is complete.

bool fuselage(void);

// Provisional FPS measurement -- see GDMFGetCurrentFPS()'s doc comment in
// GDMF/gdmf.h. Tracking, throttling, and reporting of frame rate are all
// expected to move to DICE once it exists; this is a stopgap that exists
// only out of necessity, given the engine's current lack of one.
float FuselageGetCurrentFPS(void);
