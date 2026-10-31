// Pending keyboard/mouse input. SEE: (CAKE.md, macOS input and controllers)
#include "cake_macos_private.h"
#if defined(__APPLE__)
#include <string.h>
#include <math.h>
#include <float.h>
pthread_mutex_t cake_macos_intake_lock = PTHREAD_MUTEX_INITIALIZER;
static bool cake_macos_pending_keys[CAKE_KEY_TABLE_SIZE];
static bool cake_macos_pending_buttons[CAKE_MOUSE_BUTTON_COUNT];
static double cake_macos_pending_x;
static double cake_macos_pending_y;
static double cake_macos_pending_wheel;

static bool cake_macos_silenced;
static void cake_macos_set_pending_key(uint8_t key_usage, bool down) {
    if (cake_macos_pending_keys[key_usage] != down) {
        cake_latch_key(key_usage, down);
    }
    cake_macos_pending_keys[key_usage] = down;
}
static void cake_macos_accumulate(double* pending, double delta) {
    if (!isfinite(delta)) {
        return;
    }
    double sum = *pending + delta;
    *pending = isfinite(sum) ? sum : (delta > 0 ? DBL_MAX : -DBL_MAX);
}
static double cake_macos_scale_scroll(double value, double scale) {
    if (!isfinite(value)) {
        return 0;
    }
    if (value > DBL_MAX / scale) {
        return DBL_MAX;
    }
    if (value < -DBL_MAX / scale) {
        return -DBL_MAX;
    }
    double scaled = value * scale;
    return isfinite(scaled) ? scaled : (value > 0 ? DBL_MAX : -DBL_MAX);
}
static int32_t cake_macos_drain_motion(double* pending) {
    int32_t result;
    if (*pending > INT32_MAX) {
        result = INT32_MAX;
        *pending = 0;
    } else if (*pending < INT32_MIN) {
        result = INT32_MIN;
        *pending = 0;
    } else {
        result = (int32_t)*pending;
        *pending -= result;
    }
    return result;
}
static const uint8_t cake_macos_key_to_usage[128] = {
    [0x00] = 0x04, [0x01] = 0x16, [0x02] = 0x07, [0x03] = 0x09, [0x04] = 0x0B, [0x05] = 0x0A, [0x06] = 0x1D,
    [0x07] = 0x1B, [0x08] = 0x06, [0x09] = 0x19, [0x0A] = 0x64, [0x0B] = 0x05, [0x0C] = 0x14, [0x0D] = 0x1A,
    [0x0E] = 0x08, [0x0F] = 0x15, [0x10] = 0x1C, [0x11] = 0x17, [0x12] = 0x1E, [0x13] = 0x1F, [0x14] = 0x20,
    [0x15] = 0x21, [0x16] = 0x23, [0x17] = 0x22, [0x18] = 0x2E, [0x19] = 0x26, [0x1A] = 0x24, [0x1B] = 0x2D,
    [0x1C] = 0x25, [0x1D] = 0x27, [0x1E] = 0x30, [0x1F] = 0x12, [0x20] = 0x18, [0x21] = 0x2F, [0x22] = 0x0C,
    [0x23] = 0x13, [0x24] = 0x28, [0x25] = 0x0F, [0x26] = 0x0D, [0x27] = 0x34, [0x28] = 0x0E, [0x29] = 0x33,
    [0x2A] = 0x31, [0x2B] = 0x36, [0x2C] = 0x38, [0x2D] = 0x11, [0x2E] = 0x10, [0x2F] = 0x37, [0x30] = 0x2B,
    [0x31] = 0x2C, [0x32] = 0x35, [0x33] = 0x2A, [0x34] = 0x58, [0x35] = 0x29, [0x40] = 0x6C, [0x41] = 0x63,
    [0x43] = 0x55, [0x45] = 0x57, [0x47] = 0x53, [0x4B] = 0x54, [0x4C] = 0x58, [0x4E] = 0x56, [0x51] = 0x67,
    [0x52] = 0x62, [0x53] = 0x59, [0x54] = 0x5A, [0x55] = 0x5B, [0x56] = 0x5C, [0x57] = 0x5D, [0x58] = 0x5E,
    [0x59] = 0x5F, [0x5B] = 0x60, [0x5C] = 0x61, [0x60] = 0x3E, [0x61] = 0x3F, [0x62] = 0x40, [0x63] = 0x3C,
    [0x64] = 0x41, [0x65] = 0x42, [0x67] = 0x44, [0x69] = 0x46, [0x6A] = 0x6B, [0x6B] = 0x47, [0x6D] = 0x43,
    [0x6F] = 0x45, [0x71] = 0x48, [0x72] = 0x49, [0x73] = 0x4A, [0x74] = 0x4B, [0x75] = 0x4C, [0x76] = 0x3D,
    [0x77] = 0x4D, [0x78] = 0x3B, [0x79] = 0x4E, [0x7A] = 0x3A, [0x7B] = 0x50, [0x7C] = 0x4F, [0x7D] = 0x51,
    [0x7E] = 0x52};

void cake_macos_intake_key(unsigned short key_code, bool down) {
    if (key_code >= 128) {
        return;
    }

    uint8_t key_usage = cake_macos_key_to_usage[key_code];
    if (!key_usage) {
        return;
    }

    pthread_mutex_lock(&cake_macos_intake_lock);
    if (cake_macos_silenced) {
        pthread_mutex_unlock(&cake_macos_intake_lock);
        return;
    }
    if (cake_macos_pending_keys[key_usage] != down) {
        cake_latch_key(key_usage, down);
    }
    cake_macos_pending_keys[key_usage] = down;
    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

void cake_macos_intake_flags(unsigned long device_flags, bool caps_lock) {

    pthread_mutex_lock(&cake_macos_intake_lock);
    if (cake_macos_silenced) {
        pthread_mutex_unlock(&cake_macos_intake_lock);
        return;
    }
    cake_macos_set_pending_key(0xE0, (device_flags & 0x0001) != 0);
    cake_macos_set_pending_key(0xE1, (device_flags & 0x0002) != 0);
    cake_macos_set_pending_key(0xE5, (device_flags & 0x0004) != 0);
    cake_macos_set_pending_key(0xE3, (device_flags & 0x0008) != 0);
    cake_macos_set_pending_key(0xE7, (device_flags & 0x0010) != 0);
    cake_macos_set_pending_key(0xE2, (device_flags & 0x0020) != 0);
    cake_macos_set_pending_key(0xE6, (device_flags & 0x0040) != 0);
    cake_macos_set_pending_key(0xE4, (device_flags & 0x2000) != 0);
    cake_macos_set_pending_key(0x39, caps_lock);
    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

void cake_macos_intake_button(int button, bool down) {

    if (button < 0 || button >= CAKE_MOUSE_BUTTON_COUNT) {
        return;
    }

    pthread_mutex_lock(&cake_macos_intake_lock);
    if (cake_macos_silenced) {
        pthread_mutex_unlock(&cake_macos_intake_lock);
        return;
    }
    if (cake_macos_pending_buttons[button] != down) {
        cake_latch_mouse_button(button, down);
    }
    cake_macos_pending_buttons[button] = down;
    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

void cake_macos_intake_motion(double delta_x, double delta_y) {

    pthread_mutex_lock(&cake_macos_intake_lock);
    if (cake_macos_silenced) {
        pthread_mutex_unlock(&cake_macos_intake_lock);
        return;
    }
    cake_macos_accumulate(&cake_macos_pending_x, delta_x);
    cake_macos_accumulate(&cake_macos_pending_y, delta_y);
    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

void cake_macos_intake_scroll(double scroll_delta_y, bool precise, double line_delta_y) {

    pthread_mutex_lock(&cake_macos_intake_lock);
    if (cake_macos_silenced) {
        pthread_mutex_unlock(&cake_macos_intake_lock);
        return;
    }
    cake_macos_accumulate(
        &cake_macos_pending_wheel,
        cake_macos_scale_scroll(precise ? scroll_delta_y : line_delta_y, precise ? 12.0 : 120.0));
    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

void cake_macos_publish_input(void) {
    pthread_mutex_lock(&cake_macos_intake_lock);
    memcpy(CAKE_Keys, cake_macos_pending_keys, sizeof(CAKE_Keys));
    cake_publish_input_edges();
    memcpy(CAKE_MouseButtons, cake_macos_pending_buttons, sizeof(CAKE_MouseButtons));
    CAKE_MouseDeltaX = cake_macos_drain_motion(&cake_macos_pending_x);
    CAKE_MouseDeltaY = cake_macos_drain_motion(&cake_macos_pending_y);
    CAKE_MouseWheel = cake_macos_drain_motion(&cake_macos_pending_wheel);
    pthread_mutex_unlock(&cake_macos_intake_lock);
}
void CAKE_Silence(void) {
    pthread_mutex_lock(&cake_macos_intake_lock);
    cake_macos_silenced = true;
    pthread_mutex_unlock(&cake_macos_intake_lock);
}
void CAKE_Resume(void) {
    pthread_mutex_lock(&cake_macos_intake_lock);
    cake_macos_silenced = false;
    memset(cake_macos_pending_keys, 0, sizeof(cake_macos_pending_keys));
    memset(cake_macos_pending_buttons, 0, sizeof(cake_macos_pending_buttons));
    cake_macos_pending_x = cake_macos_pending_y = cake_macos_pending_wheel = 0;
    cake_clear_keyboard_mouse_state();
    pthread_mutex_unlock(&cake_macos_intake_lock);
}
bool CAKE_IsSilenced(void) {
    pthread_mutex_lock(&cake_macos_intake_lock);
    bool silenced = cake_macos_silenced;
    pthread_mutex_unlock(&cake_macos_intake_lock);
    return silenced;
}

#endif
