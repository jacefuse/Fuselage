// GDMF -- Win32 window backend.
// The OS window itself: a dedicated window thread (so OS modal loops --
// resize drag, move -- never stall the game loop), the wndproc, display
// mode switching, cursor capture/visibility, aspect ratio enforcement, and
// the window icon. Platform-neutral state and the public API live in
// gdmf.c; the interface between the two is gdmf_window.h.

#include "gdmf.h"
#include "gdmf_window.h"
#include <windows.h>
#include <stdio.h>

// Internal window messages
// Posted from any thread; handled on the window thread where it's safe.
#define GDMF_WM_SET_DISPLAY_MODE   (WM_APP + 1)
#define GDMF_WM_SHUTDOWN           (WM_APP + 2)
#define GDMF_WM_APPLY_INPUT_STATE  (WM_APP + 3)

// Display and Window
static RECT  g_savedWindowRect  = { 0 };
static DWORD g_savedWindowStyle = 0;

// Window-thread-internal bookkeeping, never read from other threads.
static bool g_cursorCurrentlyHidden = false;

// Win32 handles
static HWND      g_hWnd             = NULL;
static HINSTANCE g_hInstance        = NULL;
static HANDLE    g_windowThread     = NULL;
static HANDLE    g_windowReadyEvent = NULL;   // signals when HWND is valid

static HICON g_hIcon = NULL;

static DWORD WINAPI gdmf_window_thread(LPVOID param);
static LRESULT CALLBACK gdmf_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void gdmf_apply_display_mode(GDMF_DisplayMode mode);
static void gdmf_apply_input_state(void);
static void gdmf_enforce_aspect_ratio(RECT* rect, WPARAM edge);
static void gdmf_get_window_border_size(HWND hWnd, int* bw, int* bh);
static void gdmf_compute_aspect_corrected_size(HWND hWnd, int* cx, int* cy);
static HICON gdmf_create_icon_from_rgba(int width, int height, const unsigned char* rgba);

HWND GDMF_GetHWND(void) { return g_hWnd; }

void* GDMF_GetNativeWindowHandle(void) { return (void*)g_hWnd; }

// --- Backend interface (see gdmf_window.h) ----------------------------------

int gdmf_window_create(void) {
    g_hInstance = GetModuleHandleA(NULL);

    // Event used to wait until the window thread has created the HWND
    g_windowReadyEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_windowReadyEvent) {
        printf("[GDMF] Failed to create ready event\n");
        //tlPrint("[GDMF] Failed to create ready event");tlNewLine();

        return -1;
    }

    g_windowThread = CreateThread(NULL, 0, gdmf_window_thread, NULL, 0, NULL);
    if (!g_windowThread) {
        printf("[GDMF] Failed to create window thread\n");
        //tlPrint("[GDMF] Failed to create window thread");tlNewLine();

        CloseHandle(g_windowReadyEvent);
        g_windowReadyEvent = NULL;

        return -1;
    }

    // Wait for the window to be created before returning
    WaitForSingleObject(g_windowReadyEvent, INFINITE);
    CloseHandle(g_windowReadyEvent);
    g_windowReadyEvent = NULL;

    if (!g_hWnd) {
        printf("[GDMF] Window creation failed on window thread\n");
        //tlPrint("[GDMF] Window creation failed on window thread");tlNewLine();

        return -1;
    }

    return 0;
}

void gdmf_window_destroy(void) {
    if (g_hWnd) {
        // Tell the window thread to close cleanly
        PostMessageA(g_hWnd, GDMF_WM_SHUTDOWN, 0, 0);
    }

    if (g_windowThread) {
        WaitForSingleObject(g_windowThread, 4000);
        CloseHandle(g_windowThread);
        g_windowThread = NULL;
    }

    return;
}

void gdmf_window_pump(void) {
    // No-op: the dedicated window thread owns the message pump.
    return;
}

void gdmf_window_request_display_mode(GDMF_DisplayMode mode) {
    // Post to window thread - safe from any thread
    if (g_hWnd) {
        PostMessageA(g_hWnd, GDMF_WM_SET_DISPLAY_MODE, (WPARAM)mode, 0);
    }

    return;
}

void gdmf_window_input_state_changed(void) {
    // The desired state is already current in gdmf_st_mouseCaptureDesired /
    // gdmf_st_cursorVisibleDesired -- gdmf.c sets them before calling this.
    // Just have the window thread re-derive the actual effect.
    if (g_hWnd) {
        PostMessageA(g_hWnd, GDMF_WM_APPLY_INPUT_STATE, 0, 0);
    }

    return;
}

// GetCursorPos/ScreenToClient give the real client-area pixel the OS cursor
// is over -- gdmf.c maps it into reference-canvas space.
bool gdmf_window_get_mouse_client(int* x, int* y) {
    if (!g_hWnd) { return false; }

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(g_hWnd, &pt);

    *x = (int)pt.x;
    *y = (int)pt.y;

    return true;
}

double gdmf_window_now_seconds(void) {
    static LARGE_INTEGER freq = { 0 };
    if (freq.QuadPart == 0) { QueryPerformanceFrequency(&freq); }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (double)now.QuadPart / (double)freq.QuadPart;
}

// --- Window icon -------------------------------------------------------------

// Builds a 32-bit alpha-aware HICON from a top-down RGBA8 buffer. The AND
// mask is left all-zero (no pixels forced opaque-black) -- on XP+ a 32-bit
// color bitmap with a real alpha channel in ICONINFO is composited using
// that alpha directly, so the mask only matters as a required-but-unused
// legacy field here.
static HICON gdmf_create_icon_from_rgba(int width, int height, const unsigned char* rgba) {
    BITMAPV5HEADER bi;

    memset(&bi, 0, sizeof(bi));
    bi.bV5Size        = sizeof(BITMAPV5HEADER);
    bi.bV5Width       = width;
    bi.bV5Height      = -height;  // negative = top-down, matches rgba's row order
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    HDC hdc = GetDC(NULL);
    void* bits = NULL;
    HBITMAP hColor = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (!hColor || !bits) { return NULL; }

    unsigned char* dst = (unsigned char*)bits;
    for (int i = 0; i < width * height; i++) {
        dst[i * 4 + 0] = rgba[i * 4 + 2];  // B
        dst[i * 4 + 1] = rgba[i * 4 + 1];  // G
        dst[i * 4 + 2] = rgba[i * 4 + 0];  // R
        dst[i * 4 + 3] = rgba[i * 4 + 3];  // A
    }

    HBITMAP hMask = CreateBitmap(width, height, 1, 1, NULL);
    if (!hMask) {
        DeleteObject(hColor);
        return NULL;
    }

    ICONINFO ii;
    memset(&ii, 0, sizeof(ii));
    ii.fIcon    = TRUE;
    ii.hbmMask  = hMask;
    ii.hbmColor = hColor;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hColor);
    DeleteObject(hMask);

    return hIcon;
}

// --- Window thread -----------------------------------------------------------

static DWORD WINAPI gdmf_window_thread(LPVOID param) {
    (void)param;

    // Register window class
    WNDCLASSEXA wc   = { 0 };
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = gdmf_wndproc;
    wc.hInstance     = g_hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "GDMF_Window";

    if (!RegisterClassExA(&wc)) {
        printf("[GDMF] Window class registration failed\n");
        //tlPrint("[GDMF] Window class registration failed");tlNewLine();

        SetEvent(g_windowReadyEvent);

        return 1;
    }

    // Calculate centered window position
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect   = { 0, 0, gdmf_cfg_width, gdmf_cfg_height };
    AdjustWindowRect(&rect, style, FALSE);

    int w = rect.right  - rect.left;
    int h = rect.bottom - rect.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hWnd = CreateWindowExA(
        0, "GDMF_Window", gdmf_cfg_title,
        style,
        x, y, w, h,
        NULL, NULL, g_hInstance, NULL
    );

    if (!g_hWnd) {
        printf("[GDMF] CreateWindowEx failed\n");
        //tlPrint("[GDMF] CreateWindowEx failed");tlNewLine();

        SetEvent(g_windowReadyEvent);

        return 1;
    }

    gdmf_st_width  = gdmf_cfg_width;
    gdmf_st_height = gdmf_cfg_height;

    if (gdmf_cfg_iconRGBA && gdmf_cfg_iconWidth > 0 && gdmf_cfg_iconHeight > 0) {
        g_hIcon = gdmf_create_icon_from_rgba(gdmf_cfg_iconWidth, gdmf_cfg_iconHeight, gdmf_cfg_iconRGBA);
        if (g_hIcon) {
            SendMessageA(g_hWnd, WM_SETICON, ICON_BIG,   (LPARAM)g_hIcon);
            SendMessageA(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);
        }
    }

    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hWnd);

    // Keep display and system awake for the window's whole lifetime -- only
    // keyboard/mouse activity resets Windows' idle timers, so a game played
    // entirely on a controller would otherwise sleep the screen mid-play.
    // Per-thread state, so it lives on this thread (which owns the window
    // for exactly that lifetime) and is cleared before the thread exits.
    // (Added alongside the macOS port's IOPM assertion; not yet exercised
    // on a Windows machine -- verify with the rest of the seam refactor.)
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);

    // Apply whatever mouse capture / cursor visibility was requested before
    // the window existed (GDMF_SetMouseCapture/GDMF_SetCursorVisible record
    // the desired state immediately, but couldn't act on it until now).
    gdmf_st_hasFocus = true;
    gdmf_apply_input_state();

    // Signal the main thread that the window is ready
    SetEvent(g_windowReadyEvent);

    // Message pump - this thread owns the window for its lifetime
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    SetThreadExecutionState(ES_CONTINUOUS);   // restore normal idle behavior

    if (g_hIcon) {
        DestroyIcon(g_hIcon);
        g_hIcon = NULL;
    }

    g_hWnd = NULL;
    UnregisterClassA("GDMF_Window", g_hInstance);

    printf("[GDMF] Window thread exiting\n");
    //("[GDMF] Window thread exiting");tlNewLine();

    return 0;
}

// Window procedure (runs on window thread)
static LRESULT CALLBACK gdmf_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CLOSE:
        gdmf_st_closeRequested = true;

        return 0;

    case WM_DESTROY:
        // Release the cursor unconditionally before the window goes away --
        // otherwise a captured/hidden cursor would stay stuck after exit.
        ClipCursor(NULL);
        if (g_cursorCurrentlyHidden) {
            ShowCursor(TRUE);
            g_cursorCurrentlyHidden = false;
        }
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        int w = (int)LOWORD(lParam);
        int h = (int)HIWORD(lParam);
        if (wParam == SIZE_MINIMIZED) {
            gdmf_st_minimized = true;
        } else {
            bool wasMinimized = gdmf_st_minimized;

            gdmf_st_minimized = false;
            if (w > 0 && h > 0 && (wasMinimized || w != gdmf_st_width || h != gdmf_st_height)) {
                gdmf_st_width          = w;
                gdmf_st_height         = h;
                gdmf_st_resizeOccurred = true;
            }
        }
        gdmf_apply_input_state();  // re-clip to the new client rect, if captured
        return 0;
    }

    case WM_MOVE:
        gdmf_apply_input_state();  // re-clip to the new screen position, if captured
        return 0;

    case WM_SETFOCUS:
        gdmf_st_hasFocus = true;
        gdmf_apply_input_state();
        return 0;

    case WM_KILLFOCUS:
        // Release capture/hidden-cursor while unfocused so alt-tabbing away
        // never leaves the user's mouse stuck. Reapplied on WM_SETFOCUS above
        // if still desired.
        gdmf_st_hasFocus = false;
        gdmf_apply_input_state();
        return 0;

    case WM_SIZING: {
        // Enforce aspect ratio during interactive resize drag
        RECT* r = (RECT*)lParam;
        gdmf_enforce_aspect_ratio(r, wParam);
        return TRUE;
    }

    case WM_WINDOWPOSCHANGING: {
        // Catches resizes WM_SIZING never sees -- Snap, Snap Layouts,
        // Win+Arrow, or anything else that hands the window a final size
        // directly via SetWindowPos instead of dragging one edge at a time.
        // Skipped for pure moves (SWP_NOSIZE, where cx/cy are meaningless)
        // and while borderless/exclusive-fullscreen (WS_POPUP) -- those
        // modes deliberately cover the whole monitor regardless of aspect
        // ratio; letterboxing them is the renderer's job, not the window's.
        WINDOWPOS* wp = (WINDOWPOS*)lParam;
        DWORD style = (DWORD)GetWindowLongA(hWnd, GWL_STYLE);
        if (!(wp->flags & SWP_NOSIZE) && !(style & WS_POPUP)) { gdmf_compute_aspect_corrected_size(hWnd, &wp->cx, &wp->cy); }
        return 0;
    }

    case WM_DPICHANGED: {
        // Windows suggests a new rect to keep the same apparent size at the new DPI
        RECT* suggested = (RECT*)lParam;
        SetWindowPos(hWnd, NULL,
            suggested->left, suggested->top,
            suggested->right  - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 320;
        mmi->ptMinTrackSize.y = 180;
        return 0;
    }

    case GDMF_WM_SET_DISPLAY_MODE:
        gdmf_apply_display_mode((GDMF_DisplayMode)wParam);
        return 0;

    case GDMF_WM_APPLY_INPUT_STATE:
        // The desired state is already current -- gdmf.c sets it before
        // gdmf_window_input_state_changed posts this. Just re-derive the
        // actual effect.
        gdmf_apply_input_state();
        return 0;

    case GDMF_WM_SHUTDOWN:
        DestroyWindow(hWnd);
        return 0;

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
}

// Display mode implementation (always called on window thread)
static void gdmf_apply_display_mode(GDMF_DisplayMode mode) {
    if (mode == gdmf_st_displayMode) { return; }

    HWND hWnd = g_hWnd;

    if (gdmf_st_displayMode == GDMF_MODE_WINDOWED) {
        // Save windowed geometry before leaving it
        GetWindowRect(hWnd, &g_savedWindowRect);
        g_savedWindowStyle = (DWORD)GetWindowLongA(hWnd, GWL_STYLE);
    }

    switch (mode) {

    case GDMF_MODE_WINDOWED: {
        SetWindowLongA(hWnd, GWL_STYLE, g_savedWindowStyle);
        SetWindowPos(hWnd, HWND_TOP,
            g_savedWindowRect.left,
            g_savedWindowRect.top,
            g_savedWindowRect.right  - g_savedWindowRect.left,
            g_savedWindowRect.bottom - g_savedWindowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ShowWindow(hWnd, SW_NORMAL);

        printf("[GDMF] Display mode: WINDOWED\n");
        //tlNewLine();tlPrint("[GDMF] Display mode: WINDOWED");tlNewLine();


        break;
    }

    case GDMF_MODE_BORDERLESS: {
        // Cover the monitor the window currently lives on
        HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { .cbSize = sizeof(mi) };
        GetMonitorInfoA(mon, &mi);
        RECT mr = mi.rcMonitor;

        SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOP,
            mr.left, mr.top,
            mr.right - mr.left,
            mr.bottom - mr.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        printf("[GDMF] Display mode: BORDERLESS\n");
        //tlNewLine();tlPrint("[GDMF] Display mode: BORDERLESS");tlNewLine();

        break;
    }

    case GDMF_MODE_FULLSCREEN_EXCLUSIVE: {
        // Exclusive fullscreen via DEVMODE change.
        // Vulkan will use VK_PRESENT_MODE_IMMEDIATE_KHR when it comes back.
        HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { .cbSize = sizeof(mi) };
        GetMonitorInfoA(mon, &mi);
        RECT mr = mi.rcMonitor;

        DEVMODEA dm = { 0 };
        dm.dmSize       = sizeof(dm);
        dm.dmPelsWidth  = (DWORD)(mr.right  - mr.left);
        dm.dmPelsHeight = (DWORD)(mr.bottom - mr.top);
        dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN);

        SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOP,
            mr.left, mr.top,
            mr.right - mr.left,
            mr.bottom - mr.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        printf("[GDMF] Display mode: FULLSCREEN EXCLUSIVE\n");
        //tlNewLine();tlPrint("[GDMF] Display mode: FULLSCREEN EXCLUSIVE");tlNewLine();

        break;
    }

    }

    // Restore display settings when leaving exclusive fullscreen
    if (gdmf_st_displayMode == GDMF_MODE_FULLSCREEN_EXCLUSIVE && mode != GDMF_MODE_FULLSCREEN_EXCLUSIVE) {
        ChangeDisplaySettingsA(NULL, 0);
    }

    gdmf_st_displayMode = mode;

    return;
}

// Mouse capture / cursor visibility implementation (always called on window
// thread). Re-derives both effects from scratch every time instead of
// tracking deltas -- cheap, and only ever runs on focus/move/resize/toggle
// events, never per-frame.
static void gdmf_apply_input_state(void) {
    if (!g_hWnd) { return; }

    if (gdmf_st_hasFocus && gdmf_st_mouseCaptureDesired) {
        RECT clientRect;

        GetClientRect(g_hWnd, &clientRect);
        POINT topLeft     = { clientRect.left,  clientRect.top };
        POINT bottomRight = { clientRect.right, clientRect.bottom };
        ClientToScreen(g_hWnd, &topLeft);
        ClientToScreen(g_hWnd, &bottomRight);
        RECT screenRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
        ClipCursor(&screenRect);
    } else {
        ClipCursor(NULL);
    }

    bool shouldHideCursor = gdmf_st_hasFocus && !gdmf_st_cursorVisibleDesired;
    if (shouldHideCursor != g_cursorCurrentlyHidden) {
        ShowCursor(shouldHideCursor ? FALSE : TRUE);
        g_cursorCurrentlyHidden = shouldHideCursor;
    }

    return;
}

// --- Aspect ratio enforcement ------------------------------------------------

// Size, in pixels, of hWnd's non-client border (title bar + frame) -- the
// difference between its window rect and its client rect. Derived via
// AdjustWindowRect on a zeroed RECT rather than the window's actual current
// rect, since we only want the border's size, not its position.
static void gdmf_get_window_border_size(HWND hWnd, int* bw, int* bh) {
    DWORD style = (DWORD)GetWindowLongA(hWnd, GWL_STYLE);
    RECT border = { 0, 0, 0, 0 };

    AdjustWindowRect(&border, style, FALSE);
    *bw = -border.left + border.right;
    *bh = -border.top  + border.bottom;

    return;
}

static void gdmf_enforce_aspect_ratio(RECT* rect, WPARAM edge) {
    int bw, bh;

    gdmf_get_window_border_size(g_hWnd, &bw, &bh);

    int cw = (rect->right  - rect->left) - bw;
    int ch = (rect->bottom - rect->top)  - bh;

    // Decide which dimension is the anchor based on drag edge
    bool anchor_width =
        (edge == WMSZ_LEFT  || edge == WMSZ_RIGHT ||
         edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT ||
         edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT);

    if (anchor_width) {
        ch = (cw * gdmf_cfg_aspectDen) / gdmf_cfg_aspectNum;
    } else {
        cw = (ch * gdmf_cfg_aspectNum) / gdmf_cfg_aspectDen;
    }

    // Adjust the rect edge that was being dragged
    switch (edge) {
    case WMSZ_RIGHT:
    case WMSZ_BOTTOMRIGHT:
    case WMSZ_BOTTOM:
        rect->right  = rect->left + cw + bw;

        rect->bottom = rect->top  + ch + bh;
        break;
    case WMSZ_LEFT:
    case WMSZ_BOTTOMLEFT:
        rect->left   = rect->right  - cw - bw;
        rect->bottom = rect->top    + ch + bh;
        break;
    case WMSZ_TOP:
    case WMSZ_TOPRIGHT:
        rect->right = rect->left + cw + bw;
        rect->top   = rect->bottom - ch - bh;
        break;
    case WMSZ_TOPLEFT:
        rect->left = rect->right  - cw - bw;
        rect->top  = rect->bottom - ch - bh;
        break;
    }

    return;
}

// Same correction as gdmf_enforce_aspect_ratio, but for callers that propose
// a final window size outright (WM_WINDOWPOSCHANGING, e.g. Snap/Win+Arrow)
// instead of dragging a single edge -- so there's no "anchor edge" to key
// off of. Anchors on whichever dimension is already smaller relative to the
// target ratio and shrinks the other to match, so the corrected size is
// never larger than what was proposed (avoids the corrected window growing
// past the snapped region/screen it was just placed into).
static void gdmf_compute_aspect_corrected_size(HWND hWnd, int* cx, int* cy) {
    int bw, bh;

    gdmf_get_window_border_size(hWnd, &bw, &bh);

    int cw = *cx - bw;
    int ch = *cy - bh;
    if (cw <= 0 || ch <= 0) { return; }

    int heightForWidth = (cw * gdmf_cfg_aspectDen) / gdmf_cfg_aspectNum;
    if (heightForWidth <= ch) { ch = heightForWidth; }
    else { cw = (ch * gdmf_cfg_aspectNum) / gdmf_cfg_aspectDen; }

    *cx = cw + bw;
    *cy = ch + bh;

    return;
}
