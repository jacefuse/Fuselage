// Linux attachment and publication. SEE: (CAKE.md, Linux input)
#include "cake_linux_private.h"
#if defined(__linux__)
#include "../../../BASE/fuselage_native.h"
#include <stdio.h>
#include <string.h>
static bool cake_linux_initialized;
static bool cake_linux_attached;
void CAKE_AttachWindow(void* native_window) {
    if (!native_window || cake_linux_attached) {
        return;
    }
    const FuselageLinuxNativeHandle* handle = native_window;
    if (!handle->backend_name || !handle->display_connection) {
        return;
    }
    if (strcmp(handle->backend_name, "wayland") == 0) {
        cake_linux_attached = cake_linux_wayland_attach(handle->display_connection);
    } else if (strcmp(handle->backend_name, "x11") == 0) {
        cake_linux_attached = cake_linux_x11_attach(handle->display_connection);
    }
    if (!cake_linux_attached) {
        fprintf(stderr, "[CAKE] Linux input attachment failed\n");
    }
}
void CAKE_Poll(void) {
    ++CAKE_PollCount;
    if (!cake_linux_initialized) {
        cake_linux_initialize_controllers();
        cake_linux_scan_controllers();
        cake_linux_initialized = true;
    }
    cake_linux_wayland_drain();
    cake_linux_x11_drain();
    cake_linux_publish_input();
    cake_linux_poll_controllers();
    cake_derive_controller_edges();
}
void CAKE_Shutdown(void) {
    if (cake_linux_initialized) {
        cake_linux_shutdown_controllers();
    }
    if (cake_linux_attached) {
        cake_linux_wayland_detach();
        cake_linux_x11_detach();
    }
    cake_linux_attached = false;
    cake_linux_initialized = false;
}
#endif
