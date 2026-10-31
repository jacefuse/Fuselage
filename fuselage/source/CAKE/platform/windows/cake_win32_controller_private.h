#ifndef CAKE_WIN32_CONTROLLER_PRIVATE_H
#define CAKE_WIN32_CONTROLLER_PRIVATE_H
#include "cake_win32_private.h"
#if defined(_WIN32)
#include <xinput.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

// Poll-thread controller state; externally coordinate all queries and teardown.
// Descriptor storage belongs to binding; raw handles and returned states are borrowed.
// SEE: (CAKE.md, Windows controllers)
#define CAKE_WIN32_MAX_NODES 128

typedef struct {
    int xinput_index;
    uint16_t vendor_id;
    uint16_t product_id;
    CAKE_ControllerBackend backend;
    XINPUT_CAPABILITIES capabilities;
    XINPUT_BATTERY_INFORMATION battery;
} cake_win32_controller_identity;

enum {
    CAKE_WIN32_HID_AXIS_X = 0,
    CAKE_WIN32_HID_AXIS_Y,
    CAKE_WIN32_HID_AXIS_Z,
    CAKE_WIN32_HID_AXIS_RX,
    CAKE_WIN32_HID_AXIS_RY,
    CAKE_WIN32_HID_AXIS_RZ,
    CAKE_WIN32_HID_AXIS_HAT,
    CAKE_WIN32_HID_AXIS_BRAKE,
    CAKE_WIN32_HID_AXIS_ACCELERATOR,
    CAKE_WIN32_HID_AXIS_COUNT
};

typedef struct {
    bool present;
    LONG logical_minimum;
    LONG logical_maximum;
    USHORT bit_size;
} cake_win32_hid_axis;

typedef struct {
    const char* name;
    uint16_t button[17];
    uint8_t left_trigger_button;
    uint8_t right_trigger_button;
} cake_win32_hid_layout;

typedef struct {
    HANDLE raw;
    PHIDP_PREPARSED_DATA preparsed;
    cake_win32_hid_axis axis[CAKE_WIN32_HID_AXIS_COUNT];
    int right_stick_x;
    int right_stick_y;
    int left_trigger_axis;
    int right_trigger_axis;
    const cake_win32_hid_layout* layout;
    uint32_t raw_buttons;
    int32_t raw_axes[CAKE_WIN32_HID_AXIS_COUNT];
    bool switch_centered;
    int32_t switch_center[4];
    CAKE_ControllerState live;
} cake_win32_hid_state;

typedef struct {
    char device_path[CAKE_CONTROLLER_PATH_MAX];
    char name[CAKE_CONTROLLER_NAME_MAX];
    int chain_index;
    uint64_t attached_at;
    uint64_t last_seen_at;
    uint64_t lost_at;
    bool identity_valid;
    cake_win32_controller_identity identity;
    cake_win32_hid_state hid;
    CAKE_ControllerState state[2];
    volatile int front_buffer;

} cake_win32_controller;

typedef struct {
    USAGE page;
    USAGE usage;
} cake_win32_hid_usage;
extern const cake_win32_hid_usage cake_win32_hid_axis_usage[CAKE_WIN32_HID_AXIS_COUNT];
extern const char* const cake_win32_hid_axis_name[CAKE_WIN32_HID_AXIS_COUNT];
#define CAKE_WIN32_HID_DPAD_MASK                                                                             \
    (CAKE_BUTTON_DPAD_UP | CAKE_BUTTON_DPAD_DOWN | CAKE_BUTTON_DPAD_LEFT | CAKE_BUTTON_DPAD_RIGHT)
extern cake_win32_controller cake_win32_controllers[CAKE_CONTROLLER_MAX];
extern CAKE_ControllerConnectionState cake_win32_connection_states[CAKE_CONTROLLER_MAX];
extern bool cake_win32_hid_trace;
bool cake_win32_is_xinput_path(const char* path);
void cake_win32_identify_controller(cake_win32_controller* controller);
void cake_win32_xinput_load(void);
void cake_win32_poll_xinput(cake_win32_controller* controller);
void cake_win32_hid_ignored_clear(void);
void cake_win32_hid_unbind(cake_win32_controller* controller);
void cake_win32_hid_bind_lookup(cake_win32_controller* controller);
cake_win32_controller* cake_win32_hid_slot_for(HANDLE device);
const cake_win32_hid_layout* cake_win32_hid_layout_for(uint16_t vendor_id, uint16_t product_id);
void cake_win32_reset_discovery(void);
#endif
#endif
