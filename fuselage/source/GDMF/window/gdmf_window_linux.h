#ifndef GDMF_WINDOW_LINUX_H
#define GDMF_WINDOW_LINUX_H

// Internal Linux window dispatch interface. Creation selects Wayland or X11;
// the selected backend remains active until destruction. Monotonic timing is
// supplied by the dispatcher, since both backends use the same platform clock.

#include <stdbool.h>
#include "gdmf_display_mode.h"

typedef struct {
    const char* name;  // "wayland" or "x11"; also used to select the Vulkan surface

    // Return 0 on successful creation or -1 on failure. Failed candidates must
    // release their partial resources before the dispatcher tries another.
    int (*create)(void);
    void (*destroy)(void);

    // Event handling or connection maintenance, according to the backend.
    void (*pump)(void);
    void (*request_display_mode)(GDMF_DisplayMode mode);
    void (*input_state_changed)(void);

    // Both outputs must be valid. False means no position was supplied;
    // outputs must not be consumed on failure. Coordinates use client space.
    bool (*get_mouse_client)(int* mouse_x, int* mouse_y);

    // Borrowed FuselageLinuxNativeHandle descriptor, owned by this backend.
    // Consumers must not free it or retain it after backend destruction.
    void* (*get_native_handle)(void);
} gdmf_linux_window_backend;

extern const gdmf_linux_window_backend gdmf_linux_window_backend_wayland;
extern const gdmf_linux_window_backend gdmf_linux_window_backend_x11;

// Borrowed table pointer, or NULL when no backend has been selected. The table
// describes the implementation; its existence does not establish thread safety.
const gdmf_linux_window_backend* gdmf_linux_get_active_window_backend(void);

#endif // GDMF_WINDOW_LINUX_H
