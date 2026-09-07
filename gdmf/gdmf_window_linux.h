#ifndef GDMF_WINDOW_LINUX_H
#define GDMF_WINDOW_LINUX_H

// GDMF internal -- the Linux backend dispatch table.
//
// Linux is the one platform where a single binary carries more than one
// window backend: Wayland-first, X11 as the fallback, chosen at run time
// by gdmf_window_linux.c (see its header comment for the probe order).
// Each backend implements this table with its functions namespaced
// (gdmf_window_wl_*, gdmf_window_x11_*); the dispatcher owns the public
// gdmf_window_* interface from gdmf_window.h and forwards to whichever
// table won the probe. now_seconds is absent on purpose -- both backends
// would answer with the same CLOCK_MONOTONIC read, so the dispatcher
// implements it once.

#include <stdbool.h>
#include "gdmf.h"   // GDMF_DisplayMode

typedef struct {
    const char* name;                                  // "wayland" / "x11"
    int   (*create)(void);
    void  (*destroy)(void);
    void  (*pump)(void);
    void  (*request_display_mode)(GDMF_DisplayMode mode);
    void  (*input_state_changed)(void);
    bool  (*get_mouse_client)(int* x, int* y);
    void* (*native_handle)(void);   // the FuselageLinuxNativeHandle*
} gdmf_linux_backend;

extern const gdmf_linux_backend gdmf_backend_wayland;
extern const gdmf_linux_backend gdmf_backend_x11;

// The backend that won the probe, or NULL before gdmf_window_create /
// after a failed one. The Vulkan surface dispatcher keys on ->name to
// create the matching VkSurfaceKHR flavor.
const gdmf_linux_backend* gdmf_linux_active_backend(void);

#endif // GDMF_WINDOW_LINUX_H
