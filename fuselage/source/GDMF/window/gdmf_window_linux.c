// Linux window backend selection and dispatch.
// SEE: (GDMF_LINUX.md, Backend selection).
#define _POSIX_C_SOURCE 200809L

#include "gdmf_window.h"
#include "gdmf_window_linux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const gdmf_linux_window_backend* gdmf_linux_active_window_backend = NULL;

const gdmf_linux_window_backend* gdmf_linux_get_active_window_backend(void) {
    return gdmf_linux_active_window_backend;
}

int gdmf_window_create(void) {
    const gdmf_linux_window_backend* backend_candidates[2] = { NULL, NULL };
    int candidate_count = 0;

    const char* requested_backend = getenv("FUSELAGE_BACKEND");
    if (requested_backend && strcmp(requested_backend, "wayland") == 0) {
        backend_candidates[candidate_count++] = &gdmf_linux_window_backend_wayland;
    } else if (requested_backend && strcmp(requested_backend, "x11") == 0) {
        backend_candidates[candidate_count++] = &gdmf_linux_window_backend_x11;
    } else {
        if (requested_backend) {
            printf("[GDMF] FUSELAGE_BACKEND=%s not recognized. Probing normally\n", requested_backend);
        }
        backend_candidates[candidate_count++] = &gdmf_linux_window_backend_wayland;
        backend_candidates[candidate_count++] = &gdmf_linux_window_backend_x11;
    }

    // Creation is the probe. A failed candidate owns its partial cleanup.
    for (int candidate_index = 0; candidate_index < candidate_count; candidate_index++) {
        if (backend_candidates[candidate_index]->create() == 0) {
            gdmf_linux_active_window_backend = backend_candidates[candidate_index];
            printf("[GDMF] Window backend: %s\n", gdmf_linux_active_window_backend->name);
            return 0;
        }
    }

    printf("[GDMF] No usable display backend (tried %d)\n", candidate_count);
    return -1;
}

void gdmf_window_destroy(void) {
    if (gdmf_linux_active_window_backend) {
        gdmf_linux_active_window_backend->destroy();
        gdmf_linux_active_window_backend = NULL;
    }
    return;
}

void gdmf_window_pump(void) {
    if (gdmf_linux_active_window_backend) { gdmf_linux_active_window_backend->pump(); }
    return;
}

void gdmf_window_request_display_mode(GDMF_DisplayMode mode) {
    if (gdmf_linux_active_window_backend) {
        gdmf_linux_active_window_backend->request_display_mode(mode);
    }
    return;
}

void gdmf_window_input_state_changed(void) {
    if (gdmf_linux_active_window_backend) {
        gdmf_linux_active_window_backend->input_state_changed();
    }
    return;
}

bool gdmf_window_get_mouse_client(int* mouse_x, int* mouse_y) {
    return gdmf_linux_active_window_backend
         ? gdmf_linux_active_window_backend->get_mouse_client(mouse_x, mouse_y) : false;
}

// Both display backends use the same monotonic platform clock.
double gdmf_window_now_seconds(void) {
    struct timespec clock_sample;
    clock_gettime(CLOCK_MONOTONIC, &clock_sample);

    return (double)clock_sample.tv_sec + (double)clock_sample.tv_nsec / 1e9;
}

// The selected backend owns the tagged descriptor shared with input consumers.
void* GDMF_GetNativeWindowHandle(void) {
    return gdmf_linux_active_window_backend
         ? gdmf_linux_active_window_backend->get_native_handle() : NULL;
}
