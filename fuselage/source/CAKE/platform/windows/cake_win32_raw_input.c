// Validate packet bounds before dispatch. SEE: (CAKE.md, Windows input)
#include "cake_win32_private.h"
#if defined(_WIN32)
#include <stddef.h>

void cake_win32_dispatch_raw_input(HRAWINPUT input, bool silenced) {
    union {
        RAWINPUT raw;
        uint8_t bytes[4096];
    } packet;
    UINT capacity = sizeof(packet);
    UINT copied = GetRawInputData(input, RID_INPUT, &packet, &capacity, sizeof(RAWINPUTHEADER));
    if (copied == (UINT)-1 || copied < sizeof(RAWINPUTHEADER) || copied > sizeof(packet) ||
        packet.raw.header.dwSize < sizeof(RAWINPUTHEADER) || packet.raw.header.dwSize > copied) {
        return;
    }
    size_t size = packet.raw.header.dwSize;
    switch (packet.raw.header.dwType) {
    case RIM_TYPEKEYBOARD:
        if (!silenced && size >= offsetof(RAWINPUT, data.keyboard) + sizeof(RAWKEYBOARD)) {
            cake_win32_handle_keyboard(&packet.raw.data.keyboard);
        }
        break;
    case RIM_TYPEMOUSE:
        if (!silenced && size >= offsetof(RAWINPUT, data.mouse) + sizeof(RAWMOUSE)) {
            cake_win32_handle_mouse(&packet.raw.data.mouse);
        }
        break;
    case RIM_TYPEHID: {
        size_t payload_offset = offsetof(RAWINPUT, data.hid.bRawData);
        if (size < payload_offset) {
            break;
        }
        const RAWHID* report = &packet.raw.data.hid;
        if (report->dwSizeHid && report->dwCount &&
            report->dwCount <= (size - payload_offset) / report->dwSizeHid) {
            cake_win32_handle_controller_input(packet.raw.header.hDevice, report);
        }
        break;
    }
    default:
        break;
    }
}
#endif
