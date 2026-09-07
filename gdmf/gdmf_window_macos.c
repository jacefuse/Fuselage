// GDMF -- macOS window backend, C half.
// Owns the backend logic and all engine-state writes; everything that is
// impossible in plain C (AppKit -- NSWindow, NSView, CAMetalLayer, the event
// pump) lives in the minimal Objective-C stub (gdmf_window_macos_stub.m),
// which does only those tasks and reports back through the gdmf_macos_notify_*
// callbacks below. Platform-neutral state and the public API live in gdmf.c;
// the interface between the two is gdmf_window.h.
//
// Threading: AppKit requires the window on the process's MAIN thread, so
// unlike the Win32 backend there is no dedicated window thread -- the window
// is created inside gdmf_window_create() (GDMF_Init already runs on the main
// thread) and pending events are serviced by gdmf_window_pump(), called once
// per GDMF_Tick on that same thread. The sim thread is unaffected; only
// rendering pauses during a live-resize drag.

#include "gdmf.h"
#include "gdmf_window.h"
#include <stdio.h>
#include <time.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

// --- The Objective-C stub's interface (gdmf_window_macos_stub.m) ------------

int   gdmf_macos_stub_create(const char* title, int width, int height,
                             int aspectNum, int aspectDen,
                             const unsigned char* iconRGBA, int iconW, int iconH);
void  gdmf_macos_stub_destroy(void);
void  gdmf_macos_stub_pump(void);
void* gdmf_macos_stub_metal_layer(void);
void* gdmf_macos_stub_nswindow(void);
void  gdmf_macos_stub_set_display_mode(int mode);  // GDMF_DisplayMode value
void  gdmf_macos_stub_apply_input_state(bool capture, bool cursorVisible);
bool  gdmf_macos_stub_mouse_client(int* x, int* y);

// --- Callbacks the stub reports events through ------------------------------
// All fire on the main thread, from inside gdmf_macos_stub_pump() or window
// delegate notifications. The C side owns every engine-state write.

void gdmf_macos_notify_resize(int pixelWidth, int pixelHeight) {
    if (pixelWidth > 0 && pixelHeight > 0 &&
        (pixelWidth != gdmf_st_width || pixelHeight != gdmf_st_height)) {
        gdmf_st_width          = pixelWidth;
        gdmf_st_height         = pixelHeight;
        gdmf_st_resizeOccurred = true;
    }

    return;
}

void gdmf_macos_notify_close(void) {
    gdmf_st_closeRequested = true;

    return;
}

void gdmf_macos_notify_focus(bool focused) {
    gdmf_st_hasFocus = focused;

    // Same rule as the Win32 backend's WM_SETFOCUS/WM_KILLFOCUS handling:
    // release capture/hidden-cursor while unfocused so switching away never
    // leaves the user's mouse stuck; reapply on refocus if still desired.
    gdmf_window_input_state_changed();

    return;
}

void gdmf_macos_notify_minimized(bool minimized) {
    gdmf_st_minimized = minimized;

    return;
}

// --- Backend interface (see gdmf_window.h) ----------------------------------

// Held for the window's whole lifetime. Only keyboard/mouse activity resets
// the OS idle timers -- a game played entirely on a controller looks "idle"
// to macOS, which then sleeps the display, drops Bluetooth, and takes the
// controller down with it. The assertion keeps display and system awake as
// long as the engine is running; releasing it on destroy restores normal
// power behavior the moment the window goes away.
static IOPMAssertionID g_powerAssertion = kIOPMNullAssertionID;

int gdmf_window_create(void) {
    if (gdmf_macos_stub_create(gdmf_cfg_title, gdmf_cfg_width, gdmf_cfg_height,
                               gdmf_cfg_aspectNum, gdmf_cfg_aspectDen,
                               gdmf_cfg_iconRGBA, gdmf_cfg_iconWidth, gdmf_cfg_iconHeight) != 0) {
        return -1;
    }

    if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                    kIOPMAssertionLevelOn,
                                    CFSTR("Fuselage engine running"),
                                    &g_powerAssertion) != kIOReturnSuccess) {
        g_powerAssertion = kIOPMNullAssertionID;
        printf("[GDMF] Warning: could not take the display-sleep assertion\n");
    }

    // Apply whatever mouse capture / cursor visibility was requested before
    // the window existed (the desired state is recorded immediately, but
    // couldn't act until now) -- same sequencing as the Win32 backend.
    gdmf_st_hasFocus = true;
    gdmf_window_input_state_changed();

    return 0;
}

void gdmf_window_destroy(void) {
    if (g_powerAssertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(g_powerAssertion);
        g_powerAssertion = kIOPMNullAssertionID;
    }

    gdmf_macos_stub_destroy();

    return;
}

void gdmf_window_pump(void) {
    gdmf_macos_stub_pump();

    return;
}

void gdmf_window_request_display_mode(GDMF_DisplayMode mode) {
    // Display modes are main-thread work in AppKit. This is called from the
    // game/render side; defer to the stub which hops to the main thread if
    // needed. The stub updates nothing itself -- it reports the applied mode
    // back and gdmf_st_displayMode is written here via the pump's callback
    // path (the stub calls gdmf_macos_notify_display_mode below).
    gdmf_macos_stub_set_display_mode((int)mode);

    return;
}

void gdmf_macos_notify_display_mode(int mode) {
    gdmf_st_displayMode = (GDMF_DisplayMode)mode;

    return;
}

void gdmf_window_input_state_changed(void) {
    // Only applied while focused -- the stub double-checks nothing; the
    // policy lives here, mirroring the Win32 backend's
    // gdmf_apply_input_state.
    bool focused = gdmf_st_hasFocus;
    bool capture = focused && gdmf_st_mouseCaptureDesired;
    bool visible = !focused || gdmf_st_cursorVisibleDesired;

    gdmf_macos_stub_apply_input_state(capture, visible);

    return;
}

bool gdmf_window_get_mouse_client(int* x, int* y) {
    return gdmf_macos_stub_mouse_client(x, y);
}

double gdmf_window_now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// Native handle for gdmf_surface_macos.c
void* GDMF_GetMetalLayer(void) {
    return gdmf_macos_stub_metal_layer();
}

// The NSWindow, for subsystems the orchestrator attaches to the application
// window (see gdmf.h's comment -- GDMF doesn't know who takes it).
void* GDMF_GetNativeWindowHandle(void) {
    return gdmf_macos_stub_nswindow();
}
