// cake.c - Controller and Keyboard Events - BUTTOCKS 2026 - Fuselage 0.2.2026060901

#include "cake.h"

#include <stdio.h> // Probably no longer necessary
//#include <stdlib.h>
#include <string.h>

uint8_t CAKE_Keys[CAKE_KEY_TABLE_SIZE] =                { 0 };
uint8_t CAKE_MouseButtons[CAKE_MOUSE_BUTTON_COUNT] =    { 0 };
int32_t CAKE_MouseX = 0;
int32_t CAKE_MouseY = 0;
int32_t CAKE_MouseW = 0;

// Windows Eventually moved to its own file
#if defined(_WIN32)

#include <dbt.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

// Internal controler types
typedef struct {
    int                         xinput_index;
    uint16_t                    vendor_id;
    uint16_t                    product_id;
    CAKE_ControllerBackend      backend;
    XINPUT_CAPABILITIES          xcapabilities;
    XINPUT_BATTERY_INFORMATION   battery;
} cake_ControllerIdentity;

typedef struct {
    char                        device_id[CAKE_CONTROLLER_PATH_MAX];
    char                        name[CAKE_CONTROLLER_NAME_MAX];
    int                         chain_index;
    uint64_t                    time_attached;
    uint64_t                    time_last_seen;
    uint64_t                    time_lost;
    int                         identity_valid; // [CHANGE TO BOOL?]
    cake_ControllerIdentity     identity; // [CHANGE]
    CAKE_ControllerState        state[2];
    volatile int                front;
    // For double buffered input; Front swaps after every poll;
    // state[1-front] is the back buffer; state[front] is always safe to read;
} cake_ControllerDevice;

// Stats
#define CAKE_TIMER_ID                   1
#define CAKE_TIMER_MS                   2000

static HWND       cake_hwnd             = NULL;
static HDEVNOTIFY cake_notif            = NULL; //[CHANGE]
static int        cake_initialized      = 0; // [CHANGE TO BOOL?]

static cake_ControllerDevice cake_controllers[CAKE_CONTROLLER_MAX];

// Decoupled from cake_controllers[] so a TIMEDOUT code survives the memset
// that frees the slot for reuse below -- otherwise it would be erased in the
// same pass that sets it, and would never be observable by a caller.
static CAKE_ControllerConnectionState cake_controller_connstate[CAKE_CONTROLLER_MAX];

static const GUID CAKE_GUID_HID = {
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

// Left Shift Defferred Commit State; Still needs work
static uint8_t cake_lshift_pending       = 0;
static int     cake_lshift_pending_valid = 0;

// Keyboard
static void cake_flush_lshift(void) {
    if (cake_lshift_pending_valid) {
        CAKE_Keys[0xE1]           = cake_lshift_pending;
        cake_lshift_pending_valid = 0;
    }

    return;
}

static void cake_handle_keyboard(RAWKEYBOARD *keyboard) {
    uint16_t    scancode    = keyboard->MakeCode;
    int         is_e0       = (keyboard->Flags & RI_KEY_E0)     ? 1 : 0;
    int         is_e1       = (keyboard->Flags & RI_KEY_E1)     ? 1 : 0;
    int         is_break    = (keyboard->Flags & RI_KEY_BREAK)  ? 1 : 0;
    uint8_t     state       = is_break                          ? 0 : 1;

    if (is_e1 && scancode == 0x45) {
        CAKE_Keys[0x48]     = state;
        return;
    }

    if (scancode == 0x2A && !is_e0) {
        cake_flush_lshift();
        cake_lshift_pending         = state;
        cake_lshift_pending_valid   = 1;
        return;
    }

    if (is_e0) {
        if (cake_lshift_pending_valid){
            cake_lshift_pending_valid = 0;
            if (GetAsyncKeyState(VK_LSHIFT) & 0x8000){
                CAKE_Keys[0xE1] = 1;
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
        CAKE_MouseX += mouse->lLastX;
        CAKE_MouseY += mouse->lLastY;
    }

    if (flags & RI_MOUSE_WHEEL) { CAKE_MouseW += (int32_t)(SHORT)mouse->usButtonData; }

    if (flags & RI_MOUSE_LEFT_BUTTON_DOWN)   { CAKE_MouseButtons[CAKE_MOUSE_LEFT]   = 1; }
    if (flags & RI_MOUSE_LEFT_BUTTON_UP)     { CAKE_MouseButtons[CAKE_MOUSE_LEFT]   = 0; }
    if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN)  { CAKE_MouseButtons[CAKE_MOUSE_RIGHT]  = 1; }
    if (flags & RI_MOUSE_RIGHT_BUTTON_UP)    { CAKE_MouseButtons[CAKE_MOUSE_RIGHT]  = 0; }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) { CAKE_MouseButtons[CAKE_MOUSE_MIDDLE] = 1; }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_UP)   { CAKE_MouseButtons[CAKE_MOUSE_MIDDLE] = 0; }
    if (flags & RI_MOUSE_BUTTON_4_DOWN)      { CAKE_MouseButtons[CAKE_MOUSE_X1]     = 1; }
    if (flags & RI_MOUSE_BUTTON_4_UP)        { CAKE_MouseButtons[CAKE_MOUSE_X1]     = 0; }
    if (flags & RI_MOUSE_BUTTON_5_DOWN)      { CAKE_MouseButtons[CAKE_MOUSE_X2]     = 1; }
    if (flags & RI_MOUSE_BUTTON_5_UP)        { CAKE_MouseButtons[CAKE_MOUSE_X2]     = 0; }

    return;
}

// Controller helpers
static int cake_is_xinput_path(const char *path) {
    return strstr(path, "IG_") != NULL ||
           strstr(path, "ig_") != NULL;
}

static int cake_is_controller_path(const char *path) {
    HANDLE h = CreateFileA(
        path, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL
    );

    if (h == INVALID_HANDLE_VALUE) { return 0; }

    PHIDP_PREPARSED_DATA preparsed = NULL;
    if (!HidD_GetPreparsedData(h, &preparsed)) {
        CloseHandle(h);
        return 0;
    }

    HIDP_CAPS caps;
    NTSTATUS  status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    CloseHandle(h);

    if (status != HIDP_STATUS_SUCCESS) { return 0; }

    return caps.UsagePage == 0x01 &&
           (caps.Usage == 0x04 || caps.Usage == 0x05);
}

/*/ Deprecated
static int cake_parse_xinput_index(const char *path) {
    const char *p = strstr(path, "IG_");
    if (!p) p = strstr(path, "ig_");
    if (!p) return -1;
    p += 3;
    char *end;
    long val = strtol(p, &end, 16);
    if (end == p || val < 0 || val > 3) return -1;
    return (int)val;
} */

/*/ Deprecated
static int cake_count_xinput_slots(void) {
    int n = 0;
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++)
        if (cake_controllers[i].identity_valid &&
            cake_controllers[i].identity.backend == CAKE_BACKEND_XINPUT)
            n++;
    return n;
} */
static int cake_xinput_index_taken(int xinput_index) {
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++)
        if (cake_controllers[i].identity_valid &&
            cake_controllers[i].identity.backend == CAKE_BACKEND_XINPUT &&
            cake_controllers[i].identity.xinput_index == xinput_index) { return 1; }

    return 0;
}

// Controller ID
static void cake_identify_controller(cake_ControllerDevice *controller) {
    cake_ControllerIdentity *id = &controller->identity;

    memset(id, 0, sizeof(*id));
    id->xinput_index = -1;

    HANDLE h = CreateFileA(
        controller->device_id, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL
    );
    if (h != INVALID_HANDLE_VALUE) {
        HIDD_ATTRIBUTES attr = {0};

        attr.Size = sizeof(attr);
        if (HidD_GetAttributes(h, &attr)) {
            id->vendor_id  = attr.VendorID;
            id->product_id = attr.ProductID;
        }
        CloseHandle(h);
    }

    /* Replaced with version below. Kept for now for comparison.
    if (cake_is_xinput_path(controller->device_id) && cake_count_xinput_slots() < 4) {
        id->backend      = CAKE_BACKEND_XINPUT;
        id->xinput_index = cake_parse_xinput_index(controller->device_id);
        if (id->xinput_index >= 0) {
            XInputGetCapabilities(id->xinput_index, 0, &id->xcapabilities);
            XInputGetBatteryInformation(
                id->xinput_index, BATTERY_DEVTYPE_GAMEPAD, &id->battery
            );
        }
    }
    else {
        id->backend = CAKE_BACKEND_HID;
    } */
    if (cake_is_xinput_path(controller->device_id)) {
        id->backend = CAKE_BACKEND_XINPUT;
        id->xinput_index = -1;

        XINPUT_STATE xs;
        for (int i = 0; i < 4; i++) {
            if (cake_xinput_index_taken(i)) { continue; }
            if (XInputGetState(i, &xs) == ERROR_SUCCESS) {
                id->xinput_index = i;
                XInputGetCapabilities(i, 0, &id->xcapabilities);
                XInputGetBatteryInformation(i, BATTERY_DEVTYPE_GAMEPAD, &id->battery);
                break;
            }
        }
    } else {
        id->backend = CAKE_BACKEND_HID;
    }

    controller->identity_valid = 1;

    return;
}

// Controller input polling
static void cake_poll_xinput(cake_ControllerDevice *controller) {
    XINPUT_STATE xinputstate;

    if (XInputGetState(controller->identity.xinput_index, &xinputstate) != ERROR_SUCCESS) { return; }

    int back                                = 1 - controller->front;
    CAKE_ControllerState *controllerstate   = &controller->state[back];

    controllerstate->buttons        = xinputstate.Gamepad.wButtons;
    controllerstate->left_trigger   = xinputstate.Gamepad.bLeftTrigger;
    controllerstate->right_trigger  = xinputstate.Gamepad.bRightTrigger;
    controllerstate->thumb_lx       = xinputstate.Gamepad.sThumbLX;
    controllerstate->thumb_ly       = xinputstate.Gamepad.sThumbLY;
    controllerstate->thumb_rx       = xinputstate.Gamepad.sThumbRX;
    controllerstate->thumb_ry       = xinputstate.Gamepad.sThumbRY;

    controller->front               = back;

    return;
}

// Called from CAKE_Poll()
static void cake_poll_controllers(void) {
    for (int i = 0; i < CAKE_CONTROLLER_MAX; i++) {
        cake_ControllerDevice *controller = &cake_controllers[i];

        if (controller->device_id[0] == '\0') { continue; }
        if (!controller->identity_valid)      { continue; }

        switch (controller->identity.backend) {
            case CAKE_BACKEND_XINPUT:
                cake_poll_xinput(controller);

                break;
            case CAKE_BACKEND_HID:
                /*
                CAKE does not currentl handle input for HID or DINPUT
                */
                break;
            default:
                break;
        }
    }

    return;
}

// Controller Enumeration **** NEEDS WORK! KNOWN ISSUES ****
static void cake_enumerate_controllers(void) {
    ULONGLONG now                       =   GetTickCount64();
    BOOL      seen[CAKE_CONTROLLER_MAX] =   {0};

    HDEVINFO dev_info                   =   SetupDiGetClassDevsA(&CAKE_GUID_HID, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (dev_info == INVALID_HANDLE_VALUE) { return; }

    SP_DEVICE_INTERFACE_DATA iface      =   {0};
    iface.cbSize                        =   sizeof(iface);

    char detail_buf[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + CAKE_CONTROLLER_PATH_MAX];
    SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A *)detail_buf;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info, NULL, &CAKE_GUID_HID, i, &iface); i++){
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(dev_info, &iface, detail, sizeof(detail_buf), NULL, NULL)) { continue; }

        const char *path = detail->DevicePath;
        if (!cake_is_controller_path(path)) { continue; }

        const char *name = cake_is_xinput_path(path) ? "XInput Controller" : "Unknown Controller";

        int slot = -1;
        for (int j = 0; j < CAKE_CONTROLLER_MAX; j++) {
            if (cake_controllers[j].device_id[0] != '\0' &&
                strcmp(cake_controllers[j].device_id, path) == 0) {
                slot = j;
                break;
            }
        }

        if (slot >= 0) {
            seen[slot] = TRUE;
            cake_controllers[slot].time_last_seen = now;

            if (cake_controllers[slot].time_lost != 0) {
                cake_controllers[slot].time_lost = 0;
                cake_controller_connstate[slot] = CAKE_CONN_ATTACHED;
                printf("[CAKE] Controller returned  | %s | slot %d\n",
                       cake_controllers[slot].name, slot);
            }
        }
        else {
            for (int j = 0; j < CAKE_CONTROLLER_MAX; j++) {
                if (cake_controllers[j].device_id[0] == '\0') {
                    strncpy(cake_controllers[j].device_id, path,
                            CAKE_CONTROLLER_PATH_MAX - 1);
                    strncpy(cake_controllers[j].name, name,
                            CAKE_CONTROLLER_NAME_MAX - 1);
                    cake_controllers[j].chain_index    = j;
                    cake_controllers[j].time_attached  = now;
                    cake_controllers[j].time_last_seen = now;
                    cake_controllers[j].time_lost      = 0;
                    cake_controllers[j].identity_valid = 0;
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
                           j, id->vendor_id, id->product_id,
                           id->backend == CAKE_BACKEND_XINPUT ? "XInput" : "HID");
                    if (id->backend == CAKE_BACKEND_XINPUT && id->xinput_index >= 0) { printf(" | xinput_index %d", id->xinput_index); }
                    printf("\n");
                    break;
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);

    for (int j = 0; j < CAKE_CONTROLLER_MAX; j++) {
        if (cake_controllers[j].device_id[0] == '\0') { continue; }
        if (seen[j]) { continue; }

        if (cake_controllers[j].time_lost == 0) {
            cake_controllers[j].time_lost = now;
            cake_controller_connstate[j] = CAKE_CONN_LOST;
            printf("[CAKE] Controller detached  | %s | slot %d\n",
                   cake_controllers[j].name, j);
        }
        else if (now - cake_controllers[j].time_lost > CAKE_CONTROLLER_TIMEOUT_MS) {
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
    dbi.dbcc_classguid  = CAKE_GUID_HID;

    cake_notif = RegisterDeviceNotificationA(
        cake_hwnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE
    );

    /* Fallback periodic re-enumeration in case any notifications are missed. */
    SetTimer(cake_hwnd, CAKE_TIMER_ID, CAKE_TIMER_MS, NULL);

    cake_initialized = 1;

    printf("[CAKE] Initialised. Scanning for controllers...\n");
    cake_enumerate_controllers();

    return;
}

// Public Facing Functions
void CAKE_Poll(void) {
    if (!cake_initialized) { cake_init(); }

    CAKE_MouseX = 0;
    CAKE_MouseY = 0;
    CAKE_MouseW = 0;

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

    if (cake_notif) {
        UnregisterDeviceNotification(cake_notif);
        cake_notif = NULL;
    }

    if (cake_hwnd) {
        DestroyWindow(cake_hwnd);
        cake_hwnd = NULL;
    }

    cake_initialized = 0;

    return;
}

int CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX)  { return 0; }
    if (cake_controllers[slot].device_id[0] == '\0') { return 0; }
    if (!cake_controllers[slot].identity_valid)      { return 0; }

    return 1;
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

    return cake_controllers[slot].identity.vendor_id;
}

uint16_t CAKE_GetControllerProductID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return 0; }

    return cake_controllers[slot].identity.product_id;
}

int CAKE_GetControllerXInputIndex(int slot) {
    if (!CAKE_IsControllerConnected(slot)) { return -1; }
    if (cake_controllers[slot].identity.backend != CAKE_BACKEND_XINPUT) { return -1; }

    return cake_controllers[slot].identity.xinput_index;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) { return CAKE_CONN_EMPTY; }

    return cake_controller_connstate[slot];
}

// LINUX -- Will be moved to its own file when implemented
#elif defined(__linux__)

void CAKE_Poll(void)     {
    return;
}
void CAKE_Shutdown(void) {
    return;
}

int                             CAKE_IsControllerConnected(int slot)        { (void)slot;

 return 0; }
CAKE_ControllerConnectionState  CAKE_GetControllerConnectionState(int slot) { (void)slot;

 return CAKE_CONN_EMPTY; }
const CAKE_ControllerState     *CAKE_GetControllerState(int slot)           { (void)slot;

 return NULL; }
const char                     *CAKE_GetControllerName(int slot)            { (void)slot;

 return NULL; }
CAKE_ControllerBackend          CAKE_GetControllerBackend(int slot)         { (void)slot;

 return CAKE_BACKEND_UNKNOWN; }
uint16_t                        CAKE_GetControllerVendorID(int slot)        { (void)slot;

 return 0; }
uint16_t                        CAKE_GetControllerProductID(int slot)       { (void)slot;

 return 0; }
int                             CAKE_GetControllerXInputIndex(int slot)     { (void)slot;

 return -1; }

// MACOS -- Will be moved to its own file when implemented
#elif defined(__APPLE__)

#include <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>

static IOHIDManagerRef cake_hid_manager = NULL;
static int             cake_initialized = 0;

static CFDictionaryRef cake_hid_match(uint32_t page, uint32_t usage) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    int page_i  = (int)page;
    int usage_i = (int)usage;
    CFNumberRef page_num  = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page_i);
    CFNumberRef usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage_i);

    CFDictionarySetValue(d, CFSTR(kIOHIDDeviceUsagePageKey), page_num);
    CFDictionarySetValue(d, CFSTR(kIOHIDDeviceUsageKey),     usage_num);
    CFRelease(page_num);
    CFRelease(usage_num);

    return d;
}

static void cake_input_callback(void *ctx, IOReturn result, void *sender, IOHIDValueRef value) {
    (void)ctx; (void)result; (void)sender;

    IOHIDElementRef elem       = IOHIDValueGetElement(value);
    uint32_t        usage_page = IOHIDElementGetUsagePage(elem);
    uint32_t        usage      = IOHIDElementGetUsage(elem);
    CFIndex         val        = IOHIDValueGetIntegerValue(value);

    if (usage_page == 0x07 && usage >= 4 && usage <= 0xE7) {
        CAKE_Keys[(uint8_t)usage] = val ? 1 : 0;
        return;
    }

    if (usage_page == 0x01) {
        if      (usage == 0x30) { CAKE_MouseDX += (int32_t)val; }
        else if (usage == 0x31) { CAKE_MouseDY += (int32_t)val; }
        else if (usage == 0x38) { CAKE_MouseDW += (int32_t)val * 120; }
        return;
    }

    if (usage_page == 0x09 && usage >= 1 && usage <= CAKE_MOUSE_BUTTON_COUNT) {
        CAKE_MouseButtons[usage - 1] = val ? 1 : 0;
        return;
    }

    return;
}

static void cake_init(void) {
    cake_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    CFDictionaryRef  kb_match    = cake_hid_match(0x01, 0x06);
    CFDictionaryRef  ptr_match   = cake_hid_match(0x01, 0x01);
    CFDictionaryRef  mouse_match = cake_hid_match(0x01, 0x02);
    const void      *matches[3]  = { kb_match, ptr_match, mouse_match };
    CFArrayRef       criteria    = CFArrayCreate(kCFAllocatorDefault, matches, 3,
                                                 &kCFTypeArrayCallBacks);
    CFRelease(kb_match);
    CFRelease(ptr_match);
    CFRelease(mouse_match);

    IOHIDManagerSetDeviceMatchingMultiple(cake_hid_manager, criteria);
    CFRelease(criteria);

    IOHIDManagerRegisterInputValueCallback(cake_hid_manager, cake_input_callback, NULL);
    IOHIDManagerScheduleWithRunLoop(cake_hid_manager, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);
    IOHIDManagerOpen(cake_hid_manager, kIOHIDOptionsTypeNone);

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);

    cake_initialized = 1;

    return;
}

void CAKE_Poll(void) {
    if (!cake_initialized) { cake_init(); }

    CAKE_MouseDX = 0;
    CAKE_MouseDY = 0;
    CAKE_MouseDW = 0;

    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource)
        ;

    return;
}

void CAKE_Shutdown(void) {
    if (!cake_initialized) { return; }

    if (cake_hid_manager) {
        IOHIDManagerUnscheduleFromRunLoop(cake_hid_manager,
                                          CFRunLoopGetCurrent(),
                                          kCFRunLoopDefaultMode);
        IOHIDManagerClose(cake_hid_manager, kIOHIDOptionsTypeNone);
        CFRelease(cake_hid_manager);
        cake_hid_manager = NULL;
    }

    cake_initialized = 0;

    return;
}

int                             CAKE_IsControllerConnected(int slot)        { (void)slot;

 return 0; }
CAKE_ControllerConnectionState  CAKE_GetControllerConnectionState(int slot) { (void)slot;

 return CAKE_CONN_EMPTY; }
const CAKE_ControllerState     *CAKE_GetControllerState(int slot)           { (void)slot;

 return NULL; }
const char                     *CAKE_GetControllerName(int slot)            { (void)slot;

 return NULL; }
CAKE_ControllerBackend          CAKE_GetControllerBackend(int slot)         { (void)slot;

 return CAKE_BACKEND_UNKNOWN; }
uint16_t                        CAKE_GetControllerVendorID(int slot)        { (void)slot;

 return 0; }
uint16_t                        CAKE_GetControllerProductID(int slot)       { (void)slot;

 return 0; }
int                             CAKE_GetControllerXInputIndex(int slot)     { (void)slot;

 return -1; }

#endif