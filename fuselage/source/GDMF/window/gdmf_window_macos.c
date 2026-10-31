// macOS window backend: C interface, shared-state updates, and focus policy.
// AppKit lifecycle and presentation live in the two Objective-C companions.
// Create, destroy, and pump the window on the process's main thread.

#include "../gdmf.h"
#include "gdmf_window.h"
#include "gdmf_window_macos_internal.h"
#include <stdio.h>
#include <time.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

// AppKit callbacks publish current backing-pixel dimensions and window state.
void gdmf_macos_notify_resize(int pixel_width, int pixel_height) {
    if (pixel_width > 0 && pixel_height > 0 &&
        (pixel_width != gdmf_window_width || pixel_height != gdmf_window_height)) {
        gdmf_window_width          = pixel_width;
        gdmf_window_height         = pixel_height;
        gdmf_window_resize_pending = true;
    }

    return;
}

void gdmf_macos_notify_close(void) {
    gdmf_window_close_requested = true;

    return;
}

void gdmf_macos_notify_focus(bool window_focused) {
    gdmf_window_has_focus = window_focused;
    gdmf_window_input_state_changed();

    return;
}

void gdmf_macos_notify_minimized(bool minimized) {
    gdmf_window_minimized = minimized;

    return;
}

// Prevent idle display sleep while the window exists. Creation failure is
// nonfatal; this assertion alone does not promise system-sleep prevention.
static IOPMAssertionID gdmf_display_sleep_assertion = kIOPMNullAssertionID;

int gdmf_window_create(void) {
    if (gdmf_macos_appkit_create(gdmf_window_config_title,
                               gdmf_window_config_width, gdmf_window_config_height,
                               gdmf_window_config_aspect_numerator,
                               gdmf_window_config_aspect_denominator,
                               gdmf_window_config_icon_rgba,
                               gdmf_window_config_icon_width,
                               gdmf_window_config_icon_height) != 0) {
        return -1;
    }

    if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                    kIOPMAssertionLevelOn,
                                    CFSTR("Fuselage engine running"),
                                    &gdmf_display_sleep_assertion) != kIOReturnSuccess) {
        gdmf_display_sleep_assertion = kIOPMNullAssertionID;
        printf("[GDMF] Warning: could not take the display-sleep assertion\n");
    }
    gdmf_window_has_focus = true;
    gdmf_window_input_state_changed();

    return 0;
}

void gdmf_window_destroy(void) {
    if (gdmf_display_sleep_assertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(gdmf_display_sleep_assertion);
        gdmf_display_sleep_assertion = kIOPMNullAssertionID;
    }

    gdmf_macos_appkit_destroy();

    return;
}

void gdmf_window_pump(void) {
    gdmf_macos_appkit_pump();

    return;
}

// The companion queues AppKit work on the main thread. Both fullscreen modes
// currently use a borderless window; no exclusive display capture is performed.
void gdmf_window_request_display_mode(GDMF_DisplayMode mode) {
    gdmf_macos_appkit_set_display_mode(mode);

    return;
}

void gdmf_macos_notify_display_mode(GDMF_DisplayMode mode) {
    gdmf_window_display_mode = mode;

    return;
}

// Resolve requested settings against focus before queuing the AppKit work.
// Capture pins cursor movement on macOS rather than clipping to a rectangle.
void gdmf_window_input_state_changed(void) {
    bool window_focused = gdmf_window_has_focus;
    bool capture_requested = window_focused && gdmf_window_mouse_capture_requested;
    bool cursor_visible = !window_focused || gdmf_window_cursor_visible_requested;

    gdmf_macos_appkit_apply_input_state(capture_requested, cursor_visible);

    return;
}

// The companion returns backing-pixel coordinates with a top-left origin.
bool gdmf_window_get_mouse_client(int* mouse_x, int* mouse_y) {
    return gdmf_macos_appkit_mouse_client(mouse_x, mouse_y);
}

double gdmf_window_now_seconds(void) {
    struct timespec clock_sample;
    clock_gettime(CLOCK_MONOTONIC, &clock_sample);

    return (double)clock_sample.tv_sec + (double)clock_sample.tv_nsec / 1e9;
}

// The current Vulkan/MoltenVK surface consumes this CAMetalLayer. This handle
// also provides a window-to-renderer boundary for future native Metal work.
void* GDMF_GetMetalLayer(void) {
    return gdmf_macos_appkit_metal_layer();
}

void* GDMF_GetNativeWindowHandle(void) {
    return gdmf_macos_appkit_window_handle();
}
