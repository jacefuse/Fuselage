// Runtime XInput binding and controller identity. SEE: (CAKE.md, Windows controllers)
#include "cake_win32_controller_private.h"
#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

bool cake_win32_is_xinput_path(const char* path) {
    return strstr(path, "IG_") != NULL || strstr(path, "ig_") != NULL;
}

typedef DWORD(WINAPI* cake_win32_xinput_get_state_callback)(DWORD, XINPUT_STATE*);
typedef DWORD(WINAPI* cake_win32_xinput_set_state_callback)(DWORD, XINPUT_VIBRATION*);
typedef DWORD(WINAPI* cake_win32_xinput_get_capabilities_callback)(DWORD, DWORD, XINPUT_CAPABILITIES*);
typedef DWORD(WINAPI* cake_win32_xinput_get_battery_information_callback)(DWORD, BYTE,
                                                                          XINPUT_BATTERY_INFORMATION*);

static bool cake_win32_xinput_tried = false;
static cake_win32_xinput_get_state_callback cake_win32_xinput_get_state = NULL;
static cake_win32_xinput_set_state_callback cake_win32_xinput_set_state = NULL;
static cake_win32_xinput_get_capabilities_callback cake_win32_xinput_get_capabilities = NULL;
static cake_win32_xinput_get_battery_information_callback cake_win32_xinput_get_battery_information = NULL;

void cake_win32_xinput_load(void) {
    if (cake_win32_xinput_tried) {
        return;
    }
    cake_win32_xinput_tried = true;

    static const char* const names[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};

    for (int index = 0; index < (int)(sizeof names / sizeof names[0]); index++) {
        HMODULE module = LoadLibraryExA(names[index], NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) {
            continue;
        }

        cake_win32_xinput_get_state_callback get_state =
            (cake_win32_xinput_get_state_callback)(void (*)(void))GetProcAddress(module, "XInputGetState");
        cake_win32_xinput_set_state_callback set_state =
            (cake_win32_xinput_set_state_callback)(void (*)(void))GetProcAddress(module, "XInputSetState");
        cake_win32_xinput_get_capabilities_callback get_capabilities =
            (cake_win32_xinput_get_capabilities_callback)(void (*)(void))GetProcAddress(
                module, "XInputGetCapabilities");

        if (!get_state || !set_state || !get_capabilities) {
            FreeLibrary(module);
            continue;
        }

        cake_win32_xinput_get_state = get_state;
        cake_win32_xinput_set_state = set_state;
        cake_win32_xinput_get_capabilities = get_capabilities;
        cake_win32_xinput_get_battery_information =
            (cake_win32_xinput_get_battery_information_callback)(void (*)(void))GetProcAddress(
                module, "XInputGetBatteryInformation");

        printf("[CAKE] XInput loaded from %s\n", names[index]);
        return;
    }

    printf("[CAKE] Warning: no XInput DLL found (xinput1_4, xinput1_3, xinput9_1_0); "
           "XInput controllers disabled, keyboard and mouse unaffected\n");

    return;
}

static bool cake_win32_xinput_index_taken(int xinput_index) {
    for (int index = 0; index < CAKE_CONTROLLER_MAX; index++) {
        if (cake_win32_controllers[index].identity_valid &&
            cake_win32_controllers[index].identity.backend == CAKE_BACKEND_XINPUT &&
            cake_win32_controllers[index].identity.xinput_index == xinput_index) {
            return true;
        }
    }

    return false;
}

void cake_win32_identify_controller(cake_win32_controller* controller) {
    cake_win32_controller_identity* identity = &controller->identity;

    memset(identity, 0, sizeof(*identity));
    identity->xinput_index = -1;

    HANDLE device_handle = CreateFileA(controller->device_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                       OPEN_EXISTING, 0, NULL);
    if (device_handle != INVALID_HANDLE_VALUE) {
        HIDD_ATTRIBUTES attributes = {0};

        attributes.Size = sizeof(attributes);
        if (HidD_GetAttributes(device_handle, &attributes)) {
            identity->vendor_id = attributes.VendorID;
            identity->product_id = attributes.ProductID;
        }

        wchar_t wide_name[CAKE_CONTROLLER_NAME_MAX];
        if (HidD_GetProductString(device_handle, wide_name, sizeof wide_name)) {
            char device_name[CAKE_CONTROLLER_NAME_MAX];
            int name_length = 0;
            for (int index = 0; wide_name[index] && name_length < CAKE_CONTROLLER_NAME_MAX - 1; index++) {
                device_name[name_length++] =
                    (wide_name[index] < 0x20 || wide_name[index] > 0x7E) ? '?' : (char)wide_name[index];
            }
            while (name_length > 0 && device_name[name_length - 1] == ' ') {
                name_length--;
            }
            device_name[name_length] = '\0';
            if (name_length > 0) {
                strncpy(controller->name, device_name, CAKE_CONTROLLER_NAME_MAX - 1);
                controller->name[CAKE_CONTROLLER_NAME_MAX - 1] = '\0';
            }
        }
        CloseHandle(device_handle);
    }

    if (cake_win32_is_xinput_path(controller->device_path)) {
        identity->backend = CAKE_BACKEND_XINPUT;
        identity->xinput_index = -1;

        XINPUT_STATE input_state;
        for (int index = 0; cake_win32_xinput_get_state && index < 4; index++) {
            if (cake_win32_xinput_index_taken(index)) {
                continue;
            }
            if (cake_win32_xinput_get_state(index, &input_state) == ERROR_SUCCESS) {
                identity->xinput_index = index;
                cake_win32_xinput_get_capabilities(index, 0, &identity->capabilities);
                if (cake_win32_xinput_get_battery_information) {
                    cake_win32_xinput_get_battery_information(index, BATTERY_DEVTYPE_GAMEPAD,
                                                              &identity->battery);
                }
                break;
            }
        }
    } else {
        identity->backend = CAKE_BACKEND_HID;
    }

    controller->identity_valid = true;

    return;
}

void cake_win32_poll_xinput(cake_win32_controller* controller) {
    XINPUT_STATE input_state;

    if (!cake_win32_xinput_get_state || controller->identity.xinput_index < 0) {
        return;
    }

    int back_buffer = 1 - controller->front_buffer;
    CAKE_ControllerState* controller_state = &controller->state[back_buffer];

    if (cake_win32_xinput_get_state(controller->identity.xinput_index, &input_state) != ERROR_SUCCESS) {

        memset(controller_state, 0, sizeof *controller_state);
        controller->front_buffer = back_buffer;
        return;
    }

    controller_state->buttons = input_state.Gamepad.wButtons;
    controller_state->leftTrigger = input_state.Gamepad.bLeftTrigger;
    controller_state->rightTrigger = input_state.Gamepad.bRightTrigger;
    controller_state->thumbLeftX = input_state.Gamepad.sThumbLX;
    controller_state->thumbLeftY = input_state.Gamepad.sThumbLY;
    controller_state->thumbRightX = input_state.Gamepad.sThumbRX;
    controller_state->thumbRightY = input_state.Gamepad.sThumbRY;

    controller->front_buffer = back_buffer;

    return;
}

bool CAKE_SetControllerVibration(int slot, uint16_t left_motor, uint16_t right_motor) {
    if (!CAKE_IsControllerConnected(slot)) {
        return false;
    }

    if (cake_win32_controllers[slot].identity.backend != CAKE_BACKEND_XINPUT) {

        return false;
    }

    if (!cake_win32_xinput_set_state || cake_win32_controllers[slot].identity.xinput_index < 0) {
        return false;
    }

    XINPUT_VIBRATION vibration = {.wLeftMotorSpeed = left_motor, .wRightMotorSpeed = right_motor};

    return cake_win32_xinput_set_state(cake_win32_controllers[slot].identity.xinput_index, &vibration) ==
           ERROR_SUCCESS;
}
#endif
