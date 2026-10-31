// Win32 window backend. A dedicated thread owns the window and message loop.
// Shared configuration and state are declared in gdmf_window.h.

#include "../gdmf.h"
#include "gdmf_window.h"
#include <windows.h>
#include <stdio.h>

// Private messages handled by the window thread.
#define GDMF_WM_SET_DISPLAY_MODE   (WM_APP + 1)
#define GDMF_WM_SHUTDOWN           (WM_APP + 2)
#define GDMF_WM_APPLY_INPUT_STATE  (WM_APP + 3)

static RECT  gdmf_saved_window_rect  = { 0 };
static DWORD gdmf_saved_window_style = 0;

// Window-thread cursor visibility bookkeeping.
static bool gdmf_cursor_hidden = false;

static HWND      gdmf_window_handle = NULL;
static HINSTANCE gdmf_instance_handle = NULL;
static HANDLE    gdmf_window_thread_handle = NULL;
static HANDLE    gdmf_window_ready_event = NULL; // signals creation success or failure

static HICON gdmf_icon_handle = NULL;

static DWORD WINAPI gdmf_window_thread(LPVOID thread_parameter);
static LRESULT CALLBACK gdmf_window_procedure(HWND window_handle, UINT message, WPARAM word_parameter, LPARAM long_parameter);
static void gdmf_apply_display_mode(GDMF_DisplayMode mode);
static void gdmf_apply_input_state(void);
static void gdmf_enforce_aspect_ratio(RECT* rect, WPARAM edge);
static void gdmf_get_window_border_size(HWND window_handle, int* border_width, int* border_height);
static void gdmf_compute_aspect_corrected_size(HWND window_handle, int* window_width, int* window_height);
static HICON gdmf_create_icon_from_rgba(int width, int height, const unsigned char* icon_rgba);

void* GDMF_GetHWND(void) { return (void*)gdmf_window_handle; }

void* GDMF_GetNativeWindowHandle(void) { return (void*)gdmf_window_handle; }

int gdmf_window_create(void) {
    gdmf_instance_handle = GetModuleHandleA(NULL);
    gdmf_window_ready_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!gdmf_window_ready_event) {
        printf("[GDMF] Failed to create ready event\n");

        return -1;
    }

    gdmf_window_thread_handle = CreateThread(NULL, 0, gdmf_window_thread, NULL, 0, NULL);
    if (!gdmf_window_thread_handle) {
        printf("[GDMF] Failed to create window thread\n");

        CloseHandle(gdmf_window_ready_event);
        gdmf_window_ready_event = NULL;

        return -1;
    }
    // Creation currently waits without a timeout; wait failures are not handled.
    WaitForSingleObject(gdmf_window_ready_event, INFINITE);
    CloseHandle(gdmf_window_ready_event);
    gdmf_window_ready_event = NULL;

    if (!gdmf_window_handle) {
        printf("[GDMF] Window creation failed on window thread\n");

        return -1;
    }

    return 0;
}

void gdmf_window_destroy(void) {
    if (gdmf_window_handle) {
        PostMessageA(gdmf_window_handle, GDMF_WM_SHUTDOWN, 0, 0);
    }

    if (gdmf_window_thread_handle) {
        // Current behavior closes the handle even if this wait times out.
        WaitForSingleObject(gdmf_window_thread_handle, 4000);
        CloseHandle(gdmf_window_thread_handle);
        gdmf_window_thread_handle = NULL;
    }

    return;
}

// The window thread dispatches messages; no main-thread pumping is needed.
void gdmf_window_pump(void) {
    return;
}

void gdmf_window_request_display_mode(GDMF_DisplayMode mode) {
    if (gdmf_window_handle) {
        PostMessageA(gdmf_window_handle, GDMF_WM_SET_DISPLAY_MODE, (WPARAM)mode, 0);
    }

    return;
}

void gdmf_window_input_state_changed(void) {
    if (gdmf_window_handle) {
        PostMessageA(gdmf_window_handle, GDMF_WM_APPLY_INPUT_STATE, 0, 0);
    }

    return;
}
bool gdmf_window_get_mouse_client(int* mouse_x, int* mouse_y) {
    if (!gdmf_window_handle) { return false; }

    POINT cursor_position;
    GetCursorPos(&cursor_position);
    ScreenToClient(gdmf_window_handle, &cursor_position);

    *mouse_x = (int)cursor_position.x;
    *mouse_y = (int)cursor_position.y;

    return true;
}

double gdmf_window_now_seconds(void) {
    static LARGE_INTEGER counter_frequency = { 0 };
    if (counter_frequency.QuadPart == 0) { QueryPerformanceFrequency(&counter_frequency); }

    LARGE_INTEGER counter_value;
    QueryPerformanceCounter(&counter_value);

    return (double)counter_value.QuadPart / (double)counter_frequency.QuadPart;
}
static HICON gdmf_create_icon_from_rgba(int width, int height, const unsigned char* icon_rgba) {
    BITMAPV5HEADER bitmap_header;

    memset(&bitmap_header, 0, sizeof(bitmap_header));
    bitmap_header.bV5Size        = sizeof(BITMAPV5HEADER);
    bitmap_header.bV5Width       = width;
    bitmap_header.bV5Height      = -height;  // top-down rows  // negative = top-down, matches icon_rgba's row order
    bitmap_header.bV5Planes      = 1;
    bitmap_header.bV5BitCount    = 32;
    bitmap_header.bV5Compression = BI_BITFIELDS;
    bitmap_header.bV5RedMask     = 0x00FF0000;
    bitmap_header.bV5GreenMask   = 0x0000FF00;
    bitmap_header.bV5BlueMask    = 0x000000FF;
    bitmap_header.bV5AlphaMask   = 0xFF000000;

    HDC device_context = GetDC(NULL);
    void* bitmap_pixels = NULL;
    HBITMAP color_bitmap = CreateDIBSection(device_context, (BITMAPINFO*)&bitmap_header, DIB_RGB_COLORS, &bitmap_pixels, NULL, 0);
    ReleaseDC(NULL, device_context);
    if (!color_bitmap || !bitmap_pixels) { return NULL; }

    // Convert source RGBA bytes to the bitmap's BGRA byte order.
    unsigned char* destination_pixels = (unsigned char*)bitmap_pixels;
    for (int pixel_index = 0; pixel_index < width * height; pixel_index++) {
        destination_pixels[pixel_index * 4 + 0] = icon_rgba[pixel_index * 4 + 2];  // B
        destination_pixels[pixel_index * 4 + 1] = icon_rgba[pixel_index * 4 + 1];  // G
        destination_pixels[pixel_index * 4 + 2] = icon_rgba[pixel_index * 4 + 0];  // R
        destination_pixels[pixel_index * 4 + 3] = icon_rgba[pixel_index * 4 + 3];  // A
    }

    HBITMAP mask_bitmap = CreateBitmap(width, height, 1, 1, NULL);
    if (!mask_bitmap) {
        DeleteObject(color_bitmap);
        return NULL;
    }

    ICONINFO icon_info;
    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.fIcon    = TRUE;
    icon_info.hbmMask  = mask_bitmap;
    icon_info.hbmColor = color_bitmap;

    HICON icon_handle = CreateIconIndirect(&icon_info);

    DeleteObject(color_bitmap);
    DeleteObject(mask_bitmap);

    return icon_handle;
}

static DWORD WINAPI gdmf_window_thread(LPVOID thread_parameter) {
    (void)thread_parameter;
    WNDCLASSEXA window_class   = { 0 };
    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = gdmf_window_procedure;
    window_class.hInstance     = gdmf_instance_handle;
    window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    // The executable's primary icon is the default; configured RGBA icons take precedence below.
    window_class.hIcon = (HICON)LoadImageA(gdmf_instance_handle, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                          GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
    window_class.hIconSm = (HICON)LoadImageA(gdmf_instance_handle, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    window_class.lpszClassName = "GDMF_Window";

    if (!RegisterClassExA(&window_class)) {
        printf("[GDMF] Window class registration failed\n");

        SetEvent(gdmf_window_ready_event);

        return 1;
    }
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect   = { 0, 0, gdmf_window_config_width, gdmf_window_config_height };
    AdjustWindowRect(&rect, style, FALSE);

    int window_width = rect.right  - rect.left;
    int window_height = rect.bottom - rect.top;
    int window_x = (GetSystemMetrics(SM_CXSCREEN) - window_width) / 2;
    int window_y = (GetSystemMetrics(SM_CYSCREEN) - window_height) / 2;

    gdmf_window_handle = CreateWindowExA(
        0, "GDMF_Window", gdmf_window_config_title,
        style,
        window_x, window_y, window_width, window_height,
        NULL, NULL, gdmf_instance_handle, NULL
    );

    if (!gdmf_window_handle) {
        printf("[GDMF] CreateWindowEx failed\n");

        SetEvent(gdmf_window_ready_event);

        return 1;
    }

    gdmf_window_width  = gdmf_window_config_width;
    gdmf_window_height = gdmf_window_config_height;

    if (gdmf_window_config_icon_rgba && gdmf_window_config_icon_width > 0 && gdmf_window_config_icon_height > 0) {
        gdmf_icon_handle = gdmf_create_icon_from_rgba(gdmf_window_config_icon_width, gdmf_window_config_icon_height, gdmf_window_config_icon_rgba);
        if (gdmf_icon_handle) {
            SendMessageA(gdmf_window_handle, WM_SETICON, ICON_BIG,   (LPARAM)gdmf_icon_handle);
            SendMessageA(gdmf_window_handle, WM_SETICON, ICON_SMALL, (LPARAM)gdmf_icon_handle);
        }
    }

    ShowWindow(gdmf_window_handle, SW_SHOWNORMAL);
    UpdateWindow(gdmf_window_handle);
    // Keep controller-only sessions from triggering display/system idle sleep.
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    gdmf_window_has_focus = true;
    gdmf_apply_input_state();
    SetEvent(gdmf_window_ready_event);
    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    SetThreadExecutionState(ES_CONTINUOUS);   // restore normal idle behavior

    if (gdmf_icon_handle) {
        DestroyIcon(gdmf_icon_handle);
        gdmf_icon_handle = NULL;
    }

    gdmf_window_handle = NULL;
    UnregisterClassA("GDMF_Window", gdmf_instance_handle);

    printf("[GDMF] Window thread exiting\n");

    return 0;
}
static LRESULT CALLBACK gdmf_window_procedure(HWND window_handle, UINT message, WPARAM word_parameter, LPARAM long_parameter) {
    switch (message) {

    case WM_CLOSE:
        gdmf_window_close_requested = true;

        return 0;

    case WM_DESTROY:
        // Restore cursor freedom and visibility before the window disappears.
        ClipCursor(NULL);
        if (gdmf_cursor_hidden) {
            ShowCursor(TRUE);
            gdmf_cursor_hidden = false;
        }
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        int window_width = (int)LOWORD(long_parameter);
        int window_height = (int)HIWORD(long_parameter);
        if (word_parameter == SIZE_MINIMIZED) {
            gdmf_window_minimized = true;
        } else {
            bool was_minimized = gdmf_window_minimized;

            gdmf_window_minimized = false;
            if (window_width > 0 && window_height > 0 && (was_minimized || window_width != gdmf_window_width || window_height != gdmf_window_height)) {
                gdmf_window_width          = window_width;
                gdmf_window_height         = window_height;
                gdmf_window_resize_pending = true;
            }
        }
        gdmf_apply_input_state();  // re-clip to the new client rect, if captured
        return 0;
    }

    case WM_MOVE:
        gdmf_apply_input_state();  // re-clip to the new screen position, if captured
        return 0;

    case WM_SETFOCUS:
        gdmf_window_has_focus = true;
        gdmf_apply_input_state();
        return 0;

    case WM_KILLFOCUS:
        gdmf_window_has_focus = false;
        gdmf_apply_input_state();
        return 0;

    case WM_SIZING: {
        RECT* sizing_rect = (RECT*)long_parameter;
        gdmf_enforce_aspect_ratio(sizing_rect, word_parameter);
        return TRUE;
    }

    case WM_WINDOWPOSCHANGING: {
        // Correct Snap/programmatic resizes, but leave fullscreen coverage intact.
        WINDOWPOS* window_position = (WINDOWPOS*)long_parameter;
        DWORD style = (DWORD)GetWindowLongA(window_handle, GWL_STYLE);
        if (!(window_position->flags & SWP_NOSIZE) && !(style & WS_POPUP)) { gdmf_compute_aspect_corrected_size(window_handle, &window_position->cx, &window_position->cy); }
        return 0;
    }

    case WM_DPICHANGED: {
        // Apply the position and size suggested for the new DPI.
        RECT* suggested = (RECT*)long_parameter;
        SetWindowPos(window_handle, NULL,
            suggested->left, suggested->top,
            suggested->right  - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* size_limits = (MINMAXINFO*)long_parameter;
        size_limits->ptMinTrackSize.x = 320;
        size_limits->ptMinTrackSize.y = 180;
        return 0;
    }

    case GDMF_WM_SET_DISPLAY_MODE:
        gdmf_apply_display_mode((GDMF_DisplayMode)word_parameter);
        return 0;

    case GDMF_WM_APPLY_INPUT_STATE:
        gdmf_apply_input_state();
        return 0;

    case GDMF_WM_SHUTDOWN:
        DestroyWindow(window_handle);
        return 0;

    default:
        return DefWindowProcA(window_handle, message, word_parameter, long_parameter);
    }
}
// Runs on the window thread. OS switch failures are not yet reflected in state.
static void gdmf_apply_display_mode(GDMF_DisplayMode mode) {
    if (mode == gdmf_window_display_mode) { return; }

    HWND window_handle = gdmf_window_handle;

    if (gdmf_window_display_mode == GDMF_MODE_WINDOWED) {
        GetWindowRect(window_handle, &gdmf_saved_window_rect);
        gdmf_saved_window_style = (DWORD)GetWindowLongA(window_handle, GWL_STYLE);
    }

    switch (mode) {

    case GDMF_MODE_WINDOWED: {
        SetWindowLongA(window_handle, GWL_STYLE, gdmf_saved_window_style);
        SetWindowPos(window_handle, HWND_TOP,
            gdmf_saved_window_rect.left,
            gdmf_saved_window_rect.top,
            gdmf_saved_window_rect.right  - gdmf_saved_window_rect.left,
            gdmf_saved_window_rect.bottom - gdmf_saved_window_rect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ShowWindow(window_handle, SW_NORMAL);

        printf("[GDMF] Display mode: WINDOWED\n");

        break;
    }

    case GDMF_MODE_BORDERLESS: {
        HMONITOR monitor_handle = MonitorFromWindow(window_handle, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor_info = { .cbSize = sizeof(monitor_info) };
        GetMonitorInfoA(monitor_handle, &monitor_info);
        RECT monitor_rect = monitor_info.rcMonitor;

        SetWindowLongA(window_handle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(window_handle, HWND_TOP,
            monitor_rect.left, monitor_rect.top,
            monitor_rect.right - monitor_rect.left,
            monitor_rect.bottom - monitor_rect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        printf("[GDMF] Display mode: BORDERLESS\n");

        break;
    }

    case GDMF_MODE_FULLSCREEN_EXCLUSIVE: {
        HMONITOR monitor_handle = MonitorFromWindow(window_handle, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor_info = { .cbSize = sizeof(monitor_info) };
        GetMonitorInfoA(monitor_handle, &monitor_info);
        RECT monitor_rect = monitor_info.rcMonitor;

        DEVMODEA display_mode = { 0 };
        display_mode.dmSize       = sizeof(display_mode);
        display_mode.dmPelsWidth  = (DWORD)(monitor_rect.right  - monitor_rect.left);
        display_mode.dmPelsHeight = (DWORD)(monitor_rect.bottom - monitor_rect.top);
        display_mode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettingsA(&display_mode, CDS_FULLSCREEN);

        SetWindowLongA(window_handle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(window_handle, HWND_TOP,
            monitor_rect.left, monitor_rect.top,
            monitor_rect.right - monitor_rect.left,
            monitor_rect.bottom - monitor_rect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        printf("[GDMF] Display mode: FULLSCREEN EXCLUSIVE\n");

        break;
    }

    }
    if (gdmf_window_display_mode == GDMF_MODE_FULLSCREEN_EXCLUSIVE && mode != GDMF_MODE_FULLSCREEN_EXCLUSIVE) {
        ChangeDisplaySettingsA(NULL, 0);
    }

    gdmf_window_display_mode = mode;

    return;
}
// Reapply requested input settings according to the current focus state.
static void gdmf_apply_input_state(void) {
    if (!gdmf_window_handle) { return; }

    if (gdmf_window_has_focus && gdmf_window_mouse_capture_requested) {
        RECT client_rect;

        GetClientRect(gdmf_window_handle, &client_rect);
        POINT top_left     = { client_rect.left,  client_rect.top };
        POINT bottom_right = { client_rect.right, client_rect.bottom };
        ClientToScreen(gdmf_window_handle, &top_left);
        ClientToScreen(gdmf_window_handle, &bottom_right);
        RECT screen_rect = { top_left.x, top_left.y, bottom_right.x, bottom_right.y };
        ClipCursor(&screen_rect);
    } else {
        ClipCursor(NULL);
    }

    bool hide_cursor = gdmf_window_has_focus && !gdmf_window_cursor_visible_requested;
    if (hide_cursor != gdmf_cursor_hidden) {
        ShowCursor(hide_cursor ? FALSE : TRUE);
        gdmf_cursor_hidden = hide_cursor;
    }

    return;
}
static void gdmf_get_window_border_size(HWND window_handle, int* border_width, int* border_height) {
    DWORD style = (DWORD)GetWindowLongA(window_handle, GWL_STYLE);
    RECT border = { 0, 0, 0, 0 };

    AdjustWindowRect(&border, style, FALSE);
    *border_width = -border.left + border.right;
    *border_height = -border.top  + border.bottom;

    return;
}

static void gdmf_enforce_aspect_ratio(RECT* rect, WPARAM edge) {
    int border_width, border_height;

    gdmf_get_window_border_size(gdmf_window_handle, &border_width, &border_height);

    int client_width = (rect->right  - rect->left) - border_width;
    int client_height = (rect->bottom - rect->top)  - border_height;
    bool anchor_width =
        (edge == WMSZ_LEFT  || edge == WMSZ_RIGHT ||
         edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT ||
         edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT);

    if (anchor_width) {
        client_height = (client_width * gdmf_window_config_aspect_denominator) / gdmf_window_config_aspect_numerator;
    } else {
        client_width = (client_height * gdmf_window_config_aspect_numerator) / gdmf_window_config_aspect_denominator;
    }
    switch (edge) {
    case WMSZ_RIGHT:
    case WMSZ_BOTTOMRIGHT:
    case WMSZ_BOTTOM:
        rect->right  = rect->left + client_width + border_width;

        rect->bottom = rect->top  + client_height + border_height;
        break;
    case WMSZ_LEFT:
    case WMSZ_BOTTOMLEFT:
        rect->left   = rect->right  - client_width - border_width;
        rect->bottom = rect->top    + client_height + border_height;
        break;
    case WMSZ_TOP:
    case WMSZ_TOPRIGHT:
        rect->right = rect->left + client_width + border_width;
        rect->top   = rect->bottom - client_height - border_height;
        break;
    case WMSZ_TOPLEFT:
        rect->left = rect->right  - client_width - border_width;
        rect->top  = rect->bottom - client_height - border_height;
        break;
    }

    return;
}
// Fit within the proposed size, preserving aspect without enlarging its bounds.
static void gdmf_compute_aspect_corrected_size(HWND window_handle, int* window_width, int* window_height) {
    int border_width, border_height;

    gdmf_get_window_border_size(window_handle, &border_width, &border_height);

    int client_width = *window_width - border_width;
    int client_height = *window_height - border_height;
    if (client_width <= 0 || client_height <= 0) { return; }

    int height_for_width = (client_width * gdmf_window_config_aspect_denominator) / gdmf_window_config_aspect_numerator;
    if (height_for_width <= client_height) { client_height = height_for_width; }
    else { client_width = (client_height * gdmf_window_config_aspect_numerator) / gdmf_window_config_aspect_denominator; }

    *window_width = client_width + border_width;
    *window_height = client_height + border_height;

    return;
}
