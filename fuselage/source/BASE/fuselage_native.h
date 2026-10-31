#ifndef FUSELAGE_NATIVE_H
#define FUSELAGE_NATIVE_H

// Shared Linux window descriptor passed from graphics to input. This contract
// belongs to neither system and requires no Wayland or X11 headers.
// SEE: (GDMF_LINUX.md, Native window descriptor).
#if defined(__linux__)

typedef struct {
    const char* backend_name; // static "wayland" or "x11" string
    void* display_connection; // wl_display* or Xlib Display*
    void* window_id;          // X11 Window via uintptr_t; NULL on Wayland
} FuselageLinuxNativeHandle;

// The backend owns this descriptor and the connection it describes. Consumers
// borrow them, must not free or modify them, and must stop using them before
// backend destruction. A retained descriptor address does not extend lifetime.

#endif // __linux__

#endif // FUSELAGE_NATIVE_H
