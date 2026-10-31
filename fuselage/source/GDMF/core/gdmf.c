// GDMF configuration, shared window state, lifecycle, and frame coordination.
// OS window operations use gdmf_window.h; rendering uses the Vulkan backend.

#include "../gdmf.h"
#include "../window/gdmf_window.h"
#include "../vulkan/gdmf_vulkan.h"
#include "../vulkan/renderer/gdmf_vulkan_internal.h"
#include "../textlayer/gdmf_textlayer.h"
#include "../workers/gdmf_workers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Configuration consumed by the window backend during initialization.
const char*  gdmf_window_config_title = "Fuselage";
int          gdmf_window_config_width = 1280;
int          gdmf_window_config_height = 720;
int          gdmf_window_config_aspect_numerator = 16;
int          gdmf_window_config_aspect_denominator = 9;

// Logical drawing dimensions, independent of the window's client dimensions.
static int gdmf_canvas_width = 1280;
static int gdmf_canvas_height = 720;

unsigned char* gdmf_window_config_icon_rgba   = NULL;
int            gdmf_window_config_icon_width  = 0;
int            gdmf_window_config_icon_height = 0;

// Backend-published state. Volatile is retained pending synchronization hardening.
volatile int  gdmf_window_width           = 1280;
volatile int  gdmf_window_height          = 720;
volatile bool gdmf_window_resize_pending  = false;
volatile bool gdmf_window_close_requested  = false;
volatile bool gdmf_window_minimized       = false;
volatile bool gdmf_window_has_focus        = true;
volatile GDMF_DisplayMode gdmf_window_display_mode = GDMF_MODE_WINDOWED;
volatile bool gdmf_window_mouse_capture_requested  = false;
volatile bool gdmf_window_cursor_visible_requested = true;

// Counts accepted presentation requests. See GDMF_GetCurrentFPS.
static double gdmf_fps_sample_start_seconds = 0.0;
static int    gdmf_fps_presentation_count = 0;
static float  gdmf_current_fps = 0.0f;

void GDMF_SetTitle(const char* title) {
    gdmf_window_config_title = title;

    return;
}

void GDMF_SetResolution(int width, int height) {
    gdmf_window_config_width = width;
    gdmf_window_config_height = height;

    return;
}

void GDMF_SetCanvasResolution(int width, int height) {
    gdmf_canvas_width = width;
    gdmf_canvas_height = height;

    return;
}

void GDMF_SetAspectRatio(int numerator, int denominator) {
    gdmf_window_config_aspect_numerator = numerator;
    gdmf_window_config_aspect_denominator = denominator;

    return;
}

void GDMF_SetWindowIcon(int width, int height, const unsigned char* icon_rgba) {
    free(gdmf_window_config_icon_rgba);
    gdmf_window_config_icon_rgba = NULL;
    gdmf_window_config_icon_width = 0;
    gdmf_window_config_icon_height = 0;

    if (width <= 0 || height <= 0 || !icon_rgba) { return; }

    size_t icon_bytes = (size_t)width * (size_t)height * 4;
    gdmf_window_config_icon_rgba = (unsigned char*)malloc(icon_bytes);
    if (!gdmf_window_config_icon_rgba) { return; }

    memcpy(gdmf_window_config_icon_rgba, icon_rgba, icon_bytes);
    gdmf_window_config_icon_width  = width;
    gdmf_window_config_icon_height = height;

    return;
}

int GDMF_Init(void) {
    if (gdmf_vulkan_has_resources()) {
        printf("[GDMF] Resolve the previous renderer lifecycle before initializing\n");
        return -1;
    }
    printf("[GDMF] Version %s\n", GDMF_VERSION);

    if (gdmf_window_create() != 0) {
        printf("[GDMF] Window creation failed\n");

        return -1;
    }

    printf("[GDMF] Window ready (%dx%d)\n", gdmf_window_width, gdmf_window_height);

    if (gdmf_vulkan_init() != 0) {
        printf("[GDMF] Vulkan init failed\n");

        if (!gdmf_vulkan_shutdown()) {
            printf("[GDMF] Initialization cleanup requires renderer recovery\n");
            return -1;
        }
        gdmf_window_destroy();
        return -1;
    }

    gdmf_fps_sample_start_seconds = 0.0;
    gdmf_fps_presentation_count = 0;
    gdmf_current_fps = 0.0f;
    return 0;
}

void GDMF_Shutdown(void) {
    printf("[GDMF] Shutdown\n");
    // Stop work producers before shutdown. Workers and text resources need
    // to be released before destroying the renderer and its window.
    gdmf_workers_shutdown();
    if (!gdmf_vulkan_quiesce()) {
        printf("[GDMF] Shutdown requires renderer recovery; resources retained\n");
        return;
    }
    if (!gdmf_vulkan_shutdown()) {
        printf("[GDMF] Shutdown could not drain renderer resources\n");
        return;
    }

    gdmf_window_destroy();

    free(gdmf_window_config_icon_rgba);
    gdmf_window_config_icon_rgba = NULL;

    printf("[GDMF] Done\n");

    return;
}

bool GDMF_Tick(void) {
    // Includes connection maintenance on backends with their own event thread.
    gdmf_window_pump();
    return !gdmf_window_close_requested && !gdmf_vulkan_should_stop();
}

bool GDMF_AcquireFrame(void) {
    return gdmf_vulkan_acquire_frame();
}

bool GDMF_PrepareFrame(void) {
    return gdmf_vulkan_prepare_frame();
}

void GDMF_SubmitFrame(void) {
    bool presentation_accepted = gdmf_vulkan_submit_frame();
    double current_seconds = gdmf_window_now_seconds();
    if (gdmf_fps_sample_start_seconds == 0.0) {
        gdmf_fps_sample_start_seconds = current_seconds;
    }
    if (presentation_accepted) { gdmf_fps_presentation_count++; }
    double elapsed_seconds = current_seconds - gdmf_fps_sample_start_seconds;
    if (elapsed_seconds >= 0.5) {
        gdmf_current_fps = (float)(gdmf_fps_presentation_count / elapsed_seconds);
        gdmf_fps_presentation_count = 0;
        gdmf_fps_sample_start_seconds = current_seconds;
    }

    return;
}

float GDMF_GetCurrentFPS(void) {
    return gdmf_current_fps;
}

void GDMF_SetDisplayMode(GDMF_DisplayMode mode) {
    gdmf_window_request_display_mode(mode);

    return;
}

GDMF_DisplayMode GDMF_GetDisplayMode(void) {
    return gdmf_window_display_mode;
}

void GDMF_SetMouseCapture(bool capture) {
    gdmf_window_mouse_capture_requested = capture;
    gdmf_window_input_state_changed();

    return;
}

bool GDMF_GetMouseCapture(void) {
    return gdmf_window_mouse_capture_requested;
}

bool GDMF_ToggleMouseCapture(void) {
    bool capture_requested = !gdmf_window_mouse_capture_requested;

    GDMF_SetMouseCapture(capture_requested);

    return capture_requested;
}

void GDMF_SetCursorVisible(bool visible) {
    gdmf_window_cursor_visible_requested = visible;
    gdmf_window_input_state_changed();

    return;
}

bool GDMF_GetCursorVisible(void) {
    return gdmf_window_cursor_visible_requested;
}

// Invert the rendered viewport transform to obtain logical canvas coordinates.
void GDMF_GetMousePosition(float* mouse_x, float* mouse_y) {
    float canvas_x = 0.0f;
    float canvas_y = 0.0f;

    int client_x, client_y;
    if (gdmf_window_get_mouse_client(&client_x, &client_y)) {
        VkRect2D viewport = gdmf_get_render_viewport_rect();
        if (viewport.extent.width > 0 && viewport.extent.height > 0) {
            canvas_x = ((float)(client_x - viewport.offset.x) / (float)viewport.extent.width)
                     * (float)GDMF_GetCanvasWidth();
            canvas_y = ((float)(client_y - viewport.offset.y) / (float)viewport.extent.height)
                     * (float)GDMF_GetCanvasHeight();
        }
    }

    if (mouse_x) { *mouse_x = canvas_x; }
    if (mouse_y) { *mouse_y = canvas_y; }

    return;
}

int  GDMF_GetWidth(void)     { return gdmf_window_width; }
int  GDMF_GetHeight(void)    { return gdmf_window_height; }
bool GDMF_IsMinimized(void)  { return gdmf_window_minimized; }
bool GDMF_IsFocused(void)    { return gdmf_window_has_focus; }

int GDMF_GetAspectRatioNum(void) { return gdmf_window_config_aspect_numerator; }
int GDMF_GetAspectRatioDen(void) { return gdmf_window_config_aspect_denominator; }
int GDMF_GetCanvasWidth(void)  { return gdmf_canvas_width;  }
int GDMF_GetCanvasHeight(void) { return gdmf_canvas_height; }

bool GDMF_ResizeOccurred(void) {
    if (gdmf_window_resize_pending) {
        gdmf_window_resize_pending = false;
        return true;
    }

    return false;
}

