// Windows keyboard and mouse intake. SEE: (CAKE.md, Windows input)
#include "cake_win32_private.h"
#if defined(_WIN32)
#include <limits.h>

static void cake_win32_set_key(uint8_t key_code, bool down) {
    if (CAKE_Keys[key_code] != down) {
        cake_latch_key(key_code, down);
    }
    CAKE_Keys[key_code] = down;
}
static int32_t cake_win32_add_motion(int32_t value, int32_t delta) {
    int64_t sum = (int64_t)value + delta;
    if (sum > INT32_MAX) {
        return INT32_MAX;
    }
    if (sum < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)sum;
}
static const uint8_t cake_win32_scan_to_key[256] = {
    [0x01] = 0x29, [0x02] = 0x1E, [0x03] = 0x1F, [0x04] = 0x20, [0x05] = 0x21, [0x06] = 0x22, [0x07] = 0x23,
    [0x08] = 0x24, [0x09] = 0x25, [0x0A] = 0x26, [0x0B] = 0x27, [0x0C] = 0x2D, [0x0D] = 0x2E, [0x0E] = 0x2A,
    [0x0F] = 0x2B, [0x10] = 0x14, [0x11] = 0x1A, [0x12] = 0x08, [0x13] = 0x15, [0x14] = 0x17, [0x15] = 0x1C,
    [0x16] = 0x18, [0x17] = 0x0C, [0x18] = 0x12, [0x19] = 0x13, [0x1A] = 0x2F, [0x1B] = 0x30, [0x1C] = 0x28,
    [0x1D] = 0xE0, [0x1E] = 0x04, [0x1F] = 0x16, [0x20] = 0x07, [0x21] = 0x09, [0x22] = 0x0A, [0x23] = 0x0B,
    [0x24] = 0x0D, [0x25] = 0x0E, [0x26] = 0x0F, [0x27] = 0x33, [0x28] = 0x34, [0x29] = 0x35, [0x2A] = 0xE1,
    [0x2B] = 0x31, [0x2C] = 0x1D, [0x2D] = 0x1B, [0x2E] = 0x06, [0x2F] = 0x19, [0x30] = 0x05, [0x31] = 0x11,
    [0x32] = 0x10, [0x33] = 0x36, [0x34] = 0x37, [0x35] = 0x38, [0x36] = 0xE5, [0x37] = 0x55, [0x38] = 0xE2,
    [0x39] = 0x2C, [0x3A] = 0x39, [0x3B] = 0x3A, [0x3C] = 0x3B, [0x3D] = 0x3C, [0x3E] = 0x3D, [0x3F] = 0x3E,
    [0x40] = 0x3F, [0x41] = 0x40, [0x42] = 0x41, [0x43] = 0x42, [0x44] = 0x43, [0x45] = 0x53, [0x46] = 0x47,
    [0x47] = 0x5F, [0x48] = 0x60, [0x49] = 0x61, [0x4A] = 0x56, [0x4B] = 0x5C, [0x4C] = 0x5D, [0x4D] = 0x5E,
    [0x4E] = 0x57, [0x4F] = 0x59, [0x50] = 0x5A, [0x51] = 0x5B, [0x52] = 0x62, [0x53] = 0x63, [0x56] = 0x64,
    [0x57] = 0x44, [0x58] = 0x45};

static const uint8_t cake_win32_extended_scan_to_key[256] = {
    [0x1C] = 0x58, [0x1D] = 0xE4, [0x35] = 0x54, [0x37] = 0x46, [0x38] = 0xE6, [0x47] = 0x4A,
    [0x48] = 0x52, [0x49] = 0x4B, [0x4B] = 0x50, [0x4D] = 0x4F, [0x4F] = 0x4D, [0x50] = 0x51,
    [0x51] = 0x4E, [0x52] = 0x49, [0x53] = 0x4C, [0x5B] = 0xE3, [0x5C] = 0xE7, [0x5D] = 0x65};

static bool cake_win32_pending_left_shift = false;
static bool cake_win32_has_pending_left_shift = false;

void cake_win32_flush_left_shift(void) {
    if (cake_win32_has_pending_left_shift) {
        cake_win32_set_key(CAKE_KEY_LSHIFT, cake_win32_pending_left_shift);
        cake_win32_has_pending_left_shift = false;
    }

    return;
}

void cake_win32_handle_keyboard(RAWKEYBOARD* keyboard) {
    uint16_t scan_code = keyboard->MakeCode;
    if (scan_code >= 256) {
        return;
    }
    bool is_e0 = (keyboard->Flags & RI_KEY_E0) != 0;
    bool is_e1 = (keyboard->Flags & RI_KEY_E1) != 0;
    bool is_break = (keyboard->Flags & RI_KEY_BREAK) != 0;
    bool down = !is_break;

    if (is_e1 && scan_code == 0x45) {
        cake_win32_set_key(CAKE_KEY_PAUSE, down);
        return;
    }

    if (scan_code == 0x2A && !is_e0) {
        cake_win32_flush_left_shift();
        cake_win32_pending_left_shift = down;
        cake_win32_has_pending_left_shift = true;
        return;
    }

    if (is_e0) {
        if (cake_win32_has_pending_left_shift) {
            cake_win32_has_pending_left_shift = false;
            if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
                cake_win32_set_key(CAKE_KEY_LSHIFT, true);
            }
        }
    } else {
        cake_win32_flush_left_shift();
    }

    uint8_t key_code = is_e0 ? cake_win32_extended_scan_to_key[scan_code] : cake_win32_scan_to_key[scan_code];
    if (key_code) {
        cake_win32_set_key(key_code, down);
    }

    return;
}

static void cake_win32_set_mouse_button(int button, bool down) {
    if (button < 0 || button >= CAKE_MOUSE_BUTTON_COUNT) {
        return;
    }

    if (CAKE_MouseButtons[button] != down) {
        cake_latch_mouse_button(button, down);
    }
    CAKE_MouseButtons[button] = down;

    return;
}

void cake_win32_handle_mouse(RAWMOUSE* mouse) {
    USHORT flags = mouse->usButtonFlags;

    if (!(mouse->usFlags & MOUSE_MOVE_ABSOLUTE)) {
        CAKE_MouseDeltaX = cake_win32_add_motion(CAKE_MouseDeltaX, mouse->lLastX);
        CAKE_MouseDeltaY = cake_win32_add_motion(CAKE_MouseDeltaY, mouse->lLastY);
    }

    if (flags & RI_MOUSE_WHEEL) {
        CAKE_MouseWheel = cake_win32_add_motion(CAKE_MouseWheel, (int32_t)(SHORT)mouse->usButtonData);
    }

    if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
        cake_win32_set_mouse_button(CAKE_MOUSE_LEFT, true);
    }
    if (flags & RI_MOUSE_LEFT_BUTTON_UP) {
        cake_win32_set_mouse_button(CAKE_MOUSE_LEFT, false);
    }
    if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
        cake_win32_set_mouse_button(CAKE_MOUSE_RIGHT, true);
    }
    if (flags & RI_MOUSE_RIGHT_BUTTON_UP) {
        cake_win32_set_mouse_button(CAKE_MOUSE_RIGHT, false);
    }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
        cake_win32_set_mouse_button(CAKE_MOUSE_MIDDLE, true);
    }
    if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) {
        cake_win32_set_mouse_button(CAKE_MOUSE_MIDDLE, false);
    }
    if (flags & RI_MOUSE_BUTTON_4_DOWN) {
        cake_win32_set_mouse_button(CAKE_MOUSE_X1, true);
    }
    if (flags & RI_MOUSE_BUTTON_4_UP) {
        cake_win32_set_mouse_button(CAKE_MOUSE_X1, false);
    }
    if (flags & RI_MOUSE_BUTTON_5_DOWN) {
        cake_win32_set_mouse_button(CAKE_MOUSE_X2, true);
    }
    if (flags & RI_MOUSE_BUTTON_5_UP) {
        cake_win32_set_mouse_button(CAKE_MOUSE_X2, false);
    }

    return;
}

void cake_win32_clear_pending_keyboard(void) {
    cake_win32_has_pending_left_shift = false;
    cake_win32_pending_left_shift = false;
}
#endif
