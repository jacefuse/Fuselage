// cake.c - CAKE input subsystem - COLON 2026

// Linux builds with strict -std=c11; the POSIX pieces the Linux section
// uses (clock_gettime, pthread, dirent) must be requested before any
// header is pulled in, so this sits above the first #include.
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "cake.h"

#include <stdio.h>
#include <string.h>

bool CAKE_Keys[CAKE_KEY_TABLE_SIZE] =                { 0 };
bool CAKE_MouseButtons[CAKE_MOUSE_BUTTON_COUNT] =    { 0 };
int32_t CAKE_MouseDeltaX = 0;
int32_t CAKE_MouseDeltaY = 0;
int32_t CAKE_MouseWheel = 0;

// Windows Eventually moved to its own file
#if defined(_WIN32)

#include <dbt.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

// Internal controller types
typedef struct {
    int                         xinputIndex;
    uint16_t                    vendorId;
    uint16_t                    productId;
    CAKE_ControllerBackend      backend;
    XINPUT_CAPABILITIES          xcapabilities;
    XINPUT_BATTERY_INFORMATION   battery;
} cake_ControllerIdentity;

typedef struct {
    char                        deviceId[CAKE_CONTROLLER_PATH_MAX];
    char                        name[CAKE_CONTROLLER_NAME_MAX];
    int                         chainIndex;
    uint64_t                    timeAttached;
    uint64_t                    timeLastSeen;
    uint64_t                    timeLost;
    bool                        identityValid;
    cake_ControllerIdentity     identity;
    CAKE_ControllerState        state[2];
    volatile int                front;
    // For double buffered input; Front swaps after every poll;
    // state[1-front] is the back buffer; state[front] is always safe to read;
} cake_ControllerDevice;

// Fallback hotplug re-enumeration timer (in case a device-change
// notification is ever missed) -- see cake_wndproc's WM_TIMER.
#define CAKE_TIMER_ID                   1
#define CAKE_TIMER_MS                   2000

static HWND       cake_hwnd             = NULL;
static HWND       cake_attached_hwnd    = NULL;  // see CAKE_AttachWindow
static HDEVNOTIFY cake_notify            = NULL;
static bool        cake_initialized      = false;
static bool        cake_silenced         = false; // see CAKE_Silence/CAKE_Resume

static cake_ControllerDevice cake_controllers[CAKE_CONTROLLER_MAX];

// Decoupled from cake_controllers[] so a TIMEDOUT code survives the memset
// that frees the slot for reuse below -- otherwise it would be erased in the
// same pass that sets it, and would never be observable by a caller.
static CAKE_ControllerConnectionState cake_controller_connstate[CAKE_CONTROLLER_MAX];

static const GUID cake_hid_guid = {
    0x4D1E55B2, 0xF16F, 0x11CF,
    { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 }
};

// PS/2 Set 1 to USB HID Translation
static const uint8_t scan_to_hid[256] = {
    [0x01]=0x29,[0x02]=0x1E,[0x03]=0x1F,[0x04]=0x20,[0x05]=0x21,[0x06]=0x22,
    [0x07]=0x23,[0x08]=0x24,[0x09]=0x25,[0x0A]=0x26,[0x0B]=0x27,[0x0C]=0x2D,
    [0x0D]=0x2E,[0x0E]=0x2A,[0x0F]=0x2B,[0x10]=0x14,[0x11]=0x1A,[0x12]=0x08,
    [0x13]=0x15,[0x14]=0x17,[0x15]=0x1C,[0x16]=0x18,[0x17]=0x0C,[0x18]=0x12,
    [0x19]=0x13,[0x1A]=0x2F,[0x1B]=0x30,[0x1C]=0x28,[0x1D]=0xE0,[0x1E]=0x04,
    [0x1F]=0x16,[0x20]=0x07,[0x21]=0x09,[0x22]=0x0A,[0x23]=0x0B,[0x24]=0x0D,
    [0x25]=0x0E,[0x26]=0x0F,[0x27]=0x33,[0x28]=0x34,[0x29]=0x35,[0x2A]=0xE1,
    [0x2B]=0x31,[0x2C]=0x1D,[0x2D]=0x1B,[0x2E]=0x06,[0x2F]=0x19,[0x30]=0x05,
    [0x31]=0x11,[0x32]=0x10,[0x33]=0x36,[0x34]=0x37,[0x35]=0x38,[0x36]=0xE5,
    [0x37]=0x55,[0x38]=0xE2,[0x39]=0x2C,[0x3A]=0x39,[0x3B]=0x3A,[0x3C]=0x3B,
    [0x3D]=0x3C,[0x3E]=0x3D,[0x3F]=0x3E,[0x40]=0x3F,[0x41]=0x40,[0x42]=0x41,
    [0x43]=0x42,[0x44]=0x43,[0x45]=0x53,[0x46]=0x47,[0x47]=0x5F,[0x48]=0x60,
    [0x49]=0x61,[0x4A]=0x56,[0x4B]=0x5C,[0x4C]=0x5D,[0x4D]=0x5E,[0x4E]=0x57,
    [0x4F]=0x59,[0x50]=0x5A,[0x51]=0x5B,[0x52]=0x62,[0x53]=0x63,[0x56]=0x64,
    [0x57]=0x44,[0x58]=0x45
};

static const uint8_t scan_to_hid_e0[256] = {
    [0x1C]=0x58,[0x1D]=0xE4,[0x35]=0x54,[0x37]=0x46,[0x38]=0xE6,[0x47]=0x4A,
    [0x48]=0x52,[0x49]=0x4B,[0x4B]=0x50,[0x4D]=0x4F,[0x4F]=0x4D,[0x50]=0x51,
    [0x51]=0x4E,[0x52]=0x49,[0x53]=0x4C,[0x5B]=0xE3,[0x5C]=0xE7,[0x5D]=0x65
};

// Left-shift deferred-commit state. Windows injects a *fake* LShift make/break
// around certain E0 (extended) keys; to avoid a phantom shift, we hold a bare
// LShift as "pending" rather than committing it immediately, then cancel it if
// the very next event is an E0 key (real shift is re-read via GetAsyncKeyState).
// See cake_handle_keyboard.
static bool    cake_lshift_pending       = false;
static bool    cake_lshift_pending_valid = false;

// Keyboard
static void cake_flush_lshift(void) {
    if (cake_lshift_pending_valid) {
        CAKE_Keys[0xE1]           = cake_lshift_pending;
        cake_lshift_pending_valid = false;
    }

    return;
}

static void cake_handle_keyboard(RAWKEYBOARD *keyboard) {
    uint16_t    scancode    = keyboard->MakeCode;
    bool         is_e0       = (keyboard->Flags & RI_KEY_E0)     != 0;
    bool         is_e1       = (keyboard->Flags & RI_KEY_E1)     != 0;
    bool         is_break    = (keyboard->Flags & RI_KEY_BREAK)  != 0;
    bool        state       = !is_break;

    if (is_e1 && scancode == 0x45) {
        CAKE_Keys[0x48]     = state;
        return;
    }

    if (scancode == 0x2A && !is_e0) {
        cake_flush_lshift();
        cake_lshift_pending         = state;
        cake_lshift_pending_valid   = true;
        return;
    }

    if (is_e0) {
        if (cake_lshift_pending_valid){
            cake_lshift_pending_valid = false;
            if (GetAsyncKeyState(VK_LSHIFT) & 0x8000){
                CAKE_Keys[0xE1] = true;
            }
        }
    }
    else {
        cake_flush_lshift();
    }

    uint8_t hid = is_e0 ? scan_to_hid_e0[scancode] : scan_to_hid[scancode];
    if(hid) { CAKE_Keys[hid] = state; }

    return;
}

// Mouse
static void cake_handle_mouse(RAWMOUSE *mouse) {
    USHORT flags = mouse->usButtonFlags;

    if (!(mouse->usFlags & MOUSE_MOVE_ABSOLUTE)) {
        CAKE_MouseDeltaX += mouse->lLastX;
        CAKE_MouseDeltaY += mouse->lLastY;
    }

    if (flags & RI_MOUSE_WHEEL) { CAKE_MouseWheel += (int32_t)(SHORT)mouse->usButtonData; }

    if (flags & RI_MOUSE_LEFT_BUTTON_DOWN)   { CAKE_MouseButtons[CAKE_MOUSE_LEFT]   = true; }
    if (flags & RI_MOUSE_LEFT_BUTTON_UP)     { CAKE_MouseButtons[CAKE_MOUSE_LEFT]   = false; }
    if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN)  { CAKE_MouseButtons[CAKE_MOUSE_RIGHT]  = true; }
    if (flags & RI_MOUSE_RIGHT_BUTTON_UP)    { CAKE_MouseButtons[CAKE_MOUSE_RIGHT]  = false; }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) { CAKE_MouseButtons[CAKE_MOUSE_MIDDLE] = true; }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_UP)   { CAKE_MouseButtons[CAKE_MOUSE_MIDDLE] = false; }
    if (flags & RI_MOUSE_BUTTON_4_DOWN)      { CAKE_MouseButtons[CAKE_MOUSE_X1]     = true; }
    if (flags & RI_MOUSE_BUTTON_4_UP)        { CAKE_MouseButtons[CAKE_MOUSE_X1]     = false; }
    if (flags & RI_MOUSE_BUTTON_5_DOWN)      { CAKE_MouseButtons[CAKE_MOUSE_X2]     = true; }
    if (flags & RI_MOUSE_BUTTON_5_UP)        { CAKE_MouseButtons[CAKE_MOUSE_X2]     = false; }

    return;
}

// Controller helpers
static bool cake_is_xinput_path(const char *path) {
    return strstr(path, "IG_") != NULL ||
           strstr(path, "ig_") != NULL;
}

static bool cake_is_controller_path(const char *path) {
    HANDLE h = CreateFileA(
        path, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL
    );

    if (h == INVALID_HANDLE_VALUE) { return false; }

    PHIDP_PREPARSED_DATA preparsed = NULL;
    if (!HidD_GetPreparsedData(h, &preparsed)) {
        CloseHandle(h);
        return false;
    }

    HIDP_CAPS caps;
    NTSTATUS  status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    CloseHandle(h);

    if (status != HIDP_STATUS_SUCCESS) { return false; }

    return caps.UsagePage == 0x01 &&
           (caps.Usage == 0x04 || caps.Usage == 0x05);
}

static bool cake_xinput_index_taken(int xinputIndex) {
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++)
        if (cake_controllers[i].identityValid &&
            cake_controllers[i].identity.backend == CAKE_BACKEND_XINPUT &&
            cake_controllers[i].identity.xinputIndex == xinputIndex) { return true; }

    return false;
}

// Controller ID
static void cake_identify_controller(cake_ControllerDevice *controller) {
    cake_ControllerIdentity *id = &controller->identity;

    memset(id, 0, sizeof(*id));
    id->xinputIndex = -1;

    HANDLE h = CreateFileA(
        controller->deviceId, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL
    );
    if (h != INVALID_HANDLE_VALUE) {
        HIDD_ATTRIBUTES attr = {0};

        attr.Size = sizeof(attr);
        if (HidD_GetAttributes(h, &attr)) {
            id->vendorId  = attr.VendorID;
            id->productId = attr.ProductID;
        }
        CloseHandle(h);
    }

    if (cake_is_xinput_path(controller->deviceId)) {
        id->backend = CAKE_BACKEND_XINPUT;
        id->xinputIndex = -1;

        XINPUT_STATE xs;
        for (int i = 0; i < 4; i++) {
            if (cake_xinput_index_taken(i)) { continue; }
            if (XInputGetState(i, &xs) == ERROR_SUCCESS) {
                id->xinputIndex = i;
                XInputGetCapabilities(i, 0, &id->xcapabilities);
                XInputGetBatteryInformation(i, BATTERY_DEVTYPE_GAMEPAD, &id->battery);
                break;
            }
        }
    } else {
        id->backend = CAKE_BACKEND_HID;
    }

    controller->identityValid = true;

    return;
}

// Controller input polling
static void cake_poll_xinput(cake_ControllerDevice *controller) {
    XINPUT_STATE xinputstate;

    if (XInputGetState(controller->identity.xinputIndex, &xinputstate) != ERROR_SUCCESS) { return; }

    int back                                = 1 - controller->front;
    CAKE_ControllerState *controllerstate   = &controller->state[back];

    controllerstate->buttons        = xinputstate.Gamepad.wButtons;
    controllerstate->leftTrigger   = xinputstate.Gamepad.bLeftTrigger;
    controllerstate->rightTrigger  = xinputstate.Gamepad.bRightTrigger;
    controllerstate->thumbLeftX       = xinputstate.Gamepad.sThumbLX;
    controllerstate->thumbLeftY       = xinputstate.Gamepad.sThumbLY;
    controllerstate->thumbRightX       = xinputstate.Gamepad.sThumbRX;
    controllerstate->thumbRightY       = xinputstate.Gamepad.sThumbRY;

    controller->front               = back;

    return;
}

// Called from CAKE_Poll()
static void cake_poll_controllers(void) {
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        cake_ControllerDevice *controller = &cake_controllers[i];

        if (controller->deviceId[0] == '\0') { continue; }
        if (!controller->identityValid)      { continue; }

        switch (controller->identity.backend) {
            case CAKE_BACKEND_XINPUT:
                cake_poll_xinput(controller);

                break;
            case CAKE_BACKEND_HID:
                /*
                CAKE does not currently handle input for HID or DINPUT
                */
                break;
            default:
                break;
        }
    }

    return;
}

// Controller Enumeration
static void cake_enumerate_controllers(void) {
    ULONGLONG now                       =   GetTickCount64();
    BOOL      seen[CAKE_CONTROLLER_MAX] =   {0};

    HDEVINFO dev_info                   =   SetupDiGetClassDevsA(&cake_hid_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (dev_info == INVALID_HANDLE_VALUE) { return; }

    SP_DEVICE_INTERFACE_DATA iface      =   {0};
    iface.cbSize                        =   sizeof(iface);

    char detail_buf[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + CAKE_CONTROLLER_PATH_MAX];
    SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A *)detail_buf;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info, NULL, &cake_hid_guid, i, &iface); i++){
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(dev_info, &iface, detail, sizeof(detail_buf), NULL, NULL)) { continue; }

        const char *path = detail->DevicePath;
        if (!cake_is_controller_path(path)) { continue; }

        const char *name = cake_is_xinput_path(path) ? "XInput Controller" : "Unknown Controller";

        int slot = -1;
        for (int j = 0; j < CAKE_CONTROLLER_MAX; j++) {
            if (cake_controllers[j].deviceId[0] != '\0' &&
                strcmp(cake_controllers[j].deviceId, path) == 0) {
                slot = j;
                break;
            }
        }

        if (slot >= 0) {
            seen[slot] = TRUE;
            cake_controllers[slot].timeLastSeen = now;

            if (cake_controllers[slot].timeLost != 0) {
                cake_controllers[slot].timeLost = 0;
                cake_controller_connstate[slot] = CAKE_CONN_ATTACHED;
                printf("[CAKE] Controller returned  | %s | slot %d\n",
                       cake_controllers[slot].name, slot);
            }
        }
        else {
            for (int j = 0; j < CAKE_CONTROLLER_MAX; j++) {
                if (cake_controllers[j].deviceId[0] == '\0') {
                    strncpy(cake_controllers[j].deviceId, path,
                            CAKE_CONTROLLER_PATH_MAX - 1);
                    strncpy(cake_controllers[j].name, name,
                            CAKE_CONTROLLER_NAME_MAX - 1);
                    cake_controllers[j].chainIndex    = j;
                    cake_controllers[j].timeAttached  = now;
                    cake_controllers[j].timeLastSeen = now;
                    cake_controllers[j].timeLost      = 0;
                    cake_controllers[j].identityValid = false;
                    cake_controllers[j].front          = 0;
                    memset(cake_controllers[j].state, 0,
                           sizeof(cake_controllers[j].state));
                    seen[j] = TRUE;
                    cake_controller_connstate[j] = CAKE_CONN_ATTACHED;

                    printf("[CAKE] Controller attached  | %s | slot %d\n",
                           cake_controllers[j].name, j);

                    cake_identify_controller(&cake_controllers[j]);

                    const cake_ControllerIdentity *id = &cake_controllers[j].identity;
                    printf("[CAKE] Controller identified | slot %d | VID %04X PID %04X | %s",
                           j, id->vendorId, id->productId,
                           id->backend == CAKE_BACKEND_XINPUT ? "XInput" : "HID");
                    if (id->backend == CAKE_BACKEND_XINPUT && id->xinputIndex >= 0) { printf(" | xinputIndex %d", id->xinputIndex); }
                    printf("\n");
                    break;
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);

    for (int j = 0; j < CAKE_CONTROLLER_MAX; j++) {
        if (cake_controllers[j].deviceId[0] == '\0') { continue; }
        if (seen[j]) { continue; }

        if (cake_controllers[j].timeLost == 0) {
            cake_controllers[j].timeLost = now;
            cake_controller_connstate[j] = CAKE_CONN_LOST;
            printf("[CAKE] Controller detached  | %s | slot %d\n",
                   cake_controllers[j].name, j);
        }
        else if (now - cake_controllers[j].timeLost > CAKE_CONTROLLER_TIMEOUT_MS) {
            cake_controller_connstate[j] = CAKE_CONN_TIMEDOUT;
            printf("[CAKE] Controller timed out | %s | slot %d\n",
                   cake_controllers[j].name, j);
            memset(&cake_controllers[j], 0, sizeof(cake_ControllerDevice));
        }
    }

    return;
}

// Combined window procedure
static LRESULT CALLBACK cake_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INPUT: {
            if (cake_silenced) { return 0; }

            UINT      size = sizeof(RAWINPUT);
            RAWINPUT  raw;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
            switch (raw.header.dwType) {
                case RIM_TYPEKEYBOARD: cake_handle_keyboard(&raw.data.keyboard);

 break;
                case RIM_TYPEMOUSE:    cake_handle_mouse(&raw.data.mouse);       break;
            }
            return 0;
        }

        case WM_DEVICECHANGE:
            if (wParam == DBT_DEVICEARRIVAL ||
                wParam == DBT_DEVICEREMOVECOMPLETE) { cake_enumerate_controllers(); }

            break;

        case WM_TIMER:
            if (wParam == CAKE_TIMER_ID) { cake_enumerate_controllers(); }
            break;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Initialization
static void cake_init(void) {
    memset(cake_controllers, 0, sizeof(cake_controllers));
    memset(cake_controller_connstate, 0, sizeof(cake_controller_connstate));

    WNDCLASSEXA windowclass   = {0};
    windowclass.cbSize        = sizeof(WNDCLASSEXA);
    windowclass.lpfnWndProc   = cake_wndproc;
    windowclass.hInstance     = GetModuleHandleA(NULL);
    windowclass.lpszClassName = "CAKE_Window";
    RegisterClassExA(&windowclass);

    cake_hwnd = CreateWindowExA(
        0, "CAKE_Window", NULL, 0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL,
        GetModuleHandleA(NULL), NULL
    );

    /* Register for raw keyboard and mouse input. */
    RAWINPUTDEVICE rid[2];

    rid[0].usUsagePage = 0x01;
    rid[0].usUsage     = 0x06; /* Keyboard */
    rid[0].dwFlags     = RIDEV_INPUTSINK;
    rid[0].hwndTarget  = cake_hwnd;

    rid[1].usUsagePage = 0x01;
    rid[1].usUsage     = 0x02; /* Mouse */
    rid[1].dwFlags     = RIDEV_INPUTSINK;
    rid[1].hwndTarget  = cake_hwnd;

    RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

    /* Register for device change notifications (controller hotplug). */
    DEV_BROADCAST_DEVICEINTERFACE_A dbi = {0};
    dbi.dbcc_size       = sizeof(dbi);
    dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbi.dbcc_classguid  = cake_hid_guid;

    cake_notify = RegisterDeviceNotificationA(
        cake_hwnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE
    );

    /* Fallback periodic re-enumeration in case any notifications are missed. */
    SetTimer(cake_hwnd, CAKE_TIMER_ID, CAKE_TIMER_MS, NULL);

    cake_initialized = true;

    printf("[CAKE] Initialised. Scanning for controllers...\n");
    cake_enumerate_controllers();

    return;
}

// Public Facing Functions

void CAKE_AttachWindow(void* nativeWindow) {
    // Recorded, deliberately unused for input capture -- see cake.h. Raw
    // Input must stay registered at the message-only window so WM_INPUT
    // keeps arriving on the CAKE_Poll thread; a handle owned by another
    // thread cannot be the Raw Input target without moving delivery there.
    cake_attached_hwnd = (HWND)nativeWindow;

    return;
}

void CAKE_Poll(void) {
    if (!cake_initialized) { cake_init(); }

    CAKE_MouseDeltaX = 0;
    CAKE_MouseDeltaY = 0;
    CAKE_MouseWheel = 0;

    MSG msg;
    while (PeekMessageA(&msg, cake_hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    cake_flush_lshift();
    cake_poll_controllers();

    return;
}

void CAKE_Shutdown(void) {
    if (!cake_initialized) { return; }

    KillTimer(cake_hwnd, CAKE_TIMER_ID);

    if (cake_notify) {
        UnregisterDeviceNotification(cake_notify);
        cake_notify = NULL;
    }

    if (cake_hwnd) {
        DestroyWindow(cake_hwnd);
        cake_hwnd = NULL;
    }

    cake_initialized = false;

    return;
}

void CAKE_Silence(void) {
    cake_silenced = true;

    return;
}

void CAKE_Resume(void) {
    cake_silenced = false;

    // Defensively clear everything WM_INPUT would have driven -- a key or
    // button released while silenced never reached cake_handle_keyboard/
    // cake_handle_mouse, so without this it would read as still held down
    // forever after resuming.
    memset(CAKE_Keys, 0, sizeof(CAKE_Keys));
    memset(CAKE_MouseButtons, 0, sizeof(CAKE_MouseButtons));
    CAKE_MouseDeltaX = 0;
    CAKE_MouseDeltaY = 0;
    CAKE_MouseWheel = 0;

    return;
}

bool CAKE_IsSilenced(void) {
    return cake_silenced;
}

bool CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX)  { return false; }
    if (cake_controllers[slot].deviceId[0] == '\0') { return false; }
    if (!cake_controllers[slot].identityValid)      { return false; }

    return true;
}

const CAKE_ControllerState *CAKE_GetControllerState(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return NULL; }

    return &cake_controllers[slot].state[cake_controllers[slot].front];
}

const char *CAKE_GetControllerName(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return NULL; }

    return cake_controllers[slot].name;
}

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return CAKE_BACKEND_UNKNOWN; }

    return cake_controllers[slot].identity.backend;
}

uint16_t CAKE_GetControllerVendorID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return 0; }

    return cake_controllers[slot].identity.vendorId;
}

uint16_t CAKE_GetControllerProductID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return 0; }

    return cake_controllers[slot].identity.productId;
}

int CAKE_GetControllerXInputIndex(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return -1; }
    if (cake_controllers[slot].identity.backend != CAKE_BACKEND_XINPUT) { return -1; }

    return cake_controllers[slot].identity.xinputIndex;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) { return CAKE_CONN_EMPTY; }

    return cake_controller_connstate[slot];
}

bool CAKE_SetControllerVibration(int slot, uint16_t leftMotor, uint16_t rightMotor) {
    if (!CAKE_IsControllerConnected(slot)) { return false; }

    if (cake_controllers[slot].identity.backend != CAKE_BACKEND_XINPUT) {
        // HID rumble is device-specific -- no generic protocol implemented
        // yet, and nothing concrete to test one against.
        return false;
    }

    XINPUT_VIBRATION vibration = { .wLeftMotorSpeed = leftMotor, .wRightMotorSpeed = rightMotor };

    return XInputSetState(cake_controllers[slot].identity.xinputIndex, &vibration) == ERROR_SUCCESS;
}

// MACOS -- window-attached input.
// CAKE owns no window here (macOS tolerates no hidden-window trick, and a
// process-global tap would demand the Input Monitoring permission). Instead
// CAKE_AttachWindow hands over the application's NSWindow and the minimal
// Objective-C stub (cake_macos_stub.m) scopes an event monitor to it inside
// the application's own event pump. The monitor fires on the MAIN thread
// (during GDMF's pump); CAKE_Poll runs on the sim thread -- so the stub
// feeds the mutex-guarded pending state below, and Poll drains it into the
// public arrays. Same observable contract as the Windows Raw Input path:
// deltas accumulate between polls, key/button tables are current state, and
// silenced input is discarded at the source. Trackpads arrive on this same
// stream already translated by the OS (motion deltas, buttons, scroll) and
// fold into the ordinary mouse state -- applications can't tell.
//
// Controllers are still stubs -- IOKit HID gamepad support is a separate
// task (game controllers don't need Input Monitoring, unlike keyboards).
#elif defined(__APPLE__)

#include <pthread.h>
#include <time.h>

// The Objective-C stub's interface (cake_macos_stub.m)
void cake_macos_stub_attach(void* nsWindow);
void cake_macos_stub_detach(void);
void cake_macos_gc_start(void);   // controller monitoring -- see the
void cake_macos_gc_stop(void);    // Controllers section below

static void cake_poll_controllers(void);

static bool cake_initialized = false;
static bool cake_silenced    = false; // see CAKE_Silence/CAKE_Resume

// Pending input state: written by the stub's intake calls (main thread),
// drained by CAKE_Poll (sim thread). The doubles carry sub-pixel trackpad
// fractions across polls so slow, fine motion is never truncated away.
static pthread_mutex_t cake_intake_lock = PTHREAD_MUTEX_INITIALIZER;
static bool   cake_pend_keys[CAKE_KEY_TABLE_SIZE];
static bool   cake_pend_buttons[CAKE_MOUSE_BUTTON_COUNT];
static double cake_pend_dx;
static double cake_pend_dy;
static double cake_pend_wheel;

// macOS virtual keycode (kVK_*) -> USB HID usage (page 0x07), the key space
// CAKE_Keys is defined in. Zero = no mapping. Modifiers are absent on
// purpose: they arrive via flagsChanged (cake_macos_intake_flags), never as
// key events.
static const uint8_t cake_mac_vk_to_hid[128] = {
    [0x00]=0x04,[0x01]=0x16,[0x02]=0x07,[0x03]=0x09,[0x04]=0x0B,[0x05]=0x0A, // A S D F H G
    [0x06]=0x1D,[0x07]=0x1B,[0x08]=0x06,[0x09]=0x19,[0x0A]=0x64,[0x0B]=0x05, // Z X C V ISO B
    [0x0C]=0x14,[0x0D]=0x1A,[0x0E]=0x08,[0x0F]=0x15,[0x10]=0x1C,[0x11]=0x17, // Q W E R Y T
    [0x12]=0x1E,[0x13]=0x1F,[0x14]=0x20,[0x15]=0x21,[0x16]=0x23,[0x17]=0x22, // 1 2 3 4 6 5
    [0x18]=0x2E,[0x19]=0x26,[0x1A]=0x24,[0x1B]=0x2D,[0x1C]=0x25,[0x1D]=0x27, // = 9 7 - 8 0
    [0x1E]=0x30,[0x1F]=0x12,[0x20]=0x18,[0x21]=0x2F,[0x22]=0x0C,[0x23]=0x13, // ] O U [ I P
    [0x24]=0x28,[0x25]=0x0F,[0x26]=0x0D,[0x27]=0x34,[0x28]=0x0E,[0x29]=0x33, // Ret L J ' K ;
    [0x2A]=0x31,[0x2B]=0x36,[0x2C]=0x38,[0x2D]=0x11,[0x2E]=0x10,[0x2F]=0x37, // \ , / N M .
    [0x30]=0x2B,[0x31]=0x2C,[0x32]=0x35,[0x33]=0x2A,[0x34]=0x58,[0x35]=0x29, // Tab Spc ` Bksp KPEnt Esc
    [0x40]=0x6C,[0x41]=0x63,[0x43]=0x55,[0x45]=0x57,[0x47]=0x53,[0x4B]=0x54, // F17 KP. KP* KP+ NumLk KP/
    [0x4C]=0x58,[0x4E]=0x56,[0x51]=0x67,[0x52]=0x62,[0x53]=0x59,[0x54]=0x5A, // KPEnt KP- KP= KP0 KP1 KP2
    [0x55]=0x5B,[0x56]=0x5C,[0x57]=0x5D,[0x58]=0x5E,[0x59]=0x5F,[0x5B]=0x60, // KP3 KP4 KP5 KP6 KP7 KP8
    [0x5C]=0x61,[0x60]=0x3E,[0x61]=0x3F,[0x62]=0x40,[0x63]=0x3C,[0x64]=0x41, // KP9 F5 F6 F7 F3 F8
    [0x65]=0x42,[0x67]=0x44,[0x69]=0x46,[0x6A]=0x6B,[0x6B]=0x47,[0x6D]=0x43, // F9 F11 F13/PrtScr F16 F14/ScrLk F10
    [0x6F]=0x45,[0x71]=0x48,[0x72]=0x49,[0x73]=0x4A,[0x74]=0x4B,[0x75]=0x4C, // F12 F15/Pause Help/Ins Home PgUp FwdDel
    [0x76]=0x3D,[0x77]=0x4D,[0x78]=0x3B,[0x79]=0x4E,[0x7A]=0x3A,[0x7B]=0x50, // F4 End F2 PgDn F1 Left
    [0x7C]=0x4F,[0x7D]=0x51,[0x7E]=0x52                                      // Right Down Up
};

// --- Intake: called by the stub's event monitor, on the main thread --------
// Silenced input is discarded here, at the source (matching the Windows
// WM_INPUT early-return), so nothing accumulates while silenced.

void cake_macos_intake_key(unsigned short keyCode, bool down) {
    if (cake_silenced || keyCode >= 128) { return; }

    uint8_t hid = cake_mac_vk_to_hid[keyCode];
    if (!hid) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_keys[hid] = down;
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

// Modifiers arrive as flagsChanged with the full flag set, not as up/down
// edges -- so all eight modifier key states are re-derived from the
// device-dependent bits every time (left/right distinguishable), plus caps
// lock from its toggle flag. Windows reports caps lock as press edges; here
// CAKE_Keys[0x39] tracks the lock *state* instead -- the closest macOS
// offers, and the difference is only observable to code timing caps-lock
// taps.
void cake_macos_intake_flags(unsigned long deviceFlags, bool capsLock) {
    if (cake_silenced) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_keys[0xE0] = (deviceFlags & 0x0001) != 0;  // LCTRL
    cake_pend_keys[0xE1] = (deviceFlags & 0x0002) != 0;  // LSHIFT
    cake_pend_keys[0xE5] = (deviceFlags & 0x0004) != 0;  // RSHIFT
    cake_pend_keys[0xE3] = (deviceFlags & 0x0008) != 0;  // LGUI (Cmd)
    cake_pend_keys[0xE7] = (deviceFlags & 0x0010) != 0;  // RGUI
    cake_pend_keys[0xE2] = (deviceFlags & 0x0020) != 0;  // LALT (Option)
    cake_pend_keys[0xE6] = (deviceFlags & 0x0040) != 0;  // RALT
    cake_pend_keys[0xE4] = (deviceFlags & 0x2000) != 0;  // RCTRL
    cake_pend_keys[0x39] = capsLock;
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

void cake_macos_intake_button(int buttonNumber, bool down) {
    // NSEvent buttonNumber order (0 left, 1 right, 2 middle, 3+ extras)
    // matches CAKE_MOUSE_* directly.
    if (cake_silenced || buttonNumber < 0 || buttonNumber >= CAKE_MOUSE_BUTTON_COUNT) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_buttons[buttonNumber] = down;
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

void cake_macos_intake_motion(double dx, double dy) {
    if (cake_silenced) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_dx += dx;
    cake_pend_dy += dy;
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

// Wheel arrives in the Windows convention: +-120 per notch. A real wheel
// reports whole lines (lineDeltaY); a trackpad two-finger scroll reports
// precise pixel-ish deltas, scaled so ~10 points of swipe equals one notch.
void cake_macos_intake_scroll(double scrollingDeltaY, bool precise, double lineDeltaY) {
    if (cake_silenced) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_wheel += precise ? scrollingDeltaY * 12.0 : lineDeltaY * 120.0;
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

// --- Public API --------------------------------------------------------------

void CAKE_AttachWindow(void* nativeWindow) {
    if (!nativeWindow) { return; }

    cake_macos_stub_attach(nativeWindow);

    // Controllers aren't window-scoped, but this call site is the one spot
    // guaranteed to be on the main thread at startup -- where the
    // framework's connect/disconnect notifications want to be registered.
    cake_macos_gc_start();

    printf("[CAKE] Attached to application window\n");

    return;
}

void CAKE_Poll(void) {
    if (!cake_initialized) {
        // Nothing to create here -- the event monitor was installed by
        // CAKE_AttachWindow (on the main thread, where AppKit demands it).
        cake_initialized = true;
        printf("[CAKE] Initialised.\n");
    }

    pthread_mutex_lock(&cake_intake_lock);

    memcpy(CAKE_Keys, cake_pend_keys, sizeof(CAKE_Keys));
    memcpy(CAKE_MouseButtons, cake_pend_buttons, sizeof(CAKE_MouseButtons));

    // Truncate-and-carry: the integer part becomes this poll's delta, the
    // sub-pixel remainder stays pending so fine trackpad motion accumulates
    // instead of vanishing.
    CAKE_MouseDeltaX = (int32_t)cake_pend_dx;   cake_pend_dx -= CAKE_MouseDeltaX;
    CAKE_MouseDeltaY = (int32_t)cake_pend_dy;   cake_pend_dy -= CAKE_MouseDeltaY;
    CAKE_MouseWheel  = (int32_t)cake_pend_wheel; cake_pend_wheel -= CAKE_MouseWheel;

    pthread_mutex_unlock(&cake_intake_lock);

    cake_poll_controllers();

    return;
}

void CAKE_Shutdown(void) {
    if (!cake_initialized) { return; }

    cake_macos_gc_stop();
    cake_macos_stub_detach();

    cake_initialized = false;

    return;
}

void CAKE_Silence(void) {
    cake_silenced = true;

    return;
}

void CAKE_Resume(void) {
    cake_silenced = false;

    // See the Windows CAKE_Resume's own comment -- same reasoning: a
    // key/button released while silenced never reached the intake, so
    // without this it would read as still held down forever after. The
    // pending copies clear too, or Poll would immediately repopulate the
    // public arrays with the stale state.
    pthread_mutex_lock(&cake_intake_lock);
    memset(cake_pend_keys, 0, sizeof(cake_pend_keys));
    memset(cake_pend_buttons, 0, sizeof(cake_pend_buttons));
    cake_pend_dx = cake_pend_dy = cake_pend_wheel = 0.0;
    pthread_mutex_unlock(&cake_intake_lock);

    memset(CAKE_Keys, 0, sizeof(CAKE_Keys));
    memset(CAKE_MouseButtons, 0, sizeof(CAKE_MouseButtons));
    CAKE_MouseDeltaX = 0;
    CAKE_MouseDeltaY = 0;
    CAKE_MouseWheel = 0;

    return;
}

bool CAKE_IsSilenced(void) {
    return cake_silenced;
}

// --- Controllers -------------------------------------------------------------
// Serviced through GameController.framework via the stub: connect/disconnect
// notifications land on the main thread (inside GDMF's pump) and call the
// two cake_macos_gc_* callbacks below; CAKE_Poll reads each present
// controller's state on the sim thread. Slot policy, the connection
// lifecycle (ATTACHED -> LOST -> grace period -> TIMEDOUT, matching the
// Windows backend), and the double-buffered state all live here in C -- the
// stub only touches the framework. The XInput-shaped mapping (real A/B/X/Y
// positions, triggers 0-255, sticks -32768..32767 with +Y up) comes from
// the framework's extended-gamepad profile, so Xbox/PlayStation/Switch pads
// arrive laid out correctly without per-device tables. The framework hides
// USB identity, so vendor/product ID report their documented 0 sentinel.

void cake_macos_gc_start(void);
void cake_macos_gc_stop(void);
bool cake_macos_gc_read(void* gc, CAKE_ControllerState* out);

typedef struct {
    void*                gc;         // opaque GCController*, valid while present
    char                 name[CAKE_CONTROLLER_NAME_MAX];
    bool                 present;
    uint64_t             timeLost;   // ms timestamp of disconnect; 0 = not lost
    CAKE_ControllerState state[2];
    volatile int         front;
    // Double buffered input; front swaps after every poll; state[1-front] is
    // the back buffer; state[front] is always safe to read.
} cake_mac_controller;

static cake_mac_controller cake_controllers[CAKE_CONTROLLER_MAX];

// Decoupled from cake_controllers[] so a TIMEDOUT code survives the memset
// that frees the slot for reuse -- same reasoning as the Windows backend.
static CAKE_ControllerConnectionState cake_controller_connstate[CAKE_CONTROLLER_MAX];

static uint64_t cake_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

// Connect callback (main thread). A controller returning within the grace
// period reclaims its old slot (matched by name) with its history intact;
// otherwise the first free slot is used.
void cake_macos_gc_connected(void* gc, const char* name) {
    pthread_mutex_lock(&cake_intake_lock);

    int slot = -1;
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        if (!cake_controllers[i].present && cake_controllers[i].timeLost != 0 &&
            strncmp(cake_controllers[i].name, name, CAKE_CONTROLLER_NAME_MAX) == 0) {
            slot = i;   // came back within the grace period
            break;
        }
    }
    for (int i = 0; slot < 0 && i < CAKE_CONTROLLER_MAX; i++) {
        if (!cake_controllers[i].present && cake_controllers[i].timeLost == 0) { slot = i; }
    }

    if (slot >= 0) {
        cake_mac_controller* c = &cake_controllers[slot];
        memset(c->state, 0, sizeof(c->state));
        c->gc       = gc;
        c->present  = true;
        c->timeLost = 0;
        c->front    = 0;
        snprintf(c->name, sizeof(c->name), "%s", name);
        cake_controller_connstate[slot] = CAKE_CONN_ATTACHED;
        printf("[CAKE] Controller attached  | %s | slot %d\n", c->name, slot);
    }

    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

// Disconnect callback (main thread). The slot enters the LOST grace period;
// CAKE_Poll times it out.
void cake_macos_gc_disconnected(void* gc) {
    pthread_mutex_lock(&cake_intake_lock);

    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        if (cake_controllers[i].present && cake_controllers[i].gc == gc) {
            cake_controllers[i].present  = false;
            cake_controllers[i].gc       = NULL;
            cake_controllers[i].timeLost = cake_now_ms();
            cake_controller_connstate[i] = CAKE_CONN_LOST;
            printf("[CAKE] Controller detached  | %s | slot %d\n",
                   cake_controllers[i].name, i);
            break;
        }
    }

    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

// Called from CAKE_Poll (sim thread): read every present controller into
// its back buffer and swap, and run the LOST -> TIMEDOUT grace period.
static void cake_poll_controllers(void) {
    uint64_t now = cake_now_ms();

    pthread_mutex_lock(&cake_intake_lock);

    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        cake_mac_controller* c = &cake_controllers[i];

        if (c->present) {
            int back = 1 - c->front;
            if (cake_macos_gc_read(c->gc, &c->state[back])) { c->front = back; }
        }
        else if (c->timeLost != 0 && now - c->timeLost > CAKE_CONTROLLER_TIMEOUT_MS) {
            cake_controller_connstate[i] = CAKE_CONN_TIMEDOUT;
            printf("[CAKE] Controller timed out | %s | slot %d\n", c->name, i);
            memset(c, 0, sizeof(*c));
        }
    }

    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

bool CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) { return false; }

    return cake_controllers[slot].present;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) { return CAKE_CONN_EMPTY; }

    return cake_controller_connstate[slot];
}

const CAKE_ControllerState *CAKE_GetControllerState(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return NULL; }

    return &cake_controllers[slot].state[cake_controllers[slot].front];
}

const char *CAKE_GetControllerName(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return NULL; }

    return cake_controllers[slot].name;
}

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return CAKE_BACKEND_UNKNOWN; }

    return CAKE_BACKEND_HID;
}

uint16_t CAKE_GetControllerVendorID(int slot) { (void)slot;

    return 0; }   // GameController.framework hides USB identity -- documented sentinel

uint16_t CAKE_GetControllerProductID(int slot) { (void)slot;

    return 0; }   // same

int CAKE_GetControllerXInputIndex(int slot) { (void)slot;

    return -1; }  // never XInput on this platform

bool CAKE_SetControllerVibration(int slot, uint16_t leftMotor, uint16_t rightMotor) {
    (void)slot; (void)leftMotor; (void)rightMotor;

    // The framework does rumble through Core Haptics -- a real task of its
    // own, not a one-liner. Documented false until then.
    return false;
}

// LINUX (Wayland + X11) -- window-attached input, evdev controllers.
// The ratified capture-on-poll-thread pattern (see cake.h) -- the Windows
// shape, not the macOS one, on both display servers. Controllers are
// shared; keyboard/mouse intake is per-display-server, chosen by the
// tagged handle CAKE_AttachWindow receives (fuselage_native.h).
//
// Wayland: GDMF's backend runs a dedicated reader thread (a compositor
// disconnects clients that stop reading during long ticks -- see
// gdmf_window_wayland.c), so CAKE keeps its events out of that thread's
// hands entirely. CAKE_AttachWindow receives the engine's wl_display (the
// connection is the handle on Wayland -- see GDMF_GetNativeWindowHandle's
// comment) and binds its OWN wl_seat objects on it, parked on a PRIVATE
// wl_event_queue: the reader thread's socket reads deposit CAKE's events
// there untouched, and CAKE_Poll dispatches the queue itself -- listeners
// run on the poll thread, exactly like WM_INPUT arriving at the Raw Input
// message-only window. Same observable contract as Windows: deltas
// accumulate between polls (unaccelerated, via zwp_relative_pointer_v1 --
// the pre-acceleration motion Raw Input reports), key/button tables are
// current state, silenced input is discarded at the source. The pending
// block + mutex stay (attach-time dispatch runs on the main thread before
// the sim starts; the lock keeps that window airtight and the drain code
// identical to macOS's).
//
// X11: CAKE opens ITS OWN Display connection in the attach -- no sharing
// with (and so no locking against) GDMF's -- and selects XInput2 raw
// events on the root window: the pre-acceleration device stream, Raw
// Input's actual X11 counterpart. CAKE_Poll drains XPending on that
// connection directly into the pending block -- listeners on the poll
// thread, same as everywhere else; the mutex still guards the intake,
// keeping the drain code identical across backends. X keycodes are evdev
// keycodes offset by 8 (a wire-protocol relic: X reserves codes 0-7), so
// the same cake_evdev_to_hid table serves both display servers. NOTE: X11
// raw events arrive regardless of window focus (unlike Windows' Raw Input
// foreground-only default) -- the engine's focus-silencing layer
// (FuselageInputRequiresFocus) already handles that, and CAKE's own
// contract has never promised focus filtering.
//
// Wayland hands CAKE two things Raw Input never did, both used there: key
// events carry raw evdev keycodes (one static table to HID usages, no
// scan-code E0/fake-shift folklore), and keyboard enter/leave carry truth
// -- enter lists the keys already held, leave means no more releases will
// arrive, so the pending keys resync on enter and clear on leave instead of
// sticking. X11 offers no held-keys-on-focus resync; raw events simply
// keep flowing, which serves the same end.
//
// Controllers never touch the display server at all: gamepads are evdev devices
// (/dev/input/event*), read and rescanned wholly on the CAKE_Poll thread --
// no lock needed, unlike the keyboard/mouse pending state. Slot policy, the
// ATTACHED -> LOST -> grace -> TIMEDOUT lifecycle, name-matched slot
// reclaim, and the double-buffered state all mirror the other two backends.
// Rumble is real here (FF_RUMBLE), matching XInput where macOS still
// returns false.
#elif defined(__linux__)

#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <wayland-client.h>
#include "relative-pointer-unstable-v1-client-protocol.h"
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include "fuselage_native.h"   // the tagged Linux native-window handle

static void cake_poll_controllers(void);
static void cake_scan_controllers(void);

static bool cake_initialized = false;
static bool cake_silenced    = false; // see CAKE_Silence/CAKE_Resume

// --- Pending input state -----------------------------------------------------
// Written by the seat listeners (main thread, inside GDMF's pump), drained
// by CAKE_Poll (sim thread). Doubles carry sub-pixel touchpad fractions
// across polls, same as the macOS backend.
static pthread_mutex_t cake_intake_lock = PTHREAD_MUTEX_INITIALIZER;
static bool   cake_pend_keys[CAKE_KEY_TABLE_SIZE];
static bool   cake_pend_buttons[CAKE_MOUSE_BUTTON_COUNT];
static double cake_pend_dx;
static double cake_pend_dy;
static double cake_pend_wheel;

// --- Wayland objects (bound by CAKE_AttachWindow, on CAKE's behalf) ----------

static struct wl_display*     cake_display  = NULL;   // borrowed, never destroyed
static struct wl_event_queue* cake_queue    = NULL;   // all CAKE events land here
static struct wl_registry*    cake_registry = NULL;
static struct wl_seat*        cake_seat     = NULL;
static struct wl_keyboard*    cake_keyboard = NULL;
static struct wl_pointer*     cake_pointer  = NULL;

static struct zwp_relative_pointer_manager_v1* cake_relManager = NULL;
static struct zwp_relative_pointer_v1*         cake_relPointer = NULL;

// --- X11 objects (opened by CAKE_AttachWindow, owned outright) ---------------

static Display* cake_x11_display = NULL;   // CAKE's OWN connection, not GDMF's
static int      cake_x11_opcode  = 0;      // XInput2's major opcode

// Wheel bookkeeping: a compositor that speaks axis_discrete sends it
// alongside the continuous axis event for the same notch; the discrete form
// is exact (one notch = one unit) so once seen it wins and the continuous
// event of that dispatch is skipped to avoid double-counting.
static bool cake_wheel_discrete_seen = false;

// evdev keycode (KEY_*, what wl_keyboard.key carries) -> USB HID usage
// (page 0x07), the key space CAKE_Keys is defined in. Zero = no mapping.
// Unlike macOS, modifiers arrive as ordinary key events here and sit in
// this same table; unlike Windows, there is no E0-prefix or fake-shift
// folklore -- evdev already speaks in whole keys.
static const uint8_t cake_evdev_to_hid[256] = {
    [KEY_ESC]=0x29,
    [KEY_1]=0x1E,[KEY_2]=0x1F,[KEY_3]=0x20,[KEY_4]=0x21,[KEY_5]=0x22,
    [KEY_6]=0x23,[KEY_7]=0x24,[KEY_8]=0x25,[KEY_9]=0x26,[KEY_0]=0x27,
    [KEY_MINUS]=0x2D,[KEY_EQUAL]=0x2E,[KEY_BACKSPACE]=0x2A,[KEY_TAB]=0x2B,
    [KEY_Q]=0x14,[KEY_W]=0x1A,[KEY_E]=0x08,[KEY_R]=0x15,[KEY_T]=0x17,
    [KEY_Y]=0x1C,[KEY_U]=0x18,[KEY_I]=0x0C,[KEY_O]=0x12,[KEY_P]=0x13,
    [KEY_LEFTBRACE]=0x2F,[KEY_RIGHTBRACE]=0x30,[KEY_ENTER]=0x28,
    [KEY_LEFTCTRL]=0xE0,
    [KEY_A]=0x04,[KEY_S]=0x16,[KEY_D]=0x07,[KEY_F]=0x09,[KEY_G]=0x0A,
    [KEY_H]=0x0B,[KEY_J]=0x0D,[KEY_K]=0x0E,[KEY_L]=0x0F,
    [KEY_SEMICOLON]=0x33,[KEY_APOSTROPHE]=0x34,[KEY_GRAVE]=0x35,
    [KEY_LEFTSHIFT]=0xE1,[KEY_BACKSLASH]=0x31,
    [KEY_Z]=0x1D,[KEY_X]=0x1B,[KEY_C]=0x06,[KEY_V]=0x19,[KEY_B]=0x05,
    [KEY_N]=0x11,[KEY_M]=0x10,
    [KEY_COMMA]=0x36,[KEY_DOT]=0x37,[KEY_SLASH]=0x38,[KEY_RIGHTSHIFT]=0xE5,
    [KEY_KPASTERISK]=0x55,[KEY_LEFTALT]=0xE2,[KEY_SPACE]=0x2C,
    [KEY_CAPSLOCK]=0x39,
    [KEY_F1]=0x3A,[KEY_F2]=0x3B,[KEY_F3]=0x3C,[KEY_F4]=0x3D,[KEY_F5]=0x3E,
    [KEY_F6]=0x3F,[KEY_F7]=0x40,[KEY_F8]=0x41,[KEY_F9]=0x42,[KEY_F10]=0x43,
    [KEY_NUMLOCK]=0x53,[KEY_SCROLLLOCK]=0x47,
    [KEY_KP7]=0x5F,[KEY_KP8]=0x60,[KEY_KP9]=0x61,[KEY_KPMINUS]=0x56,
    [KEY_KP4]=0x5C,[KEY_KP5]=0x5D,[KEY_KP6]=0x5E,[KEY_KPPLUS]=0x57,
    [KEY_KP1]=0x59,[KEY_KP2]=0x5A,[KEY_KP3]=0x5B,[KEY_KP0]=0x62,
    [KEY_KPDOT]=0x63,[KEY_102ND]=0x64,[KEY_F11]=0x44,[KEY_F12]=0x45,
    [KEY_KPENTER]=0x58,[KEY_RIGHTCTRL]=0xE4,[KEY_KPSLASH]=0x54,
    [KEY_SYSRQ]=0x46,[KEY_RIGHTALT]=0xE6,
    [KEY_HOME]=0x4A,[KEY_UP]=0x52,[KEY_PAGEUP]=0x4B,[KEY_LEFT]=0x50,
    [KEY_RIGHT]=0x4F,[KEY_END]=0x4D,[KEY_DOWN]=0x51,[KEY_PAGEDOWN]=0x4E,
    [KEY_INSERT]=0x49,[KEY_DELETE]=0x4C,[KEY_KPEQUAL]=0x67,
    [KEY_PAUSE]=0x48,[KEY_LEFTMETA]=0xE3,[KEY_RIGHTMETA]=0xE7,
    [KEY_COMPOSE]=0x65
};

// --- wl_keyboard -------------------------------------------------------------
// All listeners fire on the main thread, inside GDMF's pump. Silenced input
// is discarded here at the source (matching the Windows WM_INPUT
// early-return), so nothing accumulates while silenced.

static void cake_kb_keymap(void* data, struct wl_keyboard* kb, uint32_t format,
                           int32_t fd, uint32_t size) {
    (void)data; (void)kb; (void)format; (void)size;
    // CAKE's key space is scancode-shaped (HID usages), deliberately layout-
    // blind -- the xkb keymap is a text-input concern and not consulted.
    close(fd);

    return;
}

static void cake_kb_enter(void* data, struct wl_keyboard* kb, uint32_t serial,
                          struct wl_surface* surface, struct wl_array* keys) {
    (void)data; (void)kb; (void)serial; (void)surface;

    if (cake_silenced) { return; }

    // Focus arrives with the list of keys already held -- resync so a key
    // pressed just before focus (or held across an alt-tab return) reads
    // correctly from the first poll.
    pthread_mutex_lock(&cake_intake_lock);
    uint32_t* key;
    wl_array_for_each(key, keys) {
        if (*key < 256) {
            uint8_t hid = cake_evdev_to_hid[*key];
            if (hid) { cake_pend_keys[hid] = true; }
        }
    }
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

static void cake_kb_leave(void* data, struct wl_keyboard* kb, uint32_t serial,
                          struct wl_surface* surface) {
    (void)data; (void)kb; (void)serial; (void)surface;

    // No further key events will arrive for keys released while unfocused;
    // clear now so nothing sticks. The enter resync restores anything still
    // genuinely held when focus returns.
    pthread_mutex_lock(&cake_intake_lock);
    memset(cake_pend_keys, 0, sizeof(cake_pend_keys));
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

static void cake_kb_key(void* data, struct wl_keyboard* kb, uint32_t serial,
                        uint32_t time, uint32_t key, uint32_t state) {
    (void)data; (void)kb; (void)serial; (void)time;

    // Wayland sends only real edges -- auto-repeat is the client's own
    // affair (repeat_info) and never appears here, so no isARepeat filter.
    if (cake_silenced || key >= 256) { return; }

    uint8_t hid = cake_evdev_to_hid[key];
    if (!hid) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_keys[hid] = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

static void cake_kb_modifiers(void* data, struct wl_keyboard* kb, uint32_t serial,
                              uint32_t depressed, uint32_t latched,
                              uint32_t locked, uint32_t group) {
    (void)data; (void)kb; (void)serial; (void)depressed; (void)latched;
    (void)locked; (void)group;
    // Modifier keys already arrive as ordinary key events above; the xkb
    // modifier state is layout business, not CAKE's.
    return;
}

static void cake_kb_repeat_info(void* data, struct wl_keyboard* kb,
                                int32_t rate, int32_t delay) {
    (void)data; (void)kb; (void)rate; (void)delay;
    return;   // a state table wants edges, never repeats
}

static const struct wl_keyboard_listener cake_kb_listener = {
    .keymap      = cake_kb_keymap,
    .enter       = cake_kb_enter,
    .leave       = cake_kb_leave,
    .key         = cake_kb_key,
    .modifiers   = cake_kb_modifiers,
    .repeat_info = cake_kb_repeat_info,
};

// --- wl_pointer + zwp_relative_pointer_v1 ------------------------------------

static void cake_pt_enter(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)serial; (void)surface; (void)sx; (void)sy;
    return;   // position is GDMF's business (GDMF_GetMousePosition)
}

static void cake_pt_leave(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface) {
    (void)data; (void)pointer; (void)serial; (void)surface;
    // Nothing to clear: a button pressed over the surface keeps an implicit
    // grab, so its release still arrives even after the pointer leaves.
    return;
}

static void cake_pt_motion(void* data, struct wl_pointer* pointer, uint32_t time,
                           wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)time; (void)sx; (void)sy;
    // Deltas come from the relative-pointer protocol (unaccelerated, the
    // Raw Input parity path). Absolute surface positions are not deltas;
    // deriving motion from them would fold in pointer acceleration and go
    // silent at the surface edge. Compositors without relative-pointer are
    // not worth a degraded fallback -- every current one has it.
    return;
}

static void cake_rel_motion(void* data, struct zwp_relative_pointer_v1* rel,
                            uint32_t utime_hi, uint32_t utime_lo,
                            wl_fixed_t dx, wl_fixed_t dy,
                            wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
    (void)data; (void)rel; (void)utime_hi; (void)utime_lo; (void)dx; (void)dy;

    if (cake_silenced) { return; }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_dx += wl_fixed_to_double(dx_unaccel);
    cake_pend_dy += wl_fixed_to_double(dy_unaccel);
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

static const struct zwp_relative_pointer_v1_listener cake_rel_listener = {
    .relative_motion = cake_rel_motion,
};

static void cake_pt_button(void* data, struct wl_pointer* pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state) {
    (void)data; (void)pointer; (void)serial; (void)time;

    if (cake_silenced) { return; }

    // evdev button codes; the order matches CAKE_MOUSE_* directly.
    int index;
    switch (button) {
    case BTN_LEFT:   index = CAKE_MOUSE_LEFT;   break;
    case BTN_RIGHT:  index = CAKE_MOUSE_RIGHT;  break;
    case BTN_MIDDLE: index = CAKE_MOUSE_MIDDLE; break;
    case BTN_SIDE:   index = CAKE_MOUSE_X1;     break;
    case BTN_EXTRA:  index = CAKE_MOUSE_X2;     break;
    default:         return;
    }

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_buttons[index] = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

// Wheel arrives in the Windows convention: +-120 per notch, positive away
// from the user. Wayland's vertical axis is positive toward the user, hence
// the negations. A real wheel is the discrete path (exact notches); a
// touchpad two-finger scroll is continuous, scaled so ~15 logical units --
// the conventional notch distance -- equal one notch.
static void cake_pt_axis(void* data, struct wl_pointer* pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
    (void)data; (void)pointer; (void)time;

    if (cake_silenced || axis != WL_POINTER_AXIS_VERTICAL_SCROLL) { return; }
    if (cake_wheel_discrete_seen) { return; }   // the discrete event carried it

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_wheel -= wl_fixed_to_double(value) * (120.0 / 15.0);
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

static void cake_pt_axis_discrete(void* data, struct wl_pointer* pointer,
                                  uint32_t axis, int32_t discrete) {
    (void)data; (void)pointer;

    if (cake_silenced || axis != WL_POINTER_AXIS_VERTICAL_SCROLL) { return; }

    cake_wheel_discrete_seen = true;

    pthread_mutex_lock(&cake_intake_lock);
    cake_pend_wheel -= (double)discrete * 120.0;
    pthread_mutex_unlock(&cake_intake_lock);

    return;
}

static void cake_pt_frame(void* data, struct wl_pointer* pointer) {
    (void)data; (void)pointer;
    cake_wheel_discrete_seen = false;   // discrete/continuous pairing is per-frame

    return;
}

static void cake_pt_axis_source(void* data, struct wl_pointer* pointer, uint32_t source) {
    (void)data; (void)pointer; (void)source;
    return;
}

static void cake_pt_axis_stop(void* data, struct wl_pointer* pointer,
                              uint32_t time, uint32_t axis) {
    (void)data; (void)pointer; (void)time; (void)axis;
    return;
}

static const struct wl_pointer_listener cake_pt_listener = {
    .enter         = cake_pt_enter,
    .leave         = cake_pt_leave,
    .motion        = cake_pt_motion,
    .button        = cake_pt_button,
    .axis          = cake_pt_axis,
    .frame         = cake_pt_frame,
    .axis_source   = cake_pt_axis_source,
    .axis_stop     = cake_pt_axis_stop,
    .axis_discrete = cake_pt_axis_discrete,
};

// --- wl_seat / wl_registry ---------------------------------------------------

static void cake_seat_capabilities(void* data, struct wl_seat* seat, uint32_t caps) {
    (void)data;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !cake_keyboard) {
        cake_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(cake_keyboard, &cake_kb_listener, NULL);
    }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && cake_keyboard) {
        wl_keyboard_destroy(cake_keyboard);
        cake_keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !cake_pointer) {
        cake_pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(cake_pointer, &cake_pt_listener, NULL);
        if (cake_relManager && !cake_relPointer) {
            cake_relPointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
                cake_relManager, cake_pointer);
            zwp_relative_pointer_v1_add_listener(cake_relPointer, &cake_rel_listener, NULL);
        }
    }
    if (!(caps & WL_SEAT_CAPABILITY_POINTER) && cake_pointer) {
        if (cake_relPointer) {
            zwp_relative_pointer_v1_destroy(cake_relPointer);
            cake_relPointer = NULL;
        }
        wl_pointer_destroy(cake_pointer);
        cake_pointer = NULL;
    }

    return;
}

static void cake_seat_name(void* data, struct wl_seat* seat, const char* name) {
    (void)data; (void)seat; (void)name;
    return;
}

static const struct wl_seat_listener cake_seat_listener = {
    .capabilities = cake_seat_capabilities,
    .name         = cake_seat_name,
};

static void cake_registry_global(void* data, struct wl_registry* registry,
                                 uint32_t name, const char* interface, uint32_t version) {
    (void)data;

    if (strcmp(interface, wl_seat_interface.name) == 0 && !cake_seat) {
        cake_seat = wl_registry_bind(registry, name, &wl_seat_interface,
                                     version < 5 ? version : 5);
        wl_seat_add_listener(cake_seat, &cake_seat_listener, NULL);
    } else if (strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0) {
        cake_relManager = wl_registry_bind(registry, name,
                                           &zwp_relative_pointer_manager_v1_interface, 1);
    }

    return;
}

static void cake_registry_global_remove(void* data, struct wl_registry* registry,
                                        uint32_t name) {
    (void)data; (void)registry; (void)name;
    return;
}

static const struct wl_registry_listener cake_registry_listener = {
    .global        = cake_registry_global,
    .global_remove = cake_registry_global_remove,
};

// --- X11: XInput2 raw events -------------------------------------------------
// Runs on the poll thread (CAKE_Poll's drain), into the same pending block
// the Wayland listeners feed. Silenced input is discarded here at the
// source, same as every backend.

static void cake_x11_handle_raw(const XIRawEvent* raw) {
    if (cake_silenced) { return; }

    switch (raw->evtype) {

    case XI_RawKeyPress:
    case XI_RawKeyRelease: {
        // X keycode = evdev keycode + 8 (X reserves codes 0-7), so the
        // shared evdev table serves after the offset.
        int evdev = raw->detail - 8;
        if (evdev < 0 || evdev > 255) { return; }

        uint8_t hid = cake_evdev_to_hid[evdev];
        if (!hid) { return; }

        pthread_mutex_lock(&cake_intake_lock);
        cake_pend_keys[hid] = (raw->evtype == XI_RawKeyPress);
        pthread_mutex_unlock(&cake_intake_lock);

        return;
    }

    case XI_RawButtonPress:
    case XI_RawButtonRelease: {
        bool down = (raw->evtype == XI_RawButtonPress);

        // Core buttons by the X11 numbering: 1 left, 2 MIDDLE, 3 RIGHT
        // (not the CAKE_MOUSE_* order), 4/5 the wheel as press-edge events,
        // 6/7 horizontal scroll (unmapped), 8/9 the side buttons.
        int index;
        switch (raw->detail) {
        case 1: index = CAKE_MOUSE_LEFT;   break;
        case 2: index = CAKE_MOUSE_MIDDLE; break;
        case 3: index = CAKE_MOUSE_RIGHT;  break;
        case 4:
        case 5:
            // Wheel notches in the Windows convention: +-120 per notch,
            // positive away from the user (button 4). Press edge only --
            // the release of a wheel "button" is the same notch again.
            if (!down) { return; }
            pthread_mutex_lock(&cake_intake_lock);
            cake_pend_wheel += (raw->detail == 4) ? 120.0 : -120.0;
            pthread_mutex_unlock(&cake_intake_lock);
            return;
        case 8: index = CAKE_MOUSE_X1;     break;
        case 9: index = CAKE_MOUSE_X2;     break;
        default: return;
        }

        pthread_mutex_lock(&cake_intake_lock);
        cake_pend_buttons[index] = down;
        pthread_mutex_unlock(&cake_intake_lock);

        return;
    }

    case XI_RawMotion: {
        // raw_values are the device's untransformed deltas -- the
        // pre-acceleration motion Raw Input reports (values would fold the
        // pointer acceleration back in). Valuators 0/1 are x/y on relative
        // devices; scroll valuators sit higher and are deliberately not
        // read here -- the wheel already arrives as buttons 4/5 above, and
        // reading both would double-count.
        double dx = 0.0;
        double dy = 0.0;
        const double* value = raw->raw_values;

        for (int i = 0; i < raw->valuators.mask_len * 8; i++) {
            if (!XIMaskIsSet(raw->valuators.mask, i)) { continue; }
            if (i == 0) { dx = *value; }
            if (i == 1) { dy = *value; }
            value++;
        }
        if (dx == 0.0 && dy == 0.0) { return; }

        pthread_mutex_lock(&cake_intake_lock);
        cake_pend_dx += dx;
        cake_pend_dy += dy;
        pthread_mutex_unlock(&cake_intake_lock);

        return;
    }

    default:
        return;
    }
}

// The poll-thread drain: everything the server has queued since last poll,
// never a blocking read (XPending does the socket read itself).
static void cake_x11_drain(void) {
    while (XPending(cake_x11_display) > 0) {
        XEvent ev;
        XNextEvent(cake_x11_display, &ev);

        if (ev.xcookie.type == GenericEvent &&
            ev.xcookie.extension == cake_x11_opcode &&
            XGetEventData(cake_x11_display, &ev.xcookie)) {
            cake_x11_handle_raw((const XIRawEvent*)ev.xcookie.data);
            XFreeEventData(cake_x11_display, &ev.xcookie);
        }
    }

    return;
}

// The X11 half of CAKE_AttachWindow. A failure here is warn-and-degrade:
// controllers (evdev, display-server-blind) keep working either way.
static void cake_x11_attach(void) {
    cake_x11_display = XOpenDisplay(NULL);
    if (!cake_x11_display) {
        printf("[CAKE] Warning: XOpenDisplay failed -- no keyboard/mouse\n");
        return;
    }

    int event, error;
    if (!XQueryExtension(cake_x11_display, "XInputExtension",
                         &cake_x11_opcode, &event, &error)) {
        printf("[CAKE] Warning: no XInput extension -- no keyboard/mouse\n");
        XCloseDisplay(cake_x11_display);
        cake_x11_display = NULL;
        return;
    }

    // Announce 2.3, accept >= 2.0. The announced version matters beyond
    // negotiation: from 2.3 the server keeps delivering raw events during
    // pointer grabs -- and GDMF's capture IS a grab (XGrabPointer with
    // confine_to), from a different connection. Announcing less would
    // silence the mouse the moment capture engaged.
    int major = 2;
    int minor = 3;
    if (XIQueryVersion(cake_x11_display, &major, &minor) != Success || major < 2) {
        printf("[CAKE] Warning: XInput2 unavailable (server has %d.%d) -- no keyboard/mouse\n",
               major, minor);
        XCloseDisplay(cake_x11_display);
        cake_x11_display = NULL;
        return;
    }

    // Raw events on the root window, all master devices: the device-level
    // stream, delivered to this connection regardless of which window has
    // focus (see the section comment -- the focus-silencing layer owns
    // that concern).
    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = { 0 };
    XIEventMask em = {
        .deviceid = XIAllMasterDevices,
        .mask_len = sizeof(mask),
        .mask     = mask,
    };
    XISetMask(mask, XI_RawKeyPress);
    XISetMask(mask, XI_RawKeyRelease);
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawButtonRelease);
    XISetMask(mask, XI_RawMotion);
    XISelectEvents(cake_x11_display, DefaultRootWindow(cake_x11_display), &em, 1);
    XFlush(cake_x11_display);

    printf("[CAKE] Attached to application window\n");

    return;
}

// --- Public API --------------------------------------------------------------

void CAKE_AttachWindow(void* nativeWindow) {
    if (!nativeWindow || cake_registry || cake_x11_display) { return; }

    // The handle is tagged (fuselage_native.h): one Linux binary carries
    // both display backends and CAKE must speak whichever the window
    // chose -- Wayland below, X11 via its own connection (both the
    // capture-on-poll-thread pattern, per the ratified contract).
    FuselageLinuxNativeHandle* handle = (FuselageLinuxNativeHandle*)nativeWindow;
    if (handle->kind && strcmp(handle->kind, "x11") == 0) {
        cake_x11_attach();
        return;
    }
    if (!handle->kind || strcmp(handle->kind, "wayland") != 0 || !handle->display) {
        printf("[CAKE] Window backend '%s' has no CAKE attach yet -- keyboard/mouse inactive\n",
               handle->kind ? handle->kind : "(untagged)");
        return;
    }

    // CAKE binds its own seat on the engine's wl_display -- a second bind
    // gets its own event stream, so GDMF's focus/cursor listeners and
    // CAKE's input listeners coexist without either knowing. Everything is
    // parked on CAKE's private queue BEFORE the listener is added (objects
    // inherit their parent's queue), so no CAKE event can ever land in the
    // default queue -- which belongs to GDMF's reader thread. The
    // queue-scoped roundtrips are legal alongside that thread: only
    // same-queue dispatch from two threads is forbidden.
    cake_display  = (struct wl_display*)handle->display;
    cake_queue    = wl_display_create_queue(cake_display);
    cake_registry = wl_display_get_registry(cake_display);
    wl_proxy_set_queue((struct wl_proxy*)cake_registry, cake_queue);
    wl_registry_add_listener(cake_registry, &cake_registry_listener, NULL);
    wl_display_roundtrip_queue(cake_display, cake_queue);   // collect globals
    wl_display_roundtrip_queue(cake_display, cake_queue);   // seat capabilities

    if (!cake_seat) {
        printf("[CAKE] Warning: compositor exposes no wl_seat -- no keyboard/mouse\n");
    }
    if (!cake_relManager) {
        printf("[CAKE] Warning: no relative-pointer protocol -- mouse deltas unavailable\n");
    }

    printf("[CAKE] Attached to application window\n");

    return;
}

void CAKE_Poll(void) {
    if (!cake_initialized) {
        // Keyboard/mouse were wired by CAKE_AttachWindow (main thread);
        // controllers need no window at all -- first poll scans for them.
        cake_scan_controllers();
        cake_initialized = true;
        printf("[CAKE] Initialised.\n");
    }

    // Run CAKE's listeners over whatever the reader thread's socket reads
    // have deposited on the private queue since last poll -- this is where
    // key/button/motion intake actually executes, on this thread.
    // Non-blocking: pending events only, never a socket read of its own.
    if (cake_queue) {
        wl_display_dispatch_queue_pending(cake_display, cake_queue);
    }

    // X11: drain CAKE's own connection -- raw-event intake executes right
    // here on the poll thread, the same seat the Wayland dispatch above
    // occupies when that backend is the one attached.
    if (cake_x11_display) {
        cake_x11_drain();
    }

    pthread_mutex_lock(&cake_intake_lock);

    memcpy(CAKE_Keys, cake_pend_keys, sizeof(CAKE_Keys));
    memcpy(CAKE_MouseButtons, cake_pend_buttons, sizeof(CAKE_MouseButtons));

    // Truncate-and-carry: the integer part becomes this poll's delta, the
    // sub-pixel remainder stays pending -- fine touchpad motion accumulates
    // instead of vanishing (same as the macOS backend).
    CAKE_MouseDeltaX = (int32_t)cake_pend_dx;   cake_pend_dx -= CAKE_MouseDeltaX;
    CAKE_MouseDeltaY = (int32_t)cake_pend_dy;   cake_pend_dy -= CAKE_MouseDeltaY;
    CAKE_MouseWheel  = (int32_t)cake_pend_wheel; cake_pend_wheel -= CAKE_MouseWheel;

    pthread_mutex_unlock(&cake_intake_lock);

    cake_poll_controllers();

    return;
}

void CAKE_Shutdown(void);   // defined below the controller machinery

void CAKE_Silence(void) {
    cake_silenced = true;

    return;
}

void CAKE_Resume(void) {
    cake_silenced = false;

    // See the Windows CAKE_Resume's own comment -- same reasoning: a
    // key/button released while silenced never reached the intake, so
    // without this it would read as still held down forever after.
    pthread_mutex_lock(&cake_intake_lock);
    memset(cake_pend_keys, 0, sizeof(cake_pend_keys));
    memset(cake_pend_buttons, 0, sizeof(cake_pend_buttons));
    cake_pend_dx = cake_pend_dy = cake_pend_wheel = 0.0;
    pthread_mutex_unlock(&cake_intake_lock);

    memset(CAKE_Keys, 0, sizeof(CAKE_Keys));
    memset(CAKE_MouseButtons, 0, sizeof(CAKE_MouseButtons));
    CAKE_MouseDeltaX = 0;
    CAKE_MouseDeltaY = 0;
    CAKE_MouseWheel = 0;

    return;
}

bool CAKE_IsSilenced(void) {
    return cake_silenced;
}

// --- Controllers -------------------------------------------------------------
// evdev gamepads, wholly on the CAKE_Poll thread: scanned from
// /dev/input/event* (a 2-second rescan, the same fallback cadence as the
// Windows re-enumeration timer), read non-blocking every poll, normalized
// through each device's own EVIOCGABS ranges so Xbox (xpad), PlayStation
// (hid-playstation), and Switch (hid-nintendo) pads all land in the
// XInput-shaped state. Slot policy and the LOST -> grace -> TIMEDOUT
// lifecycle mirror the other two backends; a pad that returns within the
// grace period reclaims its slot by name.

// The axes CAKE reads, in one queryable list.
static const int cake_lnx_axes[] = {
    ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y
};
#define CAKE_LNX_AXIS_COUNT ((int)(sizeof(cake_lnx_axes) / sizeof(cake_lnx_axes[0])))

typedef struct {
    char                 path[CAKE_CONTROLLER_PATH_MAX];
    char                 name[CAKE_CONTROLLER_NAME_MAX];
    int                  fd;          // -1 while absent
    bool                 present;
    uint64_t             timeLost;    // ms timestamp of disconnect; 0 = not lost
    uint16_t             vendorId;
    uint16_t             productId;
    struct input_absinfo abs[CAKE_LNX_AXIS_COUNT];
    bool                 hasAbs[CAKE_LNX_AXIS_COUNT];
    bool                 hasRumble;
    int                  ffId;        // uploaded FF_RUMBLE effect id, -1 = none
    CAKE_ControllerState live;        // running state events apply to
    CAKE_ControllerState state[2];
    volatile int         front;
    // Double buffered input; front swaps after every poll; state[1-front] is
    // the back buffer; state[front] is always safe to read.
} cake_lnx_controller;

static cake_lnx_controller cake_controllers[CAKE_CONTROLLER_MAX];

// Decoupled from cake_controllers[] so a TIMEDOUT code survives the memset
// that frees the slot for reuse -- same reasoning as the other backends.
static CAKE_ControllerConnectionState cake_controller_connstate[CAKE_CONTROLLER_MAX];

static uint64_t cake_lnx_lastScanMs = 0;

static uint64_t cake_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static bool cake_lnx_test_bit(const unsigned long* bits, int bit) {
    return (bits[bit / (8 * (int)sizeof(unsigned long))] >>
            (bit % (8 * (int)sizeof(unsigned long)))) & 1UL;
}

// A device counts as a gamepad when it reports the gamepad button cluster
// (BTN_GAMEPAD, aka BTN_SOUTH) -- the kernel's own definition. Keyboards,
// mice, and the motion-sensor sub-devices some pads expose all fail this.
static bool cake_lnx_is_gamepad(int fd) {
    unsigned long keyBits[(KEY_MAX / (8 * sizeof(unsigned long))) + 1] = { 0 };

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0) { return false; }

    return cake_lnx_test_bit(keyBits, BTN_GAMEPAD);
}

static int16_t cake_lnx_norm_stick(const struct input_absinfo* info, int value, bool invert) {
    if (info->maximum <= info->minimum) { return 0; }

    double t = (double)(value - info->minimum) / (double)(info->maximum - info->minimum);
    double scaled = t * 65535.0 - 32768.0;
    if (invert) { scaled = -scaled - 1.0; }   // evdev +Y is down; CAKE +Y is up
    if (scaled >  32767.0) { scaled =  32767.0; }
    if (scaled < -32768.0) { scaled = -32768.0; }

    return (int16_t)scaled;
}

static uint8_t cake_lnx_norm_trigger(const struct input_absinfo* info, int value) {
    if (info->maximum <= info->minimum) { return 0; }

    double t = (double)(value - info->minimum) / (double)(info->maximum - info->minimum);
    if (t < 0.0) { t = 0.0; }
    if (t > 1.0) { t = 1.0; }

    return (uint8_t)(t * 255.0);
}

static int cake_lnx_axis_index(int code) {
    for (int i = 0; i < CAKE_LNX_AXIS_COUNT; i++) {
        if (cake_lnx_axes[i] == code) { return i; }
    }

    return -1;
}

// Apply one evdev event to the live state. Buttons map per the label
// aliases the reference driver (xpad) actually emits: BTN_X is BTN_NORTH
// and BTN_Y is BTN_WEST, so an Xbox pad's labels land on their XInput
// namesakes exactly. (Pads whose drivers map by position instead -- DS4's
// triangle arrives as BTN_NORTH -- get triangle-as-X; a per-device table
// can refine that later if it ever matters. VERIFY with real hardware.)
static void cake_lnx_apply_event(cake_lnx_controller* c, const struct input_event* ev) {
    if (ev->type == EV_KEY) {
        uint16_t bit = 0;
        bool     down = ev->value != 0;

        switch (ev->code) {
        case BTN_SOUTH:      bit = CAKE_BUTTON_A;              break;
        case BTN_EAST:       bit = CAKE_BUTTON_B;              break;
        case BTN_NORTH:      bit = CAKE_BUTTON_X;              break;  // xpad: label X
        case BTN_WEST:       bit = CAKE_BUTTON_Y;              break;  // xpad: label Y
        case BTN_TL:         bit = CAKE_BUTTON_LEFT_SHOULDER;  break;
        case BTN_TR:         bit = CAKE_BUTTON_RIGHT_SHOULDER; break;
        case BTN_SELECT:     bit = CAKE_BUTTON_BACK;           break;
        case BTN_START:      bit = CAKE_BUTTON_START;          break;
        case BTN_THUMBL:     bit = CAKE_BUTTON_LEFT_THUMB;     break;
        case BTN_THUMBR:     bit = CAKE_BUTTON_RIGHT_THUMB;    break;
        case BTN_DPAD_UP:    bit = CAKE_BUTTON_DPAD_UP;        break;
        case BTN_DPAD_DOWN:  bit = CAKE_BUTTON_DPAD_DOWN;      break;
        case BTN_DPAD_LEFT:  bit = CAKE_BUTTON_DPAD_LEFT;      break;
        case BTN_DPAD_RIGHT: bit = CAKE_BUTTON_DPAD_RIGHT;     break;
        // Digital-only triggers (Switch pads): full-scale analog stand-in.
        case BTN_TL2:        c->live.leftTrigger  = down ? 255 : 0; return;
        case BTN_TR2:        c->live.rightTrigger = down ? 255 : 0; return;
        default:             return;
        }

        if (down) { c->live.buttons |= bit; }
        else      { c->live.buttons &= (uint16_t)~bit; }

        return;
    }

    if (ev->type == EV_ABS) {
        int idx = cake_lnx_axis_index(ev->code);
        if (idx < 0 || !c->hasAbs[idx]) { return; }
        const struct input_absinfo* info = &c->abs[idx];

        switch (ev->code) {
        case ABS_X:  c->live.thumbLeftX  = cake_lnx_norm_stick(info, ev->value, false); break;
        case ABS_Y:  c->live.thumbLeftY  = cake_lnx_norm_stick(info, ev->value, true);  break;
        case ABS_RX: c->live.thumbRightX = cake_lnx_norm_stick(info, ev->value, false); break;
        case ABS_RY: c->live.thumbRightY = cake_lnx_norm_stick(info, ev->value, true);  break;
        case ABS_Z:  c->live.leftTrigger  = cake_lnx_norm_trigger(info, ev->value);     break;
        case ABS_RZ: c->live.rightTrigger = cake_lnx_norm_trigger(info, ev->value);     break;
        case ABS_HAT0X:
            c->live.buttons &= (uint16_t)~(CAKE_BUTTON_DPAD_LEFT | CAKE_BUTTON_DPAD_RIGHT);
            if (ev->value < 0) { c->live.buttons |= CAKE_BUTTON_DPAD_LEFT; }
            if (ev->value > 0) { c->live.buttons |= CAKE_BUTTON_DPAD_RIGHT; }
            break;
        case ABS_HAT0Y:
            c->live.buttons &= (uint16_t)~(CAKE_BUTTON_DPAD_UP | CAKE_BUTTON_DPAD_DOWN);
            if (ev->value < 0) { c->live.buttons |= CAKE_BUTTON_DPAD_UP; }
            if (ev->value > 0) { c->live.buttons |= CAKE_BUTTON_DPAD_DOWN; }
            break;
        default: break;
        }

        return;
    }

    return;
}

// Take one open, verified gamepad into a slot (grace-period reclaim by
// name first, then first genuinely free slot -- same policy as the other
// backends). Returns false when every slot is taken.
static bool cake_lnx_adopt(int fd, const char* path) {
    char name[CAKE_CONTROLLER_NAME_MAX] = "Gamepad";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    int slot = -1;
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        if (!cake_controllers[i].present && cake_controllers[i].timeLost != 0 &&
            strncmp(cake_controllers[i].name, name, CAKE_CONTROLLER_NAME_MAX) == 0) {
            slot = i;   // came back within the grace period
            break;
        }
    }
    for (int i = 0; slot < 0 && i < CAKE_CONTROLLER_MAX; i++) {
        if (!cake_controllers[i].present && cake_controllers[i].timeLost == 0) { slot = i; }
    }
    if (slot < 0) { return false; }

    cake_lnx_controller* c = &cake_controllers[slot];
    memset(c, 0, sizeof(*c));
    c->fd = fd;
    c->present = true;
    snprintf(c->path, sizeof(c->path), "%s", path);
    snprintf(c->name, sizeof(c->name), "%s", name);

    struct input_id id;
    if (ioctl(fd, EVIOCGID, &id) == 0) {
        c->vendorId  = id.vendor;
        c->productId = id.product;
    }

    for (int i = 0; i < CAKE_LNX_AXIS_COUNT; i++) {
        c->hasAbs[i] = (ioctl(fd, EVIOCGABS(cake_lnx_axes[i]), &c->abs[i]) == 0 &&
                        c->abs[i].maximum > c->abs[i].minimum);
    }

    unsigned long ffBits[(FF_MAX / (8 * sizeof(unsigned long))) + 1] = { 0 };
    c->hasRumble = (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffBits)), ffBits) >= 0 &&
                    cake_lnx_test_bit(ffBits, FF_RUMBLE));
    c->ffId = -1;

    cake_controller_connstate[slot] = CAKE_CONN_ATTACHED;
    printf("[CAKE] Controller attached  | %s | slot %d%s\n",
           c->name, slot, c->hasRumble ? "" : " (no rumble)");

    return true;
}

// Scan /dev/input for gamepads not already sitting in a slot. Runs on the
// poll thread; called at init and then at most every 2 seconds.
static void cake_scan_controllers(void) {
    DIR* dir = opendir("/dev/input");
    if (!dir) { return; }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) { continue; }

        char path[CAKE_CONTROLLER_PATH_MAX];
        // The precision bound exists for -Wformat-truncation's sake; a
        // real /dev/input/eventNN name is ~10 characters.
        snprintf(path, sizeof(path), "/dev/input/%.240s", entry->d_name);

        bool taken = false;
        for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
            if (cake_controllers[i].present &&
                strncmp(cake_controllers[i].path, path, sizeof(path)) == 0) {
                taken = true;
                break;
            }
        }
        if (taken) { continue; }

        // O_RDWR for rumble; a device readable but not writable (uncommon
        // under logind's seat ACLs) still works minus vibration.
        int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) { fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC); }
        if (fd < 0) { continue; }

        if (!cake_lnx_is_gamepad(fd) || !cake_lnx_adopt(fd, path)) {
            close(fd);
        }
    }

    closedir(dir);

    return;
}

// Called from CAKE_Poll (sim thread): rescan on the 2s cadence, drain each
// present pad's events into its live state, publish through the double
// buffer, and run the LOST -> TIMEDOUT grace period.
static void cake_poll_controllers(void) {
    uint64_t now = cake_now_ms();

    if (now - cake_lnx_lastScanMs >= 2000) {
        cake_lnx_lastScanMs = now;
        cake_scan_controllers();
    }

    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        cake_lnx_controller* c = &cake_controllers[i];

        if (c->present) {
            struct input_event ev;
            ssize_t n;
            bool dead = false;

            while ((n = read(c->fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
                // SYN_DROPPED (event-queue overflow) would call for a full
                // EVIOCGKEY/EVIOCGABS resync; at one drain per tick it has
                // not been observed -- noted, not handled.
                cake_lnx_apply_event(c, &ev);
            }
            if (n < 0 && errno != EAGAIN) { dead = true; }   // ENODEV: unplugged

            if (dead) {
                close(c->fd);
                c->fd       = -1;
                c->present  = false;
                c->timeLost = now;
                cake_controller_connstate[i] = CAKE_CONN_LOST;
                printf("[CAKE] Controller detached  | %s | slot %d\n", c->name, i);
            } else {
                int back = 1 - c->front;
                c->state[back] = c->live;
                c->front = back;
            }
        }
        else if (c->timeLost != 0 && now - c->timeLost > CAKE_CONTROLLER_TIMEOUT_MS) {
            cake_controller_connstate[i] = CAKE_CONN_TIMEDOUT;
            printf("[CAKE] Controller timed out | %s | slot %d\n", c->name, i);
            memset(c, 0, sizeof(*c));
            c->fd = -1;
        }
    }

    return;
}

void CAKE_Shutdown(void) {
    if (!cake_initialized) { return; }

    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        if (cake_controllers[i].fd >= 0) { close(cake_controllers[i].fd); }
        memset(&cake_controllers[i], 0, sizeof(cake_controllers[i]));
        cake_controllers[i].fd = -1;
        cake_controller_connstate[i] = CAKE_CONN_EMPTY;
    }

    if (cake_relPointer) { zwp_relative_pointer_v1_destroy(cake_relPointer); cake_relPointer = NULL; }
    if (cake_relManager) { zwp_relative_pointer_manager_v1_destroy(cake_relManager); cake_relManager = NULL; }
    if (cake_keyboard)   { wl_keyboard_destroy(cake_keyboard); cake_keyboard = NULL; }
    if (cake_pointer)    { wl_pointer_destroy(cake_pointer);   cake_pointer = NULL; }
    if (cake_seat)       { wl_seat_destroy(cake_seat);         cake_seat = NULL; }
    if (cake_registry)   { wl_registry_destroy(cake_registry); cake_registry = NULL; }
    if (cake_queue)      { wl_event_queue_destroy(cake_queue); cake_queue = NULL; }
    cake_display = NULL;   // borrowed from GDMF; never disconnected here

    // The X11 connection is CAKE's own, unlike the borrowed wl_display.
    if (cake_x11_display) { XCloseDisplay(cake_x11_display); cake_x11_display = NULL; }

    cake_initialized = false;

    return;
}

bool CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) { return false; }

    return cake_controllers[slot].present;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) { return CAKE_CONN_EMPTY; }

    return cake_controller_connstate[slot];
}

const CAKE_ControllerState *CAKE_GetControllerState(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return NULL; }

    return &cake_controllers[slot].state[cake_controllers[slot].front];
}

const char *CAKE_GetControllerName(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return NULL; }

    return cake_controllers[slot].name;
}

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return CAKE_BACKEND_UNKNOWN; }

    return CAKE_BACKEND_HID;
}

uint16_t CAKE_GetControllerVendorID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return 0; }

    return cake_controllers[slot].vendorId;
}

uint16_t CAKE_GetControllerProductID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return 0; }

    return cake_controllers[slot].productId;
}

int CAKE_GetControllerXInputIndex(int slot) { (void)slot;

    return -1; }  // never XInput on this platform

// Rumble through evdev force feedback: one FF_RUMBLE effect per pad,
// re-uploaded in place on every speed change (same id), played with an
// unbounded duration -- XInput semantics, where a set speed holds until the
// next call changes it.
bool CAKE_SetControllerVibration(int slot, uint16_t leftMotor, uint16_t rightMotor) {
    if (!CAKE_IsControllerConnected(slot)) { return false; }

    cake_lnx_controller* c = &cake_controllers[slot];
    if (!c->hasRumble || c->fd < 0) { return false; }

    struct ff_effect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type                      = FF_RUMBLE;
    effect.id                        = c->ffId;   // -1 first time = allocate
    effect.u.rumble.strong_magnitude = leftMotor;   // low-frequency motor
    effect.u.rumble.weak_magnitude   = rightMotor;  // high-frequency motor
    effect.replay.length             = 0;           // unbounded, XInput-style

    if (ioctl(c->fd, EVIOCSFF, &effect) < 0) { return false; }
    c->ffId = effect.id;

    struct input_event play;
    memset(&play, 0, sizeof(play));
    play.type  = EV_FF;
    play.code  = (uint16_t)c->ffId;
    play.value = (leftMotor || rightMotor) ? 1 : 0;

    return write(c->fd, &play, sizeof(play)) == (ssize_t)sizeof(play);
}

#endif
