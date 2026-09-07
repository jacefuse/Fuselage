// GDMF -- Linux window backend dispatcher.
// One binary, both display servers: the gdmf_window_* interface from
// gdmf_window.h is implemented here once, forwarding to the backend table
// (gdmf_window_linux.h) that wins the probe in gdmf_window_create:
//
//   1. FUSELAGE_BACKEND=wayland|x11 forces that backend, no fallback --
//      an override that names a backend gets that backend or an error,
//      never a silent substitute.
//   2. Otherwise Wayland is tried first (its create fails fast when no
//      compositor answers), then X11.
//
// A backend's create() IS the probe -- connecting to the display server is
// the first thing create does, so a dedicated "is it there" pre-check
// would just duplicate it. Whichever backend wins owns the window until
// gdmf_window_destroy; there is no mid-run switching.

// -std=c11 is strict ISO; clock_gettime is POSIX and must be asked for by
// name before any header is pulled in.
#define _POSIX_C_SOURCE 200809L

#include "gdmf.h"
#include "gdmf_window.h"
#include "gdmf_window_linux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const gdmf_linux_backend* g_active = NULL;

const gdmf_linux_backend* gdmf_linux_active_backend(void) {
    return g_active;
}

// --- The gdmf_window.h interface, dispatched -------------------------------

int gdmf_window_create(void) {
    const gdmf_linux_backend* candidates[2] = { NULL, NULL };
    int count = 0;

    const char* forced = getenv("FUSELAGE_BACKEND");
    if (forced && strcmp(forced, "wayland") == 0) {
        candidates[count++] = &gdmf_backend_wayland;
    } else if (forced && strcmp(forced, "x11") == 0) {
        candidates[count++] = &gdmf_backend_x11;
    } else {
        if (forced) {
            printf("[GDMF] FUSELAGE_BACKEND=%s not recognized -- probing normally\n", forced);
        }
        candidates[count++] = &gdmf_backend_wayland;
        candidates[count++] = &gdmf_backend_x11;
    }

    for (int i = 0; i < count; i++) {
        if (candidates[i]->create() == 0) {
            g_active = candidates[i];
            printf("[GDMF] Window backend: %s\n", g_active->name);
            return 0;
        }
    }

    printf("[GDMF] No usable display backend (tried %d)\n", count);
    return -1;
}

void gdmf_window_destroy(void) {
    if (g_active) {
        g_active->destroy();
        g_active = NULL;
    }
    return;
}

void gdmf_window_pump(void) {
    if (g_active) { g_active->pump(); }
    return;
}

void gdmf_window_request_display_mode(GDMF_DisplayMode mode) {
    if (g_active) { g_active->request_display_mode(mode); }
    return;
}

void gdmf_window_input_state_changed(void) {
    if (g_active) { g_active->input_state_changed(); }
    return;
}

bool gdmf_window_get_mouse_client(int* x, int* y) {
    return g_active ? g_active->get_mouse_client(x, y) : false;
}

// Both Linux backends would answer with the same clock; answered once here.
double gdmf_window_now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// The opaque handle the orchestrator hands to other subsystems
// (CAKE_AttachWindow): a FuselageLinuxNativeHandle (fuselage_native.h),
// tagged with which backend filled it -- the taker checks ->kind.
void* GDMF_GetNativeWindowHandle(void) {
    return g_active ? g_active->native_handle() : NULL;
}
