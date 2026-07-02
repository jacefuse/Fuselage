#pragma once

// GDMF - Graphic Device Minimalist Framework
// Window management for Fuselage on Windows (other platforms eventually).
// GDMF is self-contained within the graphics subsystem. It has no knowledge
// of CAKE, DICE, SHARP, or the top-level Fuselage orchestrator.

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include "colors.h"
#include "gdmf_textlayer.h"
#include "gdmf_sprites.h"
#include "gdmf_tiles.h"
#include "gdmf_pixies.h"

#define GDMF_VERSION "0.2.2026070101 BUTTOCKS"

// Display modes
typedef enum {
    GDMF_MODE_WINDOWED,
    GDMF_MODE_BORDERLESS,
    GDMF_MODE_FULLSCREEN_EXCLUSIVE,
} GDMFDisplayMode;

// Window configuration - set before GDMFinit()
void GDMFsetTitle(const char* title);
void GDMFsetResolution(int width, int height);
void GDMFsetAspectRatio(int num, int den);  // e.g. 16, 9

// Sets the taskbar/title-bar icon from a top-down RGBA8 buffer
// (width*height*4 bytes). Copies the data; caller retains ownership.
// Applied once the window is created -- call before GDMFinit().
void GDMFsetWindowIcon(int width, int height, const unsigned char* rgba);

// Lifecycle
int  GDMFinit(void);
void GDMFshutdown(void);

// Called once per fuselage tick
// Returns false if the window has been closed
bool GDMFtick(void);

// Submit one rendered frame to the display
void GDMFrenderFrame(void);

// Provisional FPS measurement: counts frames actually submitted via
// GDMFrenderFrame() over a rolling ~0.5s window and returns the most
// recently completed window's rate. This is a stopgap, not a stable
// contract -- DICE is the intended long-term owner of both frame pacing
// (fuselage() currently just calls Sleep(1) after every frame, with no
// real throttling) and FPS reporting. How FPS is tracked, how/whether it's
// throttled, and how it's exposed to game code are all expected to change
// once DICE exists. This exists now only so game code has *something* to
// read for HUD/debug display in the meantime.
float GDMFGetCurrentFPS(void);

// Display mode switching - safe to call from any thread
void GDMFsetDisplayMode(GDMFDisplayMode mode);
GDMFDisplayMode GDMFgetDisplayMode(void);

// Window state queries
int  GDMFgetWidth(void);
int  GDMFgetHeight(void);
bool GDMFisMinimized(void);
bool GDMFresizeOccurred(void);   // true once after each resize; clears itself
HWND GDMFgetHWND(void);          // needed later for Vulkan surface creation

// Design aspect ratio set via GDMFsetAspectRatio -- needed by the Vulkan
// layer to letterbox/pillarbox the render viewport when the live window or
// monitor doesn't match it (see gdmf_get_render_viewport_rect()).
int GDMFgetAspectRatioNum(void);
int GDMFgetAspectRatioDen(void);

// Mouse capture: confines the OS cursor to the window's client area while
// the window has focus. Automatically released while unfocused (so the
// user is never trapped after alt-tabbing away) and reapplied on refocus
// if still desired. Safe to call from any thread.
void GDMFSetMouseCapture(bool capture);
bool GDMFGetMouseCapture(void);
bool GDMFToggleMouseCapture(void);

// Cursor visibility: shows/hides the OS cursor while the window has focus
// (always shown while unfocused, same reasoning as mouse capture above).
// Independent of mouse capture -- either can be on without the other.
void GDMFSetCursorVisible(bool visible);
bool GDMFGetCursorVisible(void);