// Published input and transition history. SEE: (CAKE.md, Backend state publication)
#include "cake_state_private.h"
#include <string.h>

bool CAKE_Keys[CAKE_KEY_TABLE_SIZE];
bool CAKE_KeysPressed[CAKE_KEY_TABLE_SIZE];
bool CAKE_KeysReleased[CAKE_KEY_TABLE_SIZE];
bool CAKE_MouseButtons[CAKE_MOUSE_BUTTON_COUNT];
bool CAKE_MouseButtonsPressed[CAKE_MOUSE_BUTTON_COUNT];
bool CAKE_MouseButtonsReleased[CAKE_MOUSE_BUTTON_COUNT];
int32_t CAKE_MouseDeltaX;
int32_t CAKE_MouseDeltaY;
int32_t CAKE_MouseWheel;
uint16_t CAKE_PadButtonsPressed[CAKE_CONTROLLER_MAX];
uint16_t CAKE_PadButtonsReleased[CAKE_CONTROLLER_MAX];
uint64_t CAKE_PollCount;

static bool cake_key_press_latch[CAKE_KEY_TABLE_SIZE];
static bool cake_key_release_latch[CAKE_KEY_TABLE_SIZE];
static bool cake_mouse_press_latch[CAKE_MOUSE_BUTTON_COUNT];
static bool cake_mouse_release_latch[CAKE_MOUSE_BUTTON_COUNT];
static uint16_t cake_previous_controller_buttons[CAKE_CONTROLLER_MAX];

void cake_latch_key(uint8_t key_code, bool down) {
    if (down) {
        cake_key_press_latch[key_code] = true;
    } else {
        cake_key_release_latch[key_code] = true;
    }
}

void cake_latch_mouse_button(int button, bool down) {
    if (button < 0 || button >= CAKE_MOUSE_BUTTON_COUNT) {
        return;
    }
    if (down) {
        cake_mouse_press_latch[button] = true;
    } else {
        cake_mouse_release_latch[button] = true;
    }
}

void cake_publish_input_edges(void) {
    memcpy(CAKE_KeysPressed, cake_key_press_latch, sizeof(CAKE_KeysPressed));
    memcpy(CAKE_KeysReleased, cake_key_release_latch, sizeof(CAKE_KeysReleased));
    memset(cake_key_press_latch, 0, sizeof(cake_key_press_latch));
    memset(cake_key_release_latch, 0, sizeof(cake_key_release_latch));
    memcpy(CAKE_MouseButtonsPressed, cake_mouse_press_latch, sizeof(CAKE_MouseButtonsPressed));
    memcpy(CAKE_MouseButtonsReleased, cake_mouse_release_latch, sizeof(CAKE_MouseButtonsReleased));
    memset(cake_mouse_press_latch, 0, sizeof(cake_mouse_press_latch));
    memset(cake_mouse_release_latch, 0, sizeof(cake_mouse_release_latch));
}

void cake_derive_controller_edges(void) {
    for (int slot = 0; slot < CAKE_CONTROLLER_MAX; ++slot) {
        const CAKE_ControllerState* controller_state = CAKE_GetControllerState(slot);
        uint16_t current_buttons = controller_state ? controller_state->buttons : 0;
        CAKE_PadButtonsPressed[slot] = (uint16_t)(current_buttons & ~cake_previous_controller_buttons[slot]);
        CAKE_PadButtonsReleased[slot] = (uint16_t)(~current_buttons & cake_previous_controller_buttons[slot]);
        cake_previous_controller_buttons[slot] = current_buttons;
    }
}

void cake_clear_keyboard_mouse_state(void) {
    memset(CAKE_Keys, 0, sizeof(CAKE_Keys));
    memset(cake_key_press_latch, 0, sizeof(cake_key_press_latch));
    memset(cake_key_release_latch, 0, sizeof(cake_key_release_latch));
    memset(CAKE_KeysPressed, 0, sizeof(CAKE_KeysPressed));
    memset(CAKE_KeysReleased, 0, sizeof(CAKE_KeysReleased));
    memset(CAKE_MouseButtons, 0, sizeof(CAKE_MouseButtons));
    memset(cake_mouse_press_latch, 0, sizeof(cake_mouse_press_latch));
    memset(cake_mouse_release_latch, 0, sizeof(cake_mouse_release_latch));
    memset(CAKE_MouseButtonsPressed, 0, sizeof(CAKE_MouseButtonsPressed));
    memset(CAKE_MouseButtonsReleased, 0, sizeof(CAKE_MouseButtonsReleased));
    CAKE_MouseDeltaX = 0;
    CAKE_MouseDeltaY = 0;
    CAKE_MouseWheel = 0;
}
