#ifndef FUSELAGE_NATIVE_H
#define FUSELAGE_NATIVE_H

// The Linux native-window handle convention.
//
// On Windows and macOS the opaque handle the orchestrator passes from
// GDMF_GetNativeWindowHandle to CAKE_AttachWindow is a raw platform type
// (HWND, NSWindow*) -- the OS defines it, so producer and consumer agree
// without sharing a header. Linux has no such single type: one binary
// carries both a Wayland and an X11 backend and picks at run time, so the
// handle's shape is OUR convention -- and it lives here, in a standalone
// header owned by neither GDMF nor CAKE (the fuselage_sync.h precedent:
// shared plumbing, no subsystem knowledge). GDMF fills one in; CAKE reads
// it; neither includes the other.
//
// kind is a static string, compared with strcmp: "wayland" or "x11".
//   wayland: display = the struct wl_display*; window unused (NULL).
//   x11:     display = the Xlib Display*; window = the X11 Window id,
//            cast through uintptr_t (an XID is an integer, not a pointer).
//
// The pointer returned by GDMF_GetNativeWindowHandle points at storage
// owned by the active window backend, valid from window creation to
// GDMF_Shutdown.

#if defined(__linux__)

typedef struct {
    const char* kind;
    void*       display;
    void*       window;
} FuselageLinuxNativeHandle;

#endif // __linux__

#endif // FUSELAGE_NATIVE_H
