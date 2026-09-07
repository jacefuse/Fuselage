// GDMF - Graphic Device Minimalist Framework
// Platform-neutral window management core: configuration, shared state, and
// the public API. Everything that actually touches the OS window lives in
// exactly one platform backend file (gdmf_window_win32.c,
// gdmf_window_macos.c) behind the interface in gdmf_window.h -- same rule as
// the Vulkan surface files. No Vulkan here; the renderer is gdmf_vulkan.c.

#include "gdmf.h"
#include "gdmf_window.h"
#include "gdmf_vulkan.h"
#include "gdmf_vulkan_internal.h"
#include "gdmf_textlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration only -- do not include gdmf_textlayer.h here (colors.h
// multiple-definition hazard: Color Colors[256][16] is defined in that header).
void gdmf_textlayer_shutdown(void);

// Configuration (set before GDMF_Init; read by the platform backend at
// window creation -- see gdmf_window.h)
const char*  gdmf_cfg_title     = "Fuselage";
int          gdmf_cfg_width     = 1280;
int          gdmf_cfg_height    = 720;
int          gdmf_cfg_aspectNum = 16;
int          gdmf_cfg_aspectDen = 9;
/* Design-resolution canvas -- the system's fixed native resolution, defaulting
   to 1280x720 and independent of the window size (the rendered canvas scales,
   aspect-locked, to fill whatever the window is). Changing it is the exception,
   not the norm: GDMF_SetCanvasResolution lets a game deliberately author in a
   different fixed space (e.g. 256x192). */
static int   g_canvasWidth  = 1280;
static int   g_canvasHeight = 720;

// Window icon (set before GDMF_Init; applied by the backend once the window
// is created)
unsigned char* gdmf_cfg_iconRGBA   = NULL;
int            gdmf_cfg_iconWidth  = 0;
int            gdmf_cfg_iconHeight = 0;

// Live window state (written by the backend's window/event thread, read here
// from any thread)
volatile int  gdmf_st_width           = 1280;
volatile int  gdmf_st_height          = 720;
volatile bool gdmf_st_resizeOccurred  = false;
volatile bool gdmf_st_closeRequested  = false;
volatile bool gdmf_st_minimized       = false;
volatile bool gdmf_st_hasFocus        = true;
volatile GDMF_DisplayMode gdmf_st_displayMode = GDMF_MODE_WINDOWED;

// Mouse capture / cursor visibility (desired state, settable from any
// thread; only actually applied while the window has focus -- the backend
// re-derives the real effect on focus/move/resize/toggle events)
volatile bool gdmf_st_mouseCaptureDesired  = false;
volatile bool gdmf_st_cursorVisibleDesired = true;

// FPS measurement (see GDMF_GetCurrentFPS's doc comment in gdmf.h). Counts
// frames submitted via GDMF_SubmitFrame() over a rolling ~0.5s window.
static double g_fpsWindowStart = 0.0;
static int    g_fpsFrameCount  = 0;
static float  g_currentFPS     = 0.0f;

// Public configuration
void GDMF_SetTitle(const char* title)         { gdmf_cfg_title  = title;

    return;
}
void GDMF_SetResolution(int width, int height) { gdmf_cfg_width  = width; gdmf_cfg_height = height;

    return;
}
void GDMF_SetCanvasResolution(int width, int height) { g_canvasWidth = width; g_canvasHeight = height;

    return;
}
void GDMF_SetAspectRatio(int num, int den)     { gdmf_cfg_aspectNum = num;   gdmf_cfg_aspectDen = den;

    return;
}

// Copies the RGBA buffer (caller retains ownership) -- applied once the
// backend creates the window. width/height define both source size and
// icon size; the OS scales as needed for taskbar/title-bar/dock display.
void GDMF_SetWindowIcon(int width, int height, const unsigned char* rgba) {
    free(gdmf_cfg_iconRGBA);
    gdmf_cfg_iconRGBA = NULL;
    gdmf_cfg_iconWidth = 0;
    gdmf_cfg_iconHeight = 0;

    if (width <= 0 || height <= 0 || !rgba) { return; }

    size_t size = (size_t)width * (size_t)height * 4;
    gdmf_cfg_iconRGBA = (unsigned char*)malloc(size);
    if (!gdmf_cfg_iconRGBA) { return; }

    memcpy(gdmf_cfg_iconRGBA, rgba, size);
    gdmf_cfg_iconWidth  = width;
    gdmf_cfg_iconHeight = height;

    return;
}

int GDMF_Init(void) {
    printf("[GDMF] Version %s\n", GDMF_VERSION);
    //printf("[GDMF] Init\n");
    //tlPrintFormattedC(GREEN, "[GDMF] Version %s", GDMF_VERSION);tlNewLine();
    //tlPrint("[GDMF] Init");tlNewLine();

    if (gdmf_window_create() != 0) {
        printf("[GDMF] Window creation failed\n");
        //tlPrint("[GDMF] Window creation failed");tlNewLine();

        return -1;
    }

    printf("[GDMF] Window ready (%dx%d)\n", gdmf_st_width, gdmf_st_height);
    //tlPrintFormattedC(WHITE, "[GDMF] Window ready (%dx%d)\n", gdmf_st_width, gdmf_st_height);tlNewLine();

    if (gdmf_vulkan_init() != 0) {
        printf("[GDMF] Vulkan init failed\n");
        //tlPrint("[GDMF] Vulkan init failed");tlNewLine();

        gdmf_vulkan_shutdown();
        gdmf_window_destroy();
        return -1;
    }

    return 0;
}

void GDMF_Shutdown(void) {
    printf("[GDMF] Shutdown\n");
    //tlPrint("[GDMF] Shutdown");tlNewLine();


    // Subsystem shutdown must happen before Vulkan tears down (device still valid)
    gdmf_textlayer_shutdown();

    // Vulkan resources must be destroyed before the window closes
    gdmf_vulkan_shutdown();

    gdmf_window_destroy();

    free(gdmf_cfg_iconRGBA);
    gdmf_cfg_iconRGBA = NULL;

    printf("[GDMF] Done\n");
    //tlPrint("[GDMF] Done");tlNewLine();

    return;
}

bool GDMF_Tick(void) {
    // Service pending OS events on platforms whose window lives on this
    // thread (macOS). A no-op where a dedicated window thread pumps instead
    // (Win32) -- see the threading contract in gdmf_window.h.
    gdmf_window_pump();

    // Returns false if the OS closed the window, or if the GPU device was
    // lost (driver reset) -- see gdmf_vulkan_device_lost's doc comment.
    // There's no recovery path for a lost device yet; this just stops the
    // engine cleanly instead of the render loop continuing to submit to a
    // dead device every frame.
    return !gdmf_st_closeRequested && !gdmf_vulkan_device_lost();
}

void GDMF_PrepareFrame(void) {
    gdmf_vulkan_prepare_frame();

    return;
}

void GDMF_SubmitFrame(void) {
    gdmf_vulkan_submit_frame();

    // FPS measurement -- see GDMF_GetCurrentFPS() doc comment. Counted here
    // (not in GDMF_PrepareFrame()) since this is the call that actually
    // attempts to submit/present, so it's one bump per presented frame.
    double now = gdmf_window_now_seconds();
    if (g_fpsWindowStart == 0.0) {
        g_fpsWindowStart = now;
    }
    g_fpsFrameCount++;
    double elapsed = now - g_fpsWindowStart;
    if (elapsed >= 0.5) {
        g_currentFPS = (float)(g_fpsFrameCount / elapsed);
        g_fpsFrameCount = 0;
        g_fpsWindowStart = now;
    }

    return;
}

float GDMF_GetCurrentFPS(void) {
    return g_currentFPS;
}

// Display mode
void GDMF_SetDisplayMode(GDMF_DisplayMode mode) {
    // Handed to the backend -- safe from any thread (Win32 posts it to the
    // window thread; the backend updates gdmf_st_displayMode when the
    // switch actually happens).
    gdmf_window_request_display_mode(mode);

    return;
}

GDMF_DisplayMode GDMF_GetDisplayMode(void) {
    return gdmf_st_displayMode;
}

// Mouse capture / cursor visibility
// The desired state is recorded immediately regardless of whether the
// window exists yet (so calls made before GDMF_Init() are honored once the
// window is created -- the backend applies the desired state right after
// creation). If the window already exists, the backend is also notified for
// immediate effect.
void GDMF_SetMouseCapture(bool capture) {
    gdmf_st_mouseCaptureDesired = capture;
    gdmf_window_input_state_changed();

    return;
}

bool GDMF_GetMouseCapture(void) {
    return gdmf_st_mouseCaptureDesired;
}

bool GDMF_ToggleMouseCapture(void) {
    bool newState = !gdmf_st_mouseCaptureDesired;

    GDMF_SetMouseCapture(newState);

    return newState;
}

void GDMF_SetCursorVisible(bool visible) {
    gdmf_st_cursorVisibleDesired = visible;
    gdmf_window_input_state_changed();

    return;
}

bool GDMF_GetCursorVisible(void) {
    return gdmf_st_cursorVisibleDesired;
}

// Mouse position
// The backend gives the real client-area pixel the OS cursor is over;
// gdmf_get_render_viewport_rect() is the same aspect-correct
// sub-rectangle sprites/tiles already render onto (see its own comment in
// gdmf_vulkan.c), so inverting that mapping gets from client pixels to
// reference-canvas coordinates -- the same design-resolution canvas space
// (GDMF_GetCanvasWidth/Height, = the game's design resolution) every
// sprite/tile position is already expressed in.
void GDMF_GetMousePosition(float* x, float* y) {
    float refX = 0.0f;
    float refY = 0.0f;

    int px, py;
    if (gdmf_window_get_mouse_client(&px, &py)) {
        VkRect2D rect = gdmf_get_render_viewport_rect();
        if (rect.extent.width > 0 && rect.extent.height > 0) {
            refX = ((float)(px - rect.offset.x) / (float)rect.extent.width)  * (float)GDMF_GetCanvasWidth();
            refY = ((float)(py - rect.offset.y) / (float)rect.extent.height) * (float)GDMF_GetCanvasHeight();
        }
    }

    if (x) { *x = refX; }
    if (y) { *y = refY; }

    return;
}

// Window state queries
int  GDMF_GetWidth(void)     { return gdmf_st_width; }
int  GDMF_GetHeight(void)    { return gdmf_st_height; }
bool GDMF_IsMinimized(void)  { return gdmf_st_minimized; }
bool GDMF_IsFocused(void)    { return gdmf_st_hasFocus; }

int GDMF_GetAspectRatioNum(void) { return gdmf_cfg_aspectNum; }
int GDMF_GetAspectRatioDen(void) { return gdmf_cfg_aspectDen; }

// The design-resolution canvas: the fixed logical pixel space every
// sprite/tile/pixie coordinate is expressed in. Defaults to the system's
// 1280x720 native resolution and stays fixed regardless of window size (the
// image is scaled, aspect-locked, to fit); a game that deliberately calls
// GDMF_SetCanvasResolution(256, 192) gets a 256x192 canvas instead. Set once
// before GDMF_Init and never changed after, so this is safe to read from the
// render/game thread.
int GDMF_GetCanvasWidth(void)  { return g_canvasWidth;  }
int GDMF_GetCanvasHeight(void) { return g_canvasHeight; }

bool GDMF_ResizeOccurred(void) {
    if (gdmf_st_resizeOccurred) {
        gdmf_st_resizeOccurred = false;
        return true;
    }

    return false;
}
