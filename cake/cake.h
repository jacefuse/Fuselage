// cake.h - Controller and Keyboard Events

#ifndef CAKE_H
#define CAKE_H

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#include <xinput.h>
#endif

#include "cake_help.h"

#define CAKE_VERSION "0.2.26062901 BUTTOCKS"

// KEYBOARD Key codes are USB HID usage codes (Usage Page 0x07, Keyboard/Keypad).
#define CAKE_KEY_TABLE_SIZE 256

// Escape and function keys
#define CAKE_KEY_ESCAPE     0x29
#define CAKE_KEY_F1         0x3A
#define CAKE_KEY_F2         0x3B
#define CAKE_KEY_F3         0x3C
#define CAKE_KEY_F4         0x3D
#define CAKE_KEY_F5         0x3E
#define CAKE_KEY_F6         0x3F
#define CAKE_KEY_F7         0x40
#define CAKE_KEY_F8         0x41
#define CAKE_KEY_F9         0x42
#define CAKE_KEY_F10        0x43
#define CAKE_KEY_F11        0x44
#define CAKE_KEY_F12        0x45

// Number row
#define CAKE_KEY_GRAVE      0x35
#define CAKE_KEY_1          0x1E
#define CAKE_KEY_2          0x1F
#define CAKE_KEY_3          0x20
#define CAKE_KEY_4          0x21
#define CAKE_KEY_5          0x22
#define CAKE_KEY_6          0x23
#define CAKE_KEY_7          0x24
#define CAKE_KEY_8          0x25
#define CAKE_KEY_9          0x26
#define CAKE_KEY_0          0x27
#define CAKE_KEY_MINUS      0x2D
#define CAKE_KEY_EQUALS     0x2E
#define CAKE_KEY_BACKSPACE  0x2A

// QWERTY row
#define CAKE_KEY_TAB        0x2B
#define CAKE_KEY_Q          0x14
#define CAKE_KEY_W          0x1A
#define CAKE_KEY_E          0x08
#define CAKE_KEY_R          0x15
#define CAKE_KEY_T          0x17
#define CAKE_KEY_Y          0x1C
#define CAKE_KEY_U          0x18
#define CAKE_KEY_I          0x0C
#define CAKE_KEY_O          0x12
#define CAKE_KEY_P          0x13
#define CAKE_KEY_LBRACKET   0x2F
#define CAKE_KEY_RBRACKET   0x30
#define CAKE_KEY_BACKSLASH  0x31

// Home Row
#define CAKE_KEY_CAPSLOCK   0x39
#define CAKE_KEY_A          0x04
#define CAKE_KEY_S          0x16
#define CAKE_KEY_D          0x07
#define CAKE_KEY_F          0x09
#define CAKE_KEY_G          0x0A
#define CAKE_KEY_H          0x0B
#define CAKE_KEY_J          0x0D
#define CAKE_KEY_K          0x0E
#define CAKE_KEY_L          0x0F
#define CAKE_KEY_SEMICOLON  0x33
#define CAKE_KEY_APOSTROPHE 0x34
#define CAKE_KEY_ENTER      0x28

// Bottom Row
#define CAKE_KEY_Z          0x1D
#define CAKE_KEY_X          0x1B
#define CAKE_KEY_C          0x06
#define CAKE_KEY_V          0x19
#define CAKE_KEY_B          0x05
#define CAKE_KEY_N          0x11
#define CAKE_KEY_M          0x10
#define CAKE_KEY_COMMA      0x36
#define CAKE_KEY_PERIOD     0x37
#define CAKE_KEY_SLASH      0x38
#define CAKE_KEY_SPACE      0x2C

// Navigation Keys
#define CAKE_KEY_PRTSCR     0x46
#define CAKE_KEY_SCROLLLOCK 0x47
#define CAKE_KEY_PAUSE      0x48
#define CAKE_KEY_INSERT     0x49
#define CAKE_KEY_HOME       0x4A
#define CAKE_KEY_PGUP       0x4B
#define CAKE_KEY_DELETE     0x4C
#define CAKE_KEY_END        0x4D
#define CAKE_KEY_PGDN       0x4E
#define CAKE_KEY_RIGHT      0x4F
#define CAKE_KEY_LEFT       0x50
#define CAKE_KEY_DOWN       0x51
#define CAKE_KEY_UP         0x52

// Number Pad
#define CAKE_KEY_NUMLOCK    0x53
#define CAKE_KEY_NUMPAD_DIV 0x54
#define CAKE_KEY_NUMPAD_MUL 0x55
#define CAKE_KEY_NUMPAD_SUB 0x56
#define CAKE_KEY_NUMPAD_ADD 0x57
#define CAKE_KEY_NUMPAD_ENT 0x58
#define CAKE_KEY_NUMPAD_1   0x59
#define CAKE_KEY_NUMPAD_2   0x5A
#define CAKE_KEY_NUMPAD_3   0x5B
#define CAKE_KEY_NUMPAD_4   0x5C
#define CAKE_KEY_NUMPAD_5   0x5D
#define CAKE_KEY_NUMPAD_6   0x5E
#define CAKE_KEY_NUMPAD_7   0x5F
#define CAKE_KEY_NUMPAD_8   0x60
#define CAKE_KEY_NUMPAD_9   0x61
#define CAKE_KEY_NUMPAD_0   0x62
#define CAKE_KEY_NUMPAD_DOT 0x63

// Modifiers
#define CAKE_KEY_LCTRL      0xE0
#define CAKE_KEY_LSHIFT     0xE1
#define CAKE_KEY_LALT       0xE2
#define CAKE_KEY_LGUI       0xE3
#define CAKE_KEY_RCTRL      0xE4
#define CAKE_KEY_RSHIFT     0xE5
#define CAKE_KEY_RALT       0xE6
#define CAKE_KEY_RGUI       0xE7

// Keyboard State Table | 0 released | 1 pressed
extern uint8_t CAKE_Keys[CAKE_KEY_TABLE_SIZE];

// Mouse
#define CAKE_MOUSE_BUTTON_COUNT 5
#define CAKE_MOUSE_LEFT     0
#define CAKE_MOUSE_RIGHT    1
#define CAKE_MOUSE_MIDDLE   2
#define CAKE_MOUSE_X1       3
#define CAKE_MOUSE_X2       4

// Delta Values
extern int32_t CAKE_MouseX;
extern int32_t CAKE_MouseY;
extern int32_t CAKE_MouseW;

// Mouse State Table | 0 released | 1 pressed
extern uint8_t CAKE_MouseButtons[CAKE_MOUSE_BUTTON_COUNT];

// Controllers
#define CAKE_CONTROLLER_MAX         4
#define CAKE_CONTROLLER_PATH_MAX    256
#define CAKE_CONTROLLER_NAME_MAX    128
#define CAKE_CONTROLLER_TIMEOUT_MS  10000


// Button bitmask constants.
// Values match XINPUT_GAMEPAD_* on Windows. HID backends map onto this
// same layout so callers never need to know which backend is active.
#define CAKE_BUTTON_DPAD_UP         0x0001
#define CAKE_BUTTON_DPAD_DOWN       0x0002
#define CAKE_BUTTON_DPAD_LEFT       0x0004
#define CAKE_BUTTON_DPAD_RIGHT      0x0008
#define CAKE_BUTTON_START           0x0010
#define CAKE_BUTTON_BACK            0x0020
#define CAKE_BUTTON_LEFT_THUMB      0x0040
#define CAKE_BUTTON_RIGHT_THUMB     0x0080
#define CAKE_BUTTON_LEFT_SHOULDER   0x0100
#define CAKE_BUTTON_RIGHT_SHOULDER  0x0200
// 0x0400 and 0x0800 are reserved (XBox Home and Share?)
#define CAKE_BUTTON_A               0x1000
#define CAKE_BUTTON_B               0x2000
#define CAKE_BUTTON_X               0x4000
#define CAKE_BUTTON_Y               0x8000

// CAKE_ControllerState
// Raw snapshot of a single controller slot.
// buttons             bitmask; use CAKE_BUTTON_* constants to test bits
// left_trigger        0-255
// right_trigger       0-255
// thumb_lx/ly         -32768-32767; no deadzone applied
// thumb_rx/ry         -32768-32767; no deadzone applied
typedef struct {
    uint16_t buttons;
    uint8_t  left_trigger;
    uint8_t  right_trigger;
    int16_t  thumb_lx;
    int16_t  thumb_ly;
    int16_t  thumb_rx;
    int16_t  thumb_ry;
} CAKE_ControllerState;

// CAKE_ControllerBackend
// Which input API is servicing a given slot.
typedef enum {
    CAKE_BACKEND_UNKNOWN,
    CAKE_BACKEND_XINPUT,
    CAKE_BACKEND_HID
} CAKE_ControllerBackend;

// CAKE_ControllerConnectionState
// Lifecycle of a controller slot, updated once per CAKE_Poll(). A value persists
// across polls until the slot's state actually changes, so callers can diff
// against the previous poll to catch transitions (e.g. ATTACHED -> LOST).
// Additional codes may be reserved for future states; treat unknown values
// as informational only.
typedef enum {
    CAKE_CONN_EMPTY    = 0, // never held a controller, or cleared after timeout and not yet reused
    CAKE_CONN_ATTACHED = 1, // controller present (fresh attach, or returned before timing out)
    CAKE_CONN_LOST     = 2, // controller stopped responding; still within the timeout grace period
    CAKE_CONN_TIMEDOUT = 3, // controller exceeded the grace period without returning
} CAKE_ControllerConnectionState;

// Poll all input. First call performs full initialization.
// Motion and wheel deltas are zeroed before events are pumped so they
// always reflect movement since the previous call.
void CAKE_Poll(void);

// Tear down all input devices and free associated resources.
// Must be called once at program shutdown.
// Do not call CAKE_Poll after this.
void CAKE_Shutdown(void);

// CONTROLLER GETTERS
// All functions return a sentinel (NULL, 0, -1, CAKE_BACKEND_UNKNOWN) when
// the requested slot is empty, out of range, or not yet identified.
// Returns 1 if the slot holds a connected, identified controller, else 0. 
int CAKE_IsControllerConnected(int slot);

// Returns the connection lifecycle state for the slot. See CAKE_ControllerConnectionState.
CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot);

// Returns a pointer to the current input state for the slot, or NULL.
// The pointer is valid until the next CAKE_Poll call; do not cache it.
const CAKE_ControllerState *CAKE_GetControllerState(int slot);

// Returns the human-readable device name string, or NULL. 
const char *CAKE_GetControllerName(int slot);

// Returns the active backend for the slot, or CAKE_BACKEND_UNKNOWN.
CAKE_ControllerBackend CAKE_GetControllerBackend(int slot);

// Returns the USB vendor ID for the slot, or 0 if unavailable.
uint16_t CAKE_GetControllerVendorID(int slot);

// Returns the USB product ID for the slot, or 0 if unavailable.
uint16_t CAKE_GetControllerProductID(int slot);

// Returns the XInput player index (0-3) for the slot, or -1 if not XInput.
int CAKE_GetControllerXInputIndex(int slot);

#endif // CAKE_H
