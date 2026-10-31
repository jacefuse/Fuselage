#pragma once
#include "misl_mmio_map.h"

// Input and calendar snapshots are published once per host pump.
// Key values are USB HID usage IDs; pad masks match CAKE's public button values.
// SEE: (MISL_ABI_MMIO.md, Input and clocks)
#define MISL_NKEYS 256

#define MISL_KEY_ESCAPE 0x29
#define MISL_KEY_F1 0x3A
#define MISL_KEY_F2 0x3B
#define MISL_KEY_F3 0x3C
#define MISL_KEY_F4 0x3D
#define MISL_KEY_F5 0x3E
#define MISL_KEY_F6 0x3F
#define MISL_KEY_F7 0x40
#define MISL_KEY_F8 0x41
#define MISL_KEY_F9 0x42
#define MISL_KEY_F10 0x43
#define MISL_KEY_F11 0x44
#define MISL_KEY_F12 0x45
#define MISL_KEY_GRAVE 0x35
#define MISL_KEY_1 0x1E
#define MISL_KEY_2 0x1F
#define MISL_KEY_3 0x20
#define MISL_KEY_4 0x21
#define MISL_KEY_5 0x22
#define MISL_KEY_6 0x23
#define MISL_KEY_7 0x24
#define MISL_KEY_8 0x25
#define MISL_KEY_9 0x26
#define MISL_KEY_0 0x27
#define MISL_KEY_MINUS 0x2D
#define MISL_KEY_EQUALS 0x2E
#define MISL_KEY_BACKSPACE 0x2A
#define MISL_KEY_TAB 0x2B
#define MISL_KEY_Q 0x14
#define MISL_KEY_W 0x1A
#define MISL_KEY_E 0x08
#define MISL_KEY_R 0x15
#define MISL_KEY_T 0x17
#define MISL_KEY_Y 0x1C
#define MISL_KEY_U 0x18
#define MISL_KEY_I 0x0C
#define MISL_KEY_O 0x12
#define MISL_KEY_P 0x13
#define MISL_KEY_LBRACKET 0x2F
#define MISL_KEY_RBRACKET 0x30
#define MISL_KEY_BACKSLASH 0x31
#define MISL_KEY_CAPSLOCK 0x39
#define MISL_KEY_A 0x04
#define MISL_KEY_S 0x16
#define MISL_KEY_D 0x07
#define MISL_KEY_F 0x09
#define MISL_KEY_G 0x0A
#define MISL_KEY_H 0x0B
#define MISL_KEY_J 0x0D
#define MISL_KEY_K 0x0E
#define MISL_KEY_L 0x0F
#define MISL_KEY_SEMICOLON 0x33
#define MISL_KEY_APOSTROPHE 0x34
#define MISL_KEY_ENTER 0x28
#define MISL_KEY_Z 0x1D
#define MISL_KEY_X 0x1B
#define MISL_KEY_C 0x06
#define MISL_KEY_V 0x19
#define MISL_KEY_B 0x05
#define MISL_KEY_N 0x11
#define MISL_KEY_M 0x10
#define MISL_KEY_COMMA 0x36
#define MISL_KEY_PERIOD 0x37
#define MISL_KEY_SLASH 0x38
#define MISL_KEY_SPACE 0x2C
#define MISL_KEY_PRTSCR 0x46
#define MISL_KEY_SCROLLLOCK 0x47
#define MISL_KEY_PAUSE 0x48
#define MISL_KEY_INSERT 0x49
#define MISL_KEY_HOME 0x4A
#define MISL_KEY_PGUP 0x4B
#define MISL_KEY_DELETE 0x4C
#define MISL_KEY_END 0x4D
#define MISL_KEY_PGDN 0x4E
#define MISL_KEY_RIGHT 0x4F
#define MISL_KEY_LEFT 0x50
#define MISL_KEY_DOWN 0x51
#define MISL_KEY_UP 0x52
#define MISL_KEY_NUMLOCK 0x53
#define MISL_KEY_NUMPAD_DIV 0x54
#define MISL_KEY_NUMPAD_MUL 0x55
#define MISL_KEY_NUMPAD_SUB 0x56
#define MISL_KEY_NUMPAD_ADD 0x57
#define MISL_KEY_NUMPAD_ENT 0x58
#define MISL_KEY_NUMPAD_1 0x59
#define MISL_KEY_NUMPAD_2 0x5A
#define MISL_KEY_NUMPAD_3 0x5B
#define MISL_KEY_NUMPAD_4 0x5C
#define MISL_KEY_NUMPAD_5 0x5D
#define MISL_KEY_NUMPAD_6 0x5E
#define MISL_KEY_NUMPAD_7 0x5F
#define MISL_KEY_NUMPAD_8 0x60
#define MISL_KEY_NUMPAD_9 0x61
#define MISL_KEY_NUMPAD_0 0x62
#define MISL_KEY_NUMPAD_DOT 0x63
#define MISL_KEY_LCTRL 0xE0
#define MISL_KEY_LSHIFT 0xE1
#define MISL_KEY_LALT 0xE2
#define MISL_KEY_LGUI 0xE3
#define MISL_KEY_RCTRL 0xE4
#define MISL_KEY_RSHIFT 0xE5
#define MISL_KEY_RALT 0xE6
#define MISL_KEY_RGUI 0xE7
#define MISL_NMOUSEBTN 8
#define MISL_NPADS 4

typedef struct {
    uint8_t keys[MISL_NKEYS]; // 0/1 per USB HID scancode
    int32_t mouseX, mouseY, mouseWheel;
    uint8_t mouseButtons[MISL_NMOUSEBTN];
    struct {
        uint8_t connected;
        uint8_t lt, rt; // triggers 0..255
        uint8_t _pad;
        uint16_t buttons;       // MISL_PAD_* bitmask
        int16_t lx, ly, rx, ry; // thumbsticks -32768..32767
    } pads[MISL_NPADS];

    uint8_t keysPressed[MISL_NKEYS];  // 1 if it went down since the last pump
    uint8_t keysReleased[MISL_NKEYS]; // 1 if it went up since the last pump

    uint8_t mouseButtonsPressed[MISL_NMOUSEBTN];
    uint8_t mouseButtonsReleased[MISL_NMOUSEBTN];

    uint16_t padButtonsPressed[MISL_NPADS]; // MISL_PAD_* mask
    uint16_t padButtonsReleased[MISL_NPADS];

    uint64_t elapsedMicroseconds;

    int32_t canvasMouseX, canvasMouseY;
} MislInput;

#define MISL_PAD_DPAD_UP 0x0001
#define MISL_PAD_DPAD_DOWN 0x0002
#define MISL_PAD_DPAD_LEFT 0x0004
#define MISL_PAD_DPAD_RIGHT 0x0008
#define MISL_PAD_START 0x0010
#define MISL_PAD_BACK 0x0020
#define MISL_PAD_LTHUMB 0x0040
#define MISL_PAD_RTHUMB 0x0080
#define MISL_PAD_LSHOULDER 0x0100
#define MISL_PAD_RSHOULDER 0x0200
#define MISL_PAD_A 0x1000
#define MISL_PAD_B 0x2000
#define MISL_PAD_X 0x4000
#define MISL_PAD_Y 0x8000

typedef struct {
    uint64_t utcSeconds;      // seconds since 1970-01-01 00:00:00 UTC
    uint32_t milliseconds;    // 0..999, the fraction of utcSeconds elapsed
    int32_t utcOffsetMinutes; // local = UTC + this; includes DST when isDst
    uint32_t isDst;           // 1 while daylight saving is in effect locally
    uint32_t year;            // local civil year, e.g. 2026
    uint32_t month;           // 1..12
    uint32_t day;             // 1..31
    uint32_t weekday;         // 0 = Sunday .. 6 = Saturday
    uint32_t hour;            // 0..23
    uint32_t minute;          // 0..59
    uint32_t second;          // 0..59
    uint32_t yearDay;         // 1..366
    uint32_t _pad;            // always 0; keeps sizeof at 56 on every ABI
} MislClock;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(MislInput) <= MISL_MMIO_STRIDE, "Input exceeds its region");
_Static_assert(sizeof(MislClock) == 56u, "Calendar layout changed");
_Static_assert(sizeof(MislClock) <= MISL_CLOCK_BYTES, "Calendar exceeds its page");
#endif
