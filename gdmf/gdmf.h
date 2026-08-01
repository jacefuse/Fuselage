#ifndef GDMF_H
#define GDMF_H

// GDMF - Graphic Device Minimalist Framework
// Window management for Fuselage on Windows (other platforms eventually).
// GDMF is self-contained within the graphics subsystem. It has no knowledge
// of CAKE, DICE, SHARP, or the top-level Fuselage orchestrator.

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include "gdmf_colors.h"
#include "gdmf_textlayer.h"
#include "gdmf_sprites.h"
#include "gdmf_tiles.h"
#include "gdmf_pixies.h"
#include "gdmf_interactions.h"

#define GDMF_VERSION "0.3.2026070601 COLON"

// Thread safety
// Unless a function's own comment says otherwise, GDMF's public calls are safe
// from any thread: window/display operations post to GDMF's own window thread,
// and the state queries read values that change atomically. The exceptions,
// each also flagged at its own declaration below:
//   * GDMF_SetVSync touches Vulkan state directly, so call it from the render
//     thread (the one that calls GDMF_SubmitFrame) -- not the window/sim thread.
//   * GDMF_PrepareFrame / GDMF_SubmitFrame are the render path: PrepareFrame
//     reads live game state and should run under the game-state lock, while
//     SubmitFrame does the vsync-blocking present and must NOT (see their own
//     comment below).
//   * Lifecycle -- GDMF_Init / GDMF_Shutdown / GDMF_Tick -- runs on the thread
//     that drives the main loop.

// Display modes
typedef enum {
    GDMF_MODE_WINDOWED,
    GDMF_MODE_BORDERLESS,
    GDMF_MODE_FULLSCREEN_EXCLUSIVE,
} GDMF_DisplayMode;

// Window configuration - set before GDMF_Init()
void GDMF_SetTitle(const char* title);
void GDMF_SetResolution(int width, int height);
void GDMF_SetAspectRatio(int num, int den);  // e.g. 16, 9

// Sets the taskbar/title-bar icon from a top-down RGBA8 buffer
// (width*height*4 bytes). Copies the data; caller retains ownership.
// Applied once the window is created -- call before GDMF_Init().
void GDMF_SetWindowIcon(int width, int height, const unsigned char* rgba);

// Lifecycle
int  GDMF_Init(void);
void GDMF_Shutdown(void);

// Called once per fuselage tick
// Returns false if the window has been closed
bool GDMF_Tick(void);

// Split for callers that need to synchronize against live game state
// (sprites/tiles/pixies/text layer) without holding that synchronization
// across a vsync-blocking wait. GDMF_PrepareFrame() is the only half that
// reads any of it -- safe to run under a lock shared with game logic.
// GDMF_SubmitFrame() only touches per-image GPU buffers GDMF_PrepareFrame()
// already filled, plus the GPU queue/present, and is where the
// vsync-blocking wait lives -- must never run under that same lock.
// GDMF_SubmitFrame() is a no-op if GDMF_PrepareFrame() had nothing to do this
// tick (minimized, mid swapchain-recreate, etc.).
void GDMF_PrepareFrame(void);
void GDMF_SubmitFrame(void);

// Current render frame rate: counts frames actually submitted over a rolling
// ~0.5s window and returns the most recently completed window's rate. For
// HUD/debug display. FPS reporting lives here in GDMF (and is surfaced by
// FuselageGetCurrentFPS); it is deliberately not a DICE concern -- DICE is a
// standalone timer with no awareness of rendering or any other subsystem.
float GDMF_GetCurrentFPS(void);

// Display mode switching - safe to call from any thread
void GDMF_SetDisplayMode(GDMF_DisplayMode mode);
GDMF_DisplayMode GDMF_GetDisplayMode(void);

// VSync: FIFO present mode (locked to display refresh, no tearing) when
// enabled; IMMEDIATE (uncapped, may tear) when disabled and the surface
// actually supports it -- falls back to FIFO otherwise, since FIFO is the
// only present mode Vulkan guarantees every surface supports. Takes effect
// on the next swapchain recreation, triggered immediately when this
// actually changes the setting. Defaults to enabled. Call from the game/
// render thread -- unlike GDMF_SetDisplayMode, this touches Vulkan state
// directly rather than posting to the window thread.
void GDMF_SetVSync(bool enabled);
bool GDMF_GetVSync(void);

// Window state queries
int  GDMF_GetWidth(void);
int  GDMF_GetHeight(void);
bool GDMF_IsMinimized(void);
bool GDMF_ResizeOccurred(void);   // true once after each resize; clears itself
HWND GDMF_GetHWND(void);          // needed later for Vulkan surface creation

// True while the window has keyboard focus (updated on WM_SETFOCUS/
// WM_KILLFOCUS -- the same signal mouse capture/cursor visibility already
// key off of). Safe to call from any thread. See FuselageInputRequiresFocus
// (fuselage.h) for automatically silencing CAKE's keyboard/mouse input
// while this is false.
bool GDMF_IsFocused(void);

// Design aspect ratio set via GDMF_SetAspectRatio -- needed by the Vulkan
// layer to letterbox/pillarbox the render viewport when the live window or
// monitor doesn't match it (see gdmf_get_render_viewport_rect()).
int GDMF_GetAspectRatioNum(void);
int GDMF_GetAspectRatioDen(void);

// The design-resolution canvas -- the fixed logical pixel space every
// sprite/tile/pixie coordinate is expressed in, and the system's native
// resolution. Defaults to 1280x720 and stays fixed regardless of window size;
// the rendered canvas is scaled (aspect-locked, letterboxed) to fill the
// window. Like a pre-2000s console's native display, changing it is the
// exception -- call GDMF_SetCanvasResolution (pre-init) only when you
// deliberately want to author in a different fixed space (e.g. 256x192).
void GDMF_SetCanvasResolution(int width, int height);
int GDMF_GetCanvasWidth(void);
int GDMF_GetCanvasHeight(void);

// Mouse capture: confines the OS cursor to the window's client area while
// the window has focus. Automatically released while unfocused (so the
// user is never trapped after alt-tabbing away) and reapplied on refocus
// if still desired. Safe to call from any thread.
void GDMF_SetMouseCapture(bool capture);
bool GDMF_GetMouseCapture(void);
bool GDMF_ToggleMouseCapture(void);

// Cursor visibility: shows/hides the OS cursor while the window has focus
// (always shown while unfocused, same reasoning as mouse capture above).
// Independent of mouse capture -- either can be on without the other.
void GDMF_SetCursorVisible(bool visible);
bool GDMF_GetCursorVisible(void);

// Absolute mouse position, converted into the same reference-canvas space
// (1280x720) that sprite/tile coordinates already use (see
// SetSpritePosition/PlaceTile) -- not raw window pixels, so it's directly
// comparable to where sprites/tiles are placed, no manual letterbox/scale
// math needed on the caller's end. This is unrelated to CAKE's mouse deltas
// (CAKE_MouseX/Y): those are a running sum of raw, pre-acceleration device
// motion from raw input, good for camera-look style relative controls; this
// is the real current OS cursor position, good for pointing at something on
// screen. The two can be used side by side with no conflict. Coordinates
// can land outside 0..1280 / 0..720 if the cursor is over the letterbox
// bars or outside the window entirely -- range-check yourself if you need
// "is this actually over the canvas." Safe to call from any thread. Windows
// only for now -- GDMF has no Mac/Linux windowing backend yet (see
// gdmf_surface_win32.c), so there's nowhere else to implement this against.
void GDMF_GetMousePosition(float* x, float* y);

#endif // GDMF_H