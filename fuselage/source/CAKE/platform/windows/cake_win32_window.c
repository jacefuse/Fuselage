// Windows input window and lifecycle. SEE: (CAKE.md, Windows input)
#include "cake_win32_private.h"
#if defined(_WIN32)
#include <dbt.h>
#include <stdio.h>

#define CAKE_WIN32_DISCOVERY_TIMER_ID 1
#define CAKE_WIN32_DISCOVERY_INTERVAL_MS 2000
static HWND cake_win32_input_window;
static HWND cake_win32_attached_window;
static HDEVNOTIFY cake_win32_device_notification;
static bool cake_win32_initialized;
static bool cake_win32_silenced;
static DWORD cake_win32_owner_thread;
static HANDLE cake_win32_owner_handle;
const GUID cake_win32_hid_guid = {
    0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};
static LRESULT CALLBACK cake_win32_window_proc(HWND window, UINT message, WPARAM word_parameter,
                                               LPARAM long_parameter) {
    switch (message) {
    case WM_INPUT: {

        cake_win32_dispatch_raw_input((HRAWINPUT)long_parameter, cake_win32_silenced);
        return 0;
    }

    case WM_DEVICECHANGE:
        if (word_parameter == DBT_DEVICEARRIVAL || word_parameter == DBT_DEVICEREMOVECOMPLETE) {

            cake_win32_refresh_controller_discovery();
        }

        break;

    case WM_TIMER:
        if (word_parameter == CAKE_WIN32_DISCOVERY_TIMER_ID) {
            cake_win32_scan_controllers();
        }
        break;
    }

    return DefWindowProcA(window, message, word_parameter, long_parameter);
}

static bool cake_win32_initialize(void) {

    WNDCLASSEXA window_class = {0};
    window_class.cbSize = sizeof(WNDCLASSEXA);
    window_class.lpfnWndProc = cake_win32_window_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "CAKE_Window";
    RegisterClassExA(&window_class);

    cake_win32_input_window = CreateWindowExA(0, "CAKE_Window", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
                                              GetModuleHandleA(NULL), NULL);

    RAWINPUTDEVICE input_devices[4];

    input_devices[0].usUsagePage = 0x01;
    input_devices[0].usUsage = 0x06;
    input_devices[0].dwFlags = RIDEV_INPUTSINK;
    input_devices[0].hwndTarget = cake_win32_input_window;

    input_devices[1].usUsagePage = 0x01;
    input_devices[1].usUsage = 0x02;
    input_devices[1].dwFlags = RIDEV_INPUTSINK;
    input_devices[1].hwndTarget = cake_win32_input_window;

    input_devices[2].usUsagePage = 0x01;
    input_devices[2].usUsage = 0x04;
    input_devices[2].dwFlags = RIDEV_INPUTSINK;
    input_devices[2].hwndTarget = cake_win32_input_window;

    input_devices[3].usUsagePage = 0x01;
    input_devices[3].usUsage = 0x05;
    input_devices[3].dwFlags = RIDEV_INPUTSINK;
    input_devices[3].hwndTarget = cake_win32_input_window;

    if (!cake_win32_input_window) {
        fprintf(stderr, "[CAKE] Input window creation failed\n");
        return false;
    }
    cake_win32_owner_thread = GetCurrentThreadId();
    cake_win32_owner_handle = OpenThread(SYNCHRONIZE, FALSE, cake_win32_owner_thread);
    if (!RegisterRawInputDevices(input_devices, 4, sizeof(RAWINPUTDEVICE))) {
        fprintf(stderr, "[CAKE] Raw Input registration failed\n");
    }

    DEV_BROADCAST_DEVICEINTERFACE_A notification_filter = {0};
    notification_filter.dbcc_size = sizeof(notification_filter);
    notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notification_filter.dbcc_classguid = cake_win32_hid_guid;

    cake_win32_device_notification = RegisterDeviceNotificationA(
        cake_win32_input_window, &notification_filter, DEVICE_NOTIFY_WINDOW_HANDLE);

    SetTimer(cake_win32_input_window, CAKE_WIN32_DISCOVERY_TIMER_ID, CAKE_WIN32_DISCOVERY_INTERVAL_MS, NULL);

    cake_win32_initialized = true;

    printf("[CAKE] Initialized. Scanning for controllers...\n");
    cake_win32_initialize_controllers();

    return true;
}

void CAKE_AttachWindow(void* native_window) {

    cake_win32_attached_window = (HWND)native_window;

    return;
}

void CAKE_Poll(void) {
    CAKE_PollCount++;
    if (cake_win32_initialized && cake_win32_owner_thread != GetCurrentThreadId()) {
        fprintf(stderr, "[CAKE] Poll must use the input-window thread\n");
        return;
    }
    if (!cake_win32_initialized && !cake_win32_initialize()) {
        return;
    }

    CAKE_MouseDeltaX = 0;
    CAKE_MouseDeltaY = 0;
    CAKE_MouseWheel = 0;

    MSG message;
    while (PeekMessageA(&message, cake_win32_input_window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    cake_win32_flush_left_shift();
    cake_publish_input_edges();
    cake_win32_poll_controllers();
    cake_derive_controller_edges();

    return;
}

static bool cake_win32_owner_exited(void) {
    return cake_win32_owner_handle && WaitForSingleObject(cake_win32_owner_handle, 0) == WAIT_OBJECT_0;
}

void CAKE_Shutdown(void) {
    if (!cake_win32_initialized) {
        return;
    }
    /* A joined polling thread took its window and timer with it; release the rest.
       Asked of the thread handle: a later thread can reuse the owner's ID. */
    bool orphaned = cake_win32_owner_exited();
    if (!orphaned && cake_win32_owner_thread != GetCurrentThreadId()) {
        fprintf(stderr, "[CAKE] Shutdown must use the input-window thread\n");
        return;
    }

    if (!orphaned) {
        KillTimer(cake_win32_input_window, CAKE_WIN32_DISCOVERY_TIMER_ID);
    }

    if (cake_win32_device_notification) {
        if (!UnregisterDeviceNotification(cake_win32_device_notification)) {
            fprintf(stderr, "[CAKE] Device notification cleanup failed\n");
            return;
        }
        cake_win32_device_notification = NULL;
    }

    if (cake_win32_input_window) {
        if (!orphaned && !DestroyWindow(cake_win32_input_window)) {
            fprintf(stderr, "[CAKE] Input window cleanup failed\n");
            return;
        }
        cake_win32_input_window = NULL;
    }

    cake_win32_shutdown_controllers();
    if (cake_win32_owner_handle) {
        CloseHandle(cake_win32_owner_handle);
        cake_win32_owner_handle = NULL;
    }
    cake_win32_owner_thread = 0;

    cake_win32_initialized = false;

    return;
}

void CAKE_Silence(void) {
    cake_win32_silenced = true;

    return;
}

void CAKE_Resume(void) {
    cake_win32_silenced = false;

    cake_win32_clear_pending_keyboard();
    cake_clear_keyboard_mouse_state();

    return;
}

bool CAKE_IsSilenced(void) {
    return cake_win32_silenced;
}

#endif
