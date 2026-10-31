#ifndef GDMF_H
#define GDMF_H

// GDMF - Graphics Device Minimalist Framework
// Graphics and window management for Windows, macOS, and Linux (Wayland/X11).
// Platform handles are opaque so this header needs no operating-system headers.

#include <stdbool.h>
#include <stdint.h>
#include "window/gdmf_display_mode.h"
#include "window/gdmf_canvas.h"

#include "gdmf_colors.h"
#include "textlayer/gdmf_textlayer.h"
#include "sprites/gdmf_sprites.h"
#include "tiles/gdmf_tiles.h"
#include "pixies/gdmf_pixies.h"
#include "gdmf_interactions.h"

#define GDMF_VERSION "0.5.2026103101 ENTERON"

// Threading
// Run lifecycle and frame operations on the main/render thread.
// Synchronize changes to drawing state with GDMF_PrepareFrame(). Acquire and
// submit frames outside that lock. Set VSync on the render thread.
// Cross-thread safety is not guaranteed for all public calls; callers must
// synchronize shared access unless a verified contract provides otherwise.

// Configuration: set before GDMF_Init(). The title string is borrowed and must
// remain valid while the window is being created.
void GDMF_SetTitle(const char* title);

// Initial window client size in pixels, independent of the logical canvas size.
void GDMF_SetResolution(int width, int height);
void GDMF_SetAspectRatio(int num, int den);  // e.g. 16, 9

// Logical drawing dimensions, defaulting to 1280x720. Configure before init;
// window resizing scales the canvas without changing these dimensions.
void GDMF_SetCanvasResolution(int width, int height);

// Copies width*height*4 bytes of top-down RGBA8 data; the caller retains its
// buffer. Configure before init. No portable window-icon support on Wayland.
void GDMF_SetWindowIcon(int width, int height, const unsigned char* rgba);

// Lifecycle: GDMF_Init returns 0 on success or -1 if window or renderer
// initialization fails. Call these on the process's main thread.
// Stop producers before shutdown. Failed GPU recovery can retain the renderer
// and window; diagnostics report it. SEE: (GDMF_VULKAN.md, Renderer coordination).
int  GDMF_Init(void);
void GDMF_Shutdown(void);

// Service window events once per main-loop iteration. Returns false when
// closing was requested or rendering stopped for an error/recovery condition.
bool GDMF_Tick(void);

// Optional acquisition step. May block waiting for a frame slot; call before
// locking drawing state. Returns false when no frame can be acquired now.
// PrepareFrame acquires for itself if this step is omitted.
bool GDMF_AcquireFrame(void);

// Snapshot drawing state under the lock shared with application updates.
// Returns true when a frame was prepared, false when rendering is skipped
// (for example, while minimized or recreating the swapchain).
bool GDMF_PrepareFrame(void);

// Submit the prepared frame outside the drawing-state lock. May block for
// presentation. The backend does nothing if no frame was prepared.
void GDMF_SubmitFrame(void);

// Diagnostic rate over the most recently completed interval of at least 0.5s.
// Counts presentation requests accepted by Vulkan, excluding empty/failed
// submissions. It does not measure actual screen display. Returns 0 before a sample.
float GDMF_GetCurrentFPS(void);

// Request a display mode. The getter reports the backend's applied mode,
// which may lag the request when the platform handles it asynchronously.
void GDMF_SetDisplayMode(GDMF_DisplayMode mode);
GDMF_DisplayMode GDMF_GetDisplayMode(void);

// Background color, black by default. Alpha is ignored. This does not change
// the transparent background used by the offscreen mask compositor.
void  GDMF_SetClearColor(Color color);
Color GDMF_GetClearColor(void);

// VSync defaults to enabled. Disabled requests immediate presentation where
// supported; otherwise the backend uses synchronized presentation. Changes
// take effect on swapchain recreation. The getter reports the requested
// setting, not whether the surface supports unsynchronized presentation.
void GDMF_SetVSync(bool enabled);
bool GDMF_GetVSync(void);

// Current window client dimensions in pixels. Read current dimensions when
// needed rather than assuming the initial configuration still applies.
int GDMF_GetWidth(void);
int GDMF_GetHeight(void);

// Configured display aspect ratio, as numerator and denominator.
int GDMF_GetAspectRatioNum(void);
int GDMF_GetAspectRatioDen(void);

// Backend minimization indicator. Wayland uses suspended state where available,
// which can also represent conditions other than user-requested minimization.
bool GDMF_IsMinimized(void);
bool GDMF_IsFocused(void);  // true while the window has keyboard focus

// Consumes the resize flag: true if a resize was recorded since the previous
// consuming call. Multiple resize events may be represented by one true result.
bool GDMF_ResizeOccurred(void);

// Selected device information. Any output may be NULL. Returns false without
// writing outputs if no device is selected; otherwise returns true. Selection
// storage is cleared on completed renderer teardown.
// Returned strings belong to GDMF; do not modify or free them.
bool GDMF_GetDeviceInfo(const char** name, const char** type, int* score);

// Platform integration handles. Retained here pending separation of the
// window/backend interfaces. Handle types depend on the active platform.
#if defined(_WIN32)
void* GDMF_GetHWND(void);          // HWND
#elif defined(__APPLE__)
void* GDMF_GetMetalLayer(void);    // CAMetalLayer used by the Vulkan surface
#elif defined(__linux__)
void* GDMF_GetWaylandDisplay(void); // wl_display*
void* GDMF_GetWaylandSurface(void); // wl_surface*
void* GDMF_GetX11Display(void);     // Display*
void* GDMF_GetX11Window(void);      // X11 Window integer carried through uintptr_t
#endif

// Integration handle: HWND on Windows, NSWindow* on macOS, or a tagged Linux
// backend descriptor. NULL when no native window handle is available.
void* GDMF_GetNativeWindowHandle(void);

// Requested mouse capture: confinement on Windows, pinned cursor movement on
// macOS. Released while unfocused and reapplied on refocus. The getter reports
// the request, not the current OS effect.
// Toggle returns the new requested setting.
void GDMF_SetMouseCapture(bool capture);
bool GDMF_GetMouseCapture(void);
bool GDMF_ToggleMouseCapture(void);

// Requested cursor visibility, independent of confinement. The cursor is
// shown while unfocused; the getter still reports the requested setting.
void GDMF_SetCursorVisible(bool visible);
bool GDMF_GetCursorVisible(void);

// Backend cursor position mapped into the current logical canvas space.
// Wayland supplies the last surface-local position, not a global OS position.
// Coordinates may lie outside the canvas over letterbox bars or outside the
// window. Either output may be NULL. Returns coordinates of zero when a mouse
// position or usable viewport is unavailable; there is no separate error code.
void GDMF_GetMousePosition(float* x, float* y);

// Queue a development capture as a binary PPM (P6), at canvas resolution with
// letterboxing removed. Copies the path. True means accepted, not saved.
// False means the renderer is not initialized or has stopped, a request is pending,
// or path is NULL, empty, or too long
// for the current 512-byte path buffer (including the terminating null).
// Filesystem access is checked later; an accepted request may fail to write.
// Readback waits for device idle and stalls rendering; avoid repeated captures
// in normal frame processing.
bool GDMF_CaptureFrame(const char* path);

// True while a capture request is outstanding. Keep the scene unchanged until
// it clears. False is not proof that a file was saved successfully; readback
// and file-open outcomes are reported to stdout. A request may remain pending
// if rendering stops before it can be serviced.
bool GDMF_CapturePending(void);

#endif // GDMF_H
