#ifndef FUSELAGE_H
#define FUSELAGE_H

// Fuselage - Free Unrestricted Software Enabling Layered Asset Game Environments
// Top-level orchestrator. Owns the engine lifecycle and coordinates subsystems.
// Game logic lives in a function conventionally named game() (see
// FuselageSetTickCallback), registered before the while(fuselage()) loop
// starts. game() runs on Fuselage's own dedicated sim thread at a fixed
// rate; the while(fuselage()) loop itself renders, independently, as fast
// as vsync/hardware allows.

#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#endif
// Colors: Color type, named colors (BLANK/BLACK/WHITE/etc.), full 256-color palette
// macros (GDMF_COLOR0-GDMF_COLOR255, C64_*, ZX_*, ANSI_*, NES_*), and palette helpers
// (GetColorsColorFromChar, GetColorsCommodoreColor, GetColorsTandyColor, GetColorsANSIColor, etc.).
//#include "GDMF/gdmf_colors.h"
//#include "GDMF/textlayer/gdmf_textlayer.h"

#define FUSELAGE_VERSION "0.4.2026071503 DERRIERE"

#include "GDMF/gdmf.h"
// GDMF subsystem helpers -- relative (delta) conveniences over the absolute
// sprite/tile APIs. Pulled in here (like cake_help below) rather than by each
// core header, so the cores stay absolute-only and helper-unaware.
#include "GDMF/sprites/gdmf_sprites_help.h"
#include "GDMF/tiles/gdmf_tiles_help.h"
#include "CAKE/cake.h"
#include "CAKE/cake_help.h"
#include "DICE/dice.h"

// Signals
// FuselageSignal() is how game logic requests engine-level state transitions
// that must be applied at a safe point in the frame rather than executed
// immediately on the caller's thread. The engine queues them and processes
// them on the next fuselage() tick, on the main/render thread. Currently:
// quit, display-mode changes, and applying a new target FPS (see
// FuselageSetTargetFPS).
//
// Other runtime settings (mouse capture, cursor visibility, vsync, input-focus
// gating) are applied directly through their own Fuselage* calls below.

typedef enum {
    FUSELAGE_QUIT,
    FUSELAGE_WINDOWED,
    FUSELAGE_BORDERLESS,
    FUSELAGE_FULLSCREEN_EXCLUSIVE,
    FUSELAGE_APPLY_TARGET_FPS,
} FuselageSignalType;

void FuselageSignal(FuselageSignalType signal);

// Pre-init configuration
// Call any of these before the first fuselage() call.
// After that they have no effect.
// Resolution Changes in the future TBD

void FuselageSetTitle(const char* title);
void FuselageSetResolution(int width, int height);
// Design-resolution canvas, decoupled from the window size (see
// GDMF_SetCanvasResolution): author in a small fixed canvas while opening
// the window at an integer multiple of it.
void FuselageSetCanvasResolution(int width, int height);
void FuselageSetAspectRatio(int num, int den);

// Query the engine's dimensions. Unlike the Set* config calls above, these are
// callable at any time and return live values. Any out-parameter may be NULL.
//
// Canvas -- the fixed logical "virtual machine" resolution game code authors
// against; sprite/tile/text coordinates all live in this space. Defaults to the
// 1280x720 reference resolution and is meant to stay fixed for the life of the
// program; only change it (FuselageSetCanvasResolution, pre-init) if you
// deliberately want to author in a different fixed space. This is almost always
// the size you want for layout.
void FuselageGetCanvasResolution(int* width, int* height);

// Window -- the actual on-screen client size. This is the user's to change by
// resizing; only the aspect ratio is held fixed (see FuselageGetAspectRatio).
// Updates live as the window is resized. Prefer the canvas resolution for
// game-space layout; use this only for genuinely window-relative work.
void FuselageGetResolution(int* width, int* height);

// The locked display aspect ratio, as numerator/denominator (e.g. 16 / 9).
void FuselageGetAspectRatio(int* num, int* den);

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

// When enabled, Fuselage automatically calls CAKE_Silence()/CAKE_Resume()
// (see CAKE/cake.h) as the window loses/regains focus (GDMF_IsFocused()),
// checked once per sim tick alongside the existing CAKE_Poll() call.
// Keyboard/mouse input stops being processed at all while unfocused --
// the classic "still holding a key when I alt-tabbed" bug becomes
// impossible rather than something game code has to guard against.
// Controllers are unaffected (see CAKE_Silence's own comment for why).
// On by default: keyboard/mouse input is gated on window focus unless you
// explicitly turn it off with FuselageInputRequiresFocus(false). Applies
// immediately regardless of the window's current focus state, whether called
// before or after fuselage_init() -- no need to wait for a focus change event
// to take effect. Calling CAKE_Silence/CAKE_Resume yourself still works
// regardless of this setting; the two are independent, this is just automation
// on top.
void FuselageInputRequiresFocus(bool enabled);
bool FuselageGetInputRequiresFocus(void);

// VSync -- see GDMF_SetVSync's doc comment in GDMF/gdmf.h. Callable any time;
// takes effect on the next swapchain recreation. Defaults to enabled.
void FuselageSetVSync(bool enabled);
bool FuselageGetVSync(void);

// Sim tick rate -- ticks/sec for the callback registered via
// FuselageSetTickCallback() (see below). Defaults to 60. Call before the
// first fuselage() call; no effect after (a design-time choice for your
// game, not something to retune mid-run -- the sim thread is created once,
// at that rate, during init). Game logic that scales movement by real
// elapsed time (not a fixed amount per tick) is unaffected by whatever
// this is set to; logic using fixed per-tick deltas (the norm in this
// engine's own examples today) runs proportionally faster/slower in real
// time depending on this rate, so know what your tick rate means to your
// own game logic before choosing one other than the default.
void   FuselageSetSimRate(double hz);
double FuselageGetSimRate(void);

// Target FPS cap for rendering -- independent of vsync (see
// FuselageSetVSync above). 0 (the default) means uncapped: vsync alone
// paces render if it's on, or nothing paces it if it's off. A positive
// value caps render to that rate regardless of vsync -- useful either to
// go below the display's real refresh (e.g. capping to 60fps on a 144Hz
// panel) or, combined with vsync off, to target a rate the display itself
// doesn't support. If vsync is on and the display's real refresh is lower
// than the target, the display wins -- this can only lower the achieved
// rate, never raise it above what vsync/hardware allow.
//
// This is a setter only: it records the desired cap but does not change render
// pacing on its own. Before the first fuselage() call the value is applied
// automatically during init. After init, apply a change by sending
// FuselageSignal(FUSELAGE_APPLY_TARGET_FPS), which the engine processes on the
// render thread on the next tick -- keeping the target-FPS DICE timer's
// lifecycle off game()'s thread. FuselageGetTargetFPS() returns the value last
// set (which becomes the active cap once applied).
void   FuselageSetTargetFPS(double fps);
double FuselageGetTargetFPS(void);

// Game tick callback
// Runs at the configured sim rate (see FuselageSetSimRate() above) on
// Fuselage's own dedicated sim thread --
// NOT the thread that calls fuselage(). Register once before entering the
// while(fuselage()) loop. CAKE_Keys/CAKE_Mouse* reads and GDMF Set*/Get*
// calls (sprites, tiles, etc.) inside the callback are automatically
// synchronized against fuselage()'s rendering; nothing else needs to take a
// lock. Convention: name the function game() -- e.g.
//   static void game(void) { ... }
//   FuselageSetTickCallback(game);
// This is also the slot the future VPU will call into: VPU's job will be
// running whatever script/bytecode defines game(), same signature, same
// registration call.
typedef void (*FuselageTickCallback)(void);
void FuselageSetTickCallback(FuselageTickCallback callback);

// Blocks the calling thread until Fuselage's own frame-pacing clock reaches
// its next interval. Unlike a raw display vblank wait, this is NOT tied to
// the monitor's actual refresh rate -- DICE has final say, so calling this
// on a 144Hz/240Hz display still only returns at Fuselage's configured
// pacing rate rather than 2-4x more often. For game code that wants a
// classic "wait for the next frame" primitive independent of both game()
// and however fast rendering happens to be running.
void FuselageWaitFrame(void);

// Main loop
// Initializes on first call. Returns false when shutdown is complete.

bool fuselage(void);

// Current render frame rate, measured by GDMF over a rolling window (see
// GDMF_GetCurrentFPS()). Intended for HUD/debug display. FPS reporting lives in
// Fuselage/GDMF, deliberately not in DICE -- DICE is a standalone timer with no
// awareness of rendering or any other subsystem.
float FuselageGetCurrentFPS(void);

#endif // FUSELAGE_H
