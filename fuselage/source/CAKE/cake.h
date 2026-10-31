// Raw keyboard, mouse and controller input. SEE: (CAKE.md, Overview)
#ifndef CAKE_H
#define CAKE_H

#include <stdint.h>
#include <stdbool.h>

#define CAKE_VERSION "0.5.2026102801 ENTERON"

// Keyboard constants use USB HID keyboard/keypad usage codes.
#define CAKE_KEY_TABLE_SIZE 256

// Escape and function keys
#define CAKE_KEY_ESCAPE 0x29
#define CAKE_KEY_F1 0x3A
#define CAKE_KEY_F2 0x3B
#define CAKE_KEY_F3 0x3C
#define CAKE_KEY_F4 0x3D
#define CAKE_KEY_F5 0x3E
#define CAKE_KEY_F6 0x3F
#define CAKE_KEY_F7 0x40
#define CAKE_KEY_F8 0x41
#define CAKE_KEY_F9 0x42
#define CAKE_KEY_F10 0x43
#define CAKE_KEY_F11 0x44
#define CAKE_KEY_F12 0x45

// Number row
#define CAKE_KEY_GRAVE 0x35
#define CAKE_KEY_1 0x1E
#define CAKE_KEY_2 0x1F
#define CAKE_KEY_3 0x20
#define CAKE_KEY_4 0x21
#define CAKE_KEY_5 0x22
#define CAKE_KEY_6 0x23
#define CAKE_KEY_7 0x24
#define CAKE_KEY_8 0x25
#define CAKE_KEY_9 0x26
#define CAKE_KEY_0 0x27
#define CAKE_KEY_MINUS 0x2D
#define CAKE_KEY_EQUALS 0x2E
#define CAKE_KEY_BACKSPACE 0x2A

// QWERTY row
#define CAKE_KEY_TAB 0x2B
#define CAKE_KEY_Q 0x14
#define CAKE_KEY_W 0x1A
#define CAKE_KEY_E 0x08
#define CAKE_KEY_R 0x15
#define CAKE_KEY_T 0x17
#define CAKE_KEY_Y 0x1C
#define CAKE_KEY_U 0x18
#define CAKE_KEY_I 0x0C
#define CAKE_KEY_O 0x12
#define CAKE_KEY_P 0x13
#define CAKE_KEY_LBRACKET 0x2F
#define CAKE_KEY_RBRACKET 0x30
#define CAKE_KEY_BACKSLASH 0x31

// Home Row
#define CAKE_KEY_CAPSLOCK 0x39
#define CAKE_KEY_A 0x04
#define CAKE_KEY_S 0x16
#define CAKE_KEY_D 0x07
#define CAKE_KEY_F 0x09
#define CAKE_KEY_G 0x0A
#define CAKE_KEY_H 0x0B
#define CAKE_KEY_J 0x0D
#define CAKE_KEY_K 0x0E
#define CAKE_KEY_L 0x0F
#define CAKE_KEY_SEMICOLON 0x33
#define CAKE_KEY_APOSTROPHE 0x34
#define CAKE_KEY_ENTER 0x28

// Bottom Row
#define CAKE_KEY_Z 0x1D
#define CAKE_KEY_X 0x1B
#define CAKE_KEY_C 0x06
#define CAKE_KEY_V 0x19
#define CAKE_KEY_B 0x05
#define CAKE_KEY_N 0x11
#define CAKE_KEY_M 0x10
#define CAKE_KEY_COMMA 0x36
#define CAKE_KEY_PERIOD 0x37
#define CAKE_KEY_SLASH 0x38
#define CAKE_KEY_SPACE 0x2C

// Navigation Keys
#define CAKE_KEY_PRTSCR 0x46
#define CAKE_KEY_SCROLLLOCK 0x47
#define CAKE_KEY_PAUSE 0x48
#define CAKE_KEY_INSERT 0x49
#define CAKE_KEY_HOME 0x4A
#define CAKE_KEY_PGUP 0x4B
#define CAKE_KEY_DELETE 0x4C
#define CAKE_KEY_END 0x4D
#define CAKE_KEY_PGDN 0x4E
#define CAKE_KEY_RIGHT 0x4F
#define CAKE_KEY_LEFT 0x50
#define CAKE_KEY_DOWN 0x51
#define CAKE_KEY_UP 0x52

// Number Pad
#define CAKE_KEY_NUMLOCK 0x53
#define CAKE_KEY_NUMPAD_DIV 0x54
#define CAKE_KEY_NUMPAD_MUL 0x55
#define CAKE_KEY_NUMPAD_SUB 0x56
#define CAKE_KEY_NUMPAD_ADD 0x57
#define CAKE_KEY_NUMPAD_ENT 0x58
#define CAKE_KEY_NUMPAD_1 0x59
#define CAKE_KEY_NUMPAD_2 0x5A
#define CAKE_KEY_NUMPAD_3 0x5B
#define CAKE_KEY_NUMPAD_4 0x5C
#define CAKE_KEY_NUMPAD_5 0x5D
#define CAKE_KEY_NUMPAD_6 0x5E
#define CAKE_KEY_NUMPAD_7 0x5F
#define CAKE_KEY_NUMPAD_8 0x60
#define CAKE_KEY_NUMPAD_9 0x61
#define CAKE_KEY_NUMPAD_0 0x62
#define CAKE_KEY_NUMPAD_DOT 0x63

// Modifiers
#define CAKE_KEY_LCTRL 0xE0
#define CAKE_KEY_LSHIFT 0xE1
#define CAKE_KEY_LALT 0xE2
#define CAKE_KEY_LGUI 0xE3
#define CAKE_KEY_RCTRL 0xE4
#define CAKE_KEY_RSHIFT 0xE5
#define CAKE_KEY_RALT 0xE6
#define CAKE_KEY_RGUI 0xE7

// Published state and per-poll edges. Treat these arrays as read-only.
// SEE: (CAKE.md, Published input)
extern bool CAKE_Keys[CAKE_KEY_TABLE_SIZE];

extern bool CAKE_KeysPressed[CAKE_KEY_TABLE_SIZE];
extern bool CAKE_KeysReleased[CAKE_KEY_TABLE_SIZE];

// Increments at the start of each poll; reading does not consume input.
extern uint64_t CAKE_PollCount;

// Relative motion, wheel values and button state. SEE: (CAKE.md, Mouse input)
#define CAKE_MOUSE_BUTTON_COUNT 5
#define CAKE_MOUSE_LEFT 0
#define CAKE_MOUSE_RIGHT 1
#define CAKE_MOUSE_MIDDLE 2
#define CAKE_MOUSE_X1 3
#define CAKE_MOUSE_X2 4

extern int32_t CAKE_MouseDeltaX;
extern int32_t CAKE_MouseDeltaY;
extern int32_t CAKE_MouseWheel;

extern bool CAKE_MouseButtons[CAKE_MOUSE_BUTTON_COUNT];

extern bool CAKE_MouseButtonsPressed[CAKE_MOUSE_BUTTON_COUNT];
extern bool CAKE_MouseButtonsReleased[CAKE_MOUSE_BUTTON_COUNT];

// Controller slots and XInput-compatible button masks.
// SEE: (CAKE.md, Controllers)
#define CAKE_CONTROLLER_MAX 4
#define CAKE_CONTROLLER_PATH_MAX 256
#define CAKE_CONTROLLER_NAME_MAX 128
#define CAKE_CONTROLLER_TIMEOUT_MS 10000

#define CAKE_BUTTON_DPAD_UP 0x0001
#define CAKE_BUTTON_DPAD_DOWN 0x0002
#define CAKE_BUTTON_DPAD_LEFT 0x0004
#define CAKE_BUTTON_DPAD_RIGHT 0x0008
#define CAKE_BUTTON_START 0x0010
#define CAKE_BUTTON_BACK 0x0020
#define CAKE_BUTTON_LEFT_THUMB 0x0040
#define CAKE_BUTTON_RIGHT_THUMB 0x0080
#define CAKE_BUTTON_LEFT_SHOULDER 0x0100
#define CAKE_BUTTON_RIGHT_SHOULDER 0x0200
#define CAKE_BUTTON_A 0x1000
#define CAKE_BUTTON_B 0x2000
#define CAKE_BUTTON_X 0x4000
#define CAKE_BUTTON_Y 0x8000

// Field names, order and widths are part of the existing controller interface.
typedef struct {
    uint16_t buttons;
    uint8_t leftTrigger;
    uint8_t rightTrigger;
    int16_t thumbLeftX;
    int16_t thumbLeftY;
    int16_t thumbRightX;
    int16_t thumbRightY;
} CAKE_ControllerState;

extern uint16_t CAKE_PadButtonsPressed[CAKE_CONTROLLER_MAX];
extern uint16_t CAKE_PadButtonsReleased[CAKE_CONTROLLER_MAX];

typedef enum {
    CAKE_AXIS_LEFT_X = 0,
    CAKE_AXIS_LEFT_Y = 1,
    CAKE_AXIS_RIGHT_X = 2,
    CAKE_AXIS_RIGHT_Y = 3,
} CAKE_ControllerAxis;

typedef enum {
    CAKE_TRIGGER_LEFT = 0,
    CAKE_TRIGGER_RIGHT = 1,
} CAKE_ControllerTrigger;

typedef enum { CAKE_BACKEND_UNKNOWN, CAKE_BACKEND_XINPUT, CAKE_BACKEND_HID } CAKE_ControllerBackend;

typedef enum {
    CAKE_CONN_EMPTY = 0,
    CAKE_CONN_ATTACHED = 1,
    CAKE_CONN_LOST = 2,
    CAKE_CONN_TIMEDOUT = 3,
} CAKE_ControllerConnectionState;

// Opaque platform window descriptor. SEE: (CAKE.md, Window attachment)
void CAKE_AttachWindow(void* native_window);

// Refresh state and publish transitions. First call initializes the backend.
void CAKE_Poll(void);

// Stop input before destroying the attached window. SEE: (CAKE.md, Lifecycle)
void CAKE_Shutdown(void);

// Silence keyboard/mouse only; resume clears their published state and edges.
void CAKE_Silence(void);
void CAKE_Resume(void);
bool CAKE_IsSilenced(void);

// Getter fallbacks and platform differences: SEE: (CAKE.md, Controller queries)
bool CAKE_IsControllerConnected(int slot);

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot);

const CAKE_ControllerState* CAKE_GetControllerState(int slot);

const char* CAKE_GetControllerName(int slot);

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot);

uint16_t CAKE_GetControllerVendorID(int slot);

uint16_t CAKE_GetControllerProductID(int slot);

int CAKE_GetControllerXInputIndex(int slot);

// Motor speeds are 0..65535; false means unsupported, unavailable or failed.
bool CAKE_SetControllerVibration(int slot, uint16_t left_motor, uint16_t right_motor);

#endif
