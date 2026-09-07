#ifndef GDMF_WINDOW_H
#define GDMF_WINDOW_H

// GDMF internal -- the platform window backend interface.
// gdmf.c owns the platform-neutral window state and public API; one backend
// file per platform (gdmf_window_win32.c, gdmf_window_macos.c,
// gdmf_window_wayland.c) owns the OS window itself and implements the
// gdmf_window_* functions below. Adding a platform means writing one new
// backend file and pointing the Makefile at it -- same rule as the Vulkan
// surface files (see gdmf_surface_win32.c).
//
// Threading contract:
//   * Win32 spawns a dedicated window thread (OS modal loops -- resize drag,
//     move -- must never stall the render loop, and Win32 allows a window on
//     any thread). gdmf_window_pump() is a no-op there.
//   * macOS requires the window on the process's MAIN thread, so the backend
//     creates it inside gdmf_window_create() (GDMF_Init already runs on the
//     main thread) and relies on gdmf_window_pump() being called once per
//     GDMF_Tick -- also the main thread -- to service pending events. The
//     sim thread never stalls either way; only rendering pauses during a
//     live-resize drag on macOS.
//   * Wayland creates the window on the main thread (like macOS) but then
//     runs a dedicated event READER thread (like Win32's window thread, for
//     a Wayland-only reason: a compositor disconnects a client that stops
//     reading its socket during a long tick). gdmf_window_pump() only
//     flushes outgoing requests and reports a dead connection as
//     closeRequested. See gdmf_window_wayland.c's header comment.

#include "gdmf.h"   // GDMF_DisplayMode

// --- Shared state, defined in gdmf.c ---------------------------------------

// Configuration recorded before GDMF_Init (read by the backend at creation).
extern const char*    gdmf_cfg_title;
extern int            gdmf_cfg_width;
extern int            gdmf_cfg_height;
extern int            gdmf_cfg_aspectNum;
extern int            gdmf_cfg_aspectDen;
extern unsigned char* gdmf_cfg_iconRGBA;   // NULL if never set
extern int            gdmf_cfg_iconWidth;
extern int            gdmf_cfg_iconHeight;

// Live window state: written by the backend (its window/event thread), read
// by the public getters from any thread.
extern volatile int  gdmf_st_width;            // client area, physical pixels
extern volatile int  gdmf_st_height;
extern volatile bool gdmf_st_resizeOccurred;
extern volatile bool gdmf_st_closeRequested;
extern volatile bool gdmf_st_minimized;
extern volatile bool gdmf_st_hasFocus;
extern volatile GDMF_DisplayMode gdmf_st_displayMode;  // backend writes on actual switch

// Desired input state: written by the public setters (any thread), read by
// the backend when (re)applying capture/visibility.
extern volatile bool gdmf_st_mouseCaptureDesired;
extern volatile bool gdmf_st_cursorVisibleDesired;

// --- Backend interface, one implementation per platform ---------------------

// Creates the window (spawning a window thread if the platform wants one)
// and blocks until it exists. Returns 0 on success, -1 on failure.
int  gdmf_window_create(void);

// Tears the window down and joins any window thread. Safe to call after a
// failed gdmf_window_create.
void gdmf_window_destroy(void);

// Services pending OS events. Called once per GDMF_Tick on the thread that
// drives the main loop. No-op on platforms with a dedicated window thread.
void gdmf_window_pump(void);

// Asks the backend to switch display mode. Asynchronous where the platform
// needs it to be (Win32 posts to the window thread); the backend updates
// gdmf_st_displayMode when the switch actually happens.
void gdmf_window_request_display_mode(GDMF_DisplayMode mode);

// Notifies the backend that gdmf_st_mouseCaptureDesired /
// gdmf_st_cursorVisibleDesired changed so it can (re)apply them.
void gdmf_window_input_state_changed(void);

// Current OS cursor position in client-area pixels (same space as
// gdmf_st_width/height). Returns false if there is no window yet.
bool gdmf_window_get_mouse_client(int* x, int* y);

// Monotonic time in seconds, for FPS measurement (gdmf.c's rolling window).
double gdmf_window_now_seconds(void);

#endif // GDMF_WINDOW_H
