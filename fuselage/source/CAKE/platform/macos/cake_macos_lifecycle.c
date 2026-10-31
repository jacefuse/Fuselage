// macOS attachment and publication. SEE: (CAKE.md, macOS input and controllers)
#include "cake_macos_private.h"
#if defined(__APPLE__)
#include <stdio.h>

static bool cake_macos_initialized;
static bool cake_macos_attached;

void CAKE_AttachWindow(void* native_window) {
    if (!native_window) {
        return;
    }
    if (!cake_macos_is_main_thread()) {
        fprintf(stderr, "[CAKE] Window attachment must use the main thread\n");
        return;
    }
    if (!cake_macos_attach_input(native_window)) {
        fprintf(stderr, "[CAKE] Window attachment failed\n");
        return;
    }
    cake_macos_attached = true;
    cake_macos_enable_controllers();
    cake_macos_start_controllers();
}

void CAKE_Poll(void) {
    ++CAKE_PollCount;
    cake_macos_initialized = true;
    cake_macos_publish_input();
    cake_macos_poll_controllers();
    cake_derive_controller_edges();
}

void CAKE_Shutdown(void) {
    if (!cake_macos_initialized && !cake_macos_attached) {
        return;
    }
    if (!cake_macos_is_main_thread()) {
        fprintf(stderr, "[CAKE] Shutdown must use the main thread after polling stops\n");
        return;
    }
    // Invalidate opaque pointers while the bridge still retains the objects.
    cake_macos_clear_controllers();
    cake_macos_stop_controllers();
    cake_macos_detach_input();
    cake_macos_attached = false;
    cake_macos_initialized = false;
}
#endif
