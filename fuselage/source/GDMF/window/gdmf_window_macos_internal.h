#ifndef GDMF_WINDOW_MACOS_INTERNAL_H
#define GDMF_WINDOW_MACOS_INTERNAL_H

// C/Objective-C boundary. AppKit lifecycle and direct object access belong on
// the main thread; presentation setters queue their AppKit work there.
#include <stdbool.h>
#include "gdmf_display_mode.h"

// Main-thread creation: returns 0 on success or -1 on failure. Window dimensions
// are AppKit points; resize callbacks below publish backing-pixel dimensions.
int gdmf_macos_appkit_create(const char* title, int width, int height,
                            int aspect_numerator, int aspect_denominator,
                            const unsigned char* icon_rgba, int icon_width, int icon_height);
void gdmf_macos_appkit_destroy(void);
void gdmf_macos_appkit_pump(void);
void* gdmf_macos_appkit_metal_layer(void);
void* gdmf_macos_appkit_window_handle(void);
// Queue requests; return does not acknowledge application or cancel old work.
void gdmf_macos_appkit_set_display_mode(GDMF_DisplayMode mode);
void gdmf_macos_appkit_apply_input_state(bool capture_requested, bool cursor_visible);
// Main-thread query. Both outputs must be valid. False means no window/view;
// true supplies top-left backing-pixel coordinates, possibly outside the view.
bool gdmf_macos_appkit_mouse_client(int* mouse_x, int* mouse_y);

// AppKit notifications implemented by the C backend.
void gdmf_macos_notify_resize(int pixel_width, int pixel_height);
void gdmf_macos_notify_close(void);
void gdmf_macos_notify_focus(bool window_focused);
void gdmf_macos_notify_minimized(bool minimized);
void gdmf_macos_notify_display_mode(GDMF_DisplayMode mode);

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>

// Shared only by the two AppKit implementation files. The lifecycle file owns
// these objects; the presentation file uses them on the main thread.
extern NSWindow* gdmf_macos_window;
extern NSView* gdmf_macos_content_view;
void gdmf_macos_sync_drawable_size(void);
void gdmf_macos_restore_presentation(void);
#endif

#endif // GDMF_WINDOW_MACOS_INTERNAL_H
