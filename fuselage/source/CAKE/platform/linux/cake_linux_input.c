// Pending input. SEE: (CAKE.md, Linux input)
#include "cake_linux_private.h"
#if defined(__linux__)
#include <string.h>
#include <pthread.h>
#include <linux/input.h>
#include <math.h>
#include <float.h>
static pthread_mutex_t cake_linux_intake_lock = PTHREAD_MUTEX_INITIALIZER;
static bool cake_linux_pending_keys[CAKE_KEY_TABLE_SIZE];
static bool cake_linux_pending_buttons[CAKE_MOUSE_BUTTON_COUNT];
static double cake_linux_pending_x;
static double cake_linux_pending_y;
static double cake_linux_pending_wheel;

static bool cake_linux_silenced;
static void cake_linux_set_pending_key(uint8_t key_usage, bool down) {
    if (cake_linux_pending_keys[key_usage] != down) {
        cake_latch_key(key_usage, down);
    }
    cake_linux_pending_keys[key_usage] = down;
}
static void cake_linux_accumulate(double* pending, double delta) {
    if (!isfinite(delta)) {
        return;
    }
    double sum = *pending + delta;
    *pending = isfinite(sum) ? sum : (delta > 0 ? DBL_MAX : -DBL_MAX);
}

static int32_t cake_linux_drain_motion(double* pending) {
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
static const uint8_t cake_linux_key_to_usage[256] = {
    [KEY_ESC] = 0x29,       [KEY_1] = 0x1E,          [KEY_2] = 0x1F,          [KEY_3] = 0x20,
    [KEY_4] = 0x21,         [KEY_5] = 0x22,          [KEY_6] = 0x23,          [KEY_7] = 0x24,
    [KEY_8] = 0x25,         [KEY_9] = 0x26,          [KEY_0] = 0x27,          [KEY_MINUS] = 0x2D,
    [KEY_EQUAL] = 0x2E,     [KEY_BACKSPACE] = 0x2A,  [KEY_TAB] = 0x2B,        [KEY_Q] = 0x14,
    [KEY_W] = 0x1A,         [KEY_E] = 0x08,          [KEY_R] = 0x15,          [KEY_T] = 0x17,
    [KEY_Y] = 0x1C,         [KEY_U] = 0x18,          [KEY_I] = 0x0C,          [KEY_O] = 0x12,
    [KEY_P] = 0x13,         [KEY_LEFTBRACE] = 0x2F,  [KEY_RIGHTBRACE] = 0x30, [KEY_ENTER] = 0x28,
    [KEY_LEFTCTRL] = 0xE0,  [KEY_A] = 0x04,          [KEY_S] = 0x16,          [KEY_D] = 0x07,
    [KEY_F] = 0x09,         [KEY_G] = 0x0A,          [KEY_H] = 0x0B,          [KEY_J] = 0x0D,
    [KEY_K] = 0x0E,         [KEY_L] = 0x0F,          [KEY_SEMICOLON] = 0x33,  [KEY_APOSTROPHE] = 0x34,
    [KEY_GRAVE] = 0x35,     [KEY_LEFTSHIFT] = 0xE1,  [KEY_BACKSLASH] = 0x31,  [KEY_Z] = 0x1D,
    [KEY_X] = 0x1B,         [KEY_C] = 0x06,          [KEY_V] = 0x19,          [KEY_B] = 0x05,
    [KEY_N] = 0x11,         [KEY_M] = 0x10,          [KEY_COMMA] = 0x36,      [KEY_DOT] = 0x37,
    [KEY_SLASH] = 0x38,     [KEY_RIGHTSHIFT] = 0xE5, [KEY_KPASTERISK] = 0x55, [KEY_LEFTALT] = 0xE2,
    [KEY_SPACE] = 0x2C,     [KEY_CAPSLOCK] = 0x39,   [KEY_F1] = 0x3A,         [KEY_F2] = 0x3B,
    [KEY_F3] = 0x3C,        [KEY_F4] = 0x3D,         [KEY_F5] = 0x3E,         [KEY_F6] = 0x3F,
    [KEY_F7] = 0x40,        [KEY_F8] = 0x41,         [KEY_F9] = 0x42,         [KEY_F10] = 0x43,
    [KEY_NUMLOCK] = 0x53,   [KEY_SCROLLLOCK] = 0x47, [KEY_KP7] = 0x5F,        [KEY_KP8] = 0x60,
    [KEY_KP9] = 0x61,       [KEY_KPMINUS] = 0x56,    [KEY_KP4] = 0x5C,        [KEY_KP5] = 0x5D,
    [KEY_KP6] = 0x5E,       [KEY_KPPLUS] = 0x57,     [KEY_KP1] = 0x59,        [KEY_KP2] = 0x5A,
    [KEY_KP3] = 0x5B,       [KEY_KP0] = 0x62,        [KEY_KPDOT] = 0x63,      [KEY_102ND] = 0x64,
    [KEY_F11] = 0x44,       [KEY_F12] = 0x45,        [KEY_KPENTER] = 0x58,    [KEY_RIGHTCTRL] = 0xE4,
    [KEY_KPSLASH] = 0x54,   [KEY_SYSRQ] = 0x46,      [KEY_RIGHTALT] = 0xE6,   [KEY_HOME] = 0x4A,
    [KEY_UP] = 0x52,        [KEY_PAGEUP] = 0x4B,     [KEY_LEFT] = 0x50,       [KEY_RIGHT] = 0x4F,
    [KEY_END] = 0x4D,       [KEY_DOWN] = 0x51,       [KEY_PAGEDOWN] = 0x4E,   [KEY_INSERT] = 0x49,
    [KEY_DELETE] = 0x4C,    [KEY_KPEQUAL] = 0x67,    [KEY_PAUSE] = 0x48,      [KEY_LEFTMETA] = 0xE3,
    [KEY_RIGHTMETA] = 0xE7, [KEY_COMPOSE] = 0x65};

void cake_linux_intake_key(uint32_t key_code, bool down) {
    if (key_code >= 256) {
        return;
    }

    uint8_t key_usage = cake_linux_key_to_usage[key_code];
    if (!key_usage) {
        return;
    }

    pthread_mutex_lock(&cake_linux_intake_lock);
    if (cake_linux_silenced) {
        pthread_mutex_unlock(&cake_linux_intake_lock);
        return;
    }
    if (cake_linux_pending_keys[key_usage] != down) {
        cake_latch_key(key_usage, down);
    }
    cake_linux_pending_keys[key_usage] = down;
    pthread_mutex_unlock(&cake_linux_intake_lock);

    return;
}

void cake_linux_resync_keys(const uint32_t* keys, size_t count) {
    bool held[CAKE_KEY_TABLE_SIZE] = {0};
    if (keys) {
        for (size_t key_index = 0; key_index < count; ++key_index) {
            uint32_t key_code = keys[key_index];
            if (key_code < 256 && cake_linux_key_to_usage[key_code]) {
                held[cake_linux_key_to_usage[key_code]] = true;
            }
        }
    }
    pthread_mutex_lock(&cake_linux_intake_lock);
    if (!cake_linux_silenced) {
        for (int key_usage = 0; key_usage < CAKE_KEY_TABLE_SIZE; ++key_usage) {
            cake_linux_set_pending_key((uint8_t)key_usage, held[key_usage]);
        }
    }
    pthread_mutex_unlock(&cake_linux_intake_lock);
}

void cake_linux_intake_button(int button, bool down) {

    if (button < 0 || button >= CAKE_MOUSE_BUTTON_COUNT) {
        return;
    }

    pthread_mutex_lock(&cake_linux_intake_lock);
    if (cake_linux_silenced) {
        pthread_mutex_unlock(&cake_linux_intake_lock);
        return;
    }
    if (cake_linux_pending_buttons[button] != down) {
        cake_latch_mouse_button(button, down);
    }
    cake_linux_pending_buttons[button] = down;
    pthread_mutex_unlock(&cake_linux_intake_lock);

    return;
}

void cake_linux_intake_motion(double delta_x, double delta_y) {

    pthread_mutex_lock(&cake_linux_intake_lock);
    if (cake_linux_silenced) {
        pthread_mutex_unlock(&cake_linux_intake_lock);
        return;
    }
    cake_linux_accumulate(&cake_linux_pending_x, delta_x);
    cake_linux_accumulate(&cake_linux_pending_y, delta_y);
    pthread_mutex_unlock(&cake_linux_intake_lock);

    return;
}

void cake_linux_intake_scroll(double wheel_delta) {
    pthread_mutex_lock(&cake_linux_intake_lock);
    if (!cake_linux_silenced) {
        cake_linux_accumulate(&cake_linux_pending_wheel, wheel_delta);
    }
    pthread_mutex_unlock(&cake_linux_intake_lock);
}
void cake_linux_release_buttons(void) {
    pthread_mutex_lock(&cake_linux_intake_lock);
    for (int button = 0; button < CAKE_MOUSE_BUTTON_COUNT; ++button) {
        if (cake_linux_pending_buttons[button]) {
            cake_latch_mouse_button(button, false);
        }
        cake_linux_pending_buttons[button] = false;
    }
    pthread_mutex_unlock(&cake_linux_intake_lock);
}

void cake_linux_publish_input(void) {
    pthread_mutex_lock(&cake_linux_intake_lock);
    memcpy(CAKE_Keys, cake_linux_pending_keys, sizeof(CAKE_Keys));
    cake_publish_input_edges();
    memcpy(CAKE_MouseButtons, cake_linux_pending_buttons, sizeof(CAKE_MouseButtons));
    CAKE_MouseDeltaX = cake_linux_drain_motion(&cake_linux_pending_x);
    CAKE_MouseDeltaY = cake_linux_drain_motion(&cake_linux_pending_y);
    CAKE_MouseWheel = cake_linux_drain_motion(&cake_linux_pending_wheel);
    pthread_mutex_unlock(&cake_linux_intake_lock);
}
void CAKE_Silence(void) {
    pthread_mutex_lock(&cake_linux_intake_lock);
    cake_linux_silenced = true;
    pthread_mutex_unlock(&cake_linux_intake_lock);
}
void CAKE_Resume(void) {
    pthread_mutex_lock(&cake_linux_intake_lock);
    cake_linux_silenced = false;
    memset(cake_linux_pending_keys, 0, sizeof(cake_linux_pending_keys));
    memset(cake_linux_pending_buttons, 0, sizeof(cake_linux_pending_buttons));
    cake_linux_pending_x = cake_linux_pending_y = cake_linux_pending_wheel = 0;
    cake_clear_keyboard_mouse_state();
    pthread_mutex_unlock(&cake_linux_intake_lock);
}
bool CAKE_IsSilenced(void) {
    pthread_mutex_lock(&cake_linux_intake_lock);
    bool silenced = cake_linux_silenced;
    pthread_mutex_unlock(&cake_linux_intake_lock);
    return silenced;
}

#endif
