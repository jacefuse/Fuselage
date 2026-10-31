#ifndef GDMF_WINDOW_H
#define GDMF_WINDOW_H

// Internal interface between gdmf.c and the platform window backends.
// Windows and macOS implement it directly. Linux dispatches to Wayland or X11.

#include <stdbool.h>
#include "gdmf_display_mode.h"

// Window creation and event handling follow platform requirements:
// - Windows uses a dedicated window thread; pump() does nothing.
// - macOS creates and services the window on the process's main thread.
// - Wayland creates the window on the main thread and dispatches events on a
//   reader thread; pump() flushes requests and checks connection state.
// Call create(), destroy(), and pump() from the engine's main thread.
// Shared state still uses volatile; this does not provide synchronization.
// Cross-thread access requires the FUNDUS threading audit before a general
// safety guarantee can be made.

// Configuration supplied before GDMF_Init(), read during window creation.
// The title is borrowed. The icon buffer is owned by gdmf.c.
extern const char*    gdmf_window_config_title;
extern int            gdmf_window_config_width;
extern int            gdmf_window_config_height;
extern int            gdmf_window_config_aspect_numerator;
extern int            gdmf_window_config_aspect_denominator;
extern unsigned char* gdmf_window_config_icon_rgba;   // NULL when absent
extern int            gdmf_window_config_icon_width;
extern int            gdmf_window_config_icon_height;

// Current client dimensions and state, published by the active backend.
// Width and height form a pair, but separate reads are not an atomic snapshot.
// Platform scaling and coordinate units must be reconciled during backend review.
extern volatile int  gdmf_window_width;
extern volatile int  gdmf_window_height;
extern volatile bool gdmf_window_resize_pending;
extern volatile bool gdmf_window_close_requested;
extern volatile bool gdmf_window_minimized;
extern volatile bool gdmf_window_has_focus;
extern volatile GDMF_DisplayMode gdmf_window_display_mode;

// Requested input settings. Their effect depends on window focus and platform
// support; the requested values are retained while the window is unfocused.
extern volatile bool gdmf_window_mouse_capture_requested;
extern volatile bool gdmf_window_cursor_visible_requested;

// Create the window and wait for the backend's creation result.
// Returns 0 on success or -1 on failure. No detailed error code is returned.
int gdmf_window_create(void);

// Request window destruction and release backend resources. Windows currently
// waits up to four seconds for its window thread; completion is not reported.
// Cleanup after partial creation must be checked as each backend is reviewed.
void gdmf_window_destroy(void);

// Service platform events or connection maintenance once per GDMF_Tick().
// A dedicated event thread does not necessarily make this operation a no-op.
void gdmf_window_pump(void);

// Request a display mode. Completion may be asynchronous; the backend updates
// gdmf_window_display_mode. Platform support determines the effective behavior.
void gdmf_window_request_display_mode(GDMF_DisplayMode mode);

// Notify the backend after changing the requested mouse capture or cursor
// visibility setting so it can apply the appropriate focus-dependent effect.
void gdmf_window_input_state_changed(void);

// Query cursor position in the backend's client-coordinate space. Both output
// pointers must be valid. Returns false when the backend cannot supply a
// position; outputs must not be consumed on failure. gdmf.c maps a successful
// result into canvas coordinates.
bool gdmf_window_get_mouse_client(int* mouse_x, int* mouse_y);

// Platform monotonic time in seconds, used for FPS measurement. This is an
// elapsed-time clock, not a calendar timestamp.
double gdmf_window_now_seconds(void);

#endif // GDMF_WINDOW_H
