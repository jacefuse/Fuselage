#ifndef CAKE_LINUX_CONTROLLER_PRIVATE_H
#define CAKE_LINUX_CONTROLLER_PRIVATE_H
#include "cake_linux_private.h"
#if defined(__linux__)
#include <linux/input.h>
#define CAKE_LINUX_AXIS_COUNT 8
typedef struct {
    char path[CAKE_CONTROLLER_PATH_MAX];
    char name[CAKE_CONTROLLER_NAME_MAX];
    int descriptor;
    bool present;
    uint64_t lost_at;
    uint16_t vendor_id;
    uint16_t product_id;
    struct input_absinfo axis_info[CAKE_LINUX_AXIS_COUNT];
    bool has_axis[CAKE_LINUX_AXIS_COUNT];
    bool has_rumble;
    int effect_id;
    CAKE_ControllerState live;
    CAKE_ControllerState state[2];
    volatile int front_buffer; // Buffer selection does not synchronize concurrent readers.

} cake_linux_controller;

extern const int cake_linux_axes[CAKE_LINUX_AXIS_COUNT];
extern cake_linux_controller cake_linux_controllers[CAKE_CONTROLLER_MAX];
extern CAKE_ControllerConnectionState cake_linux_connection_states[CAKE_CONTROLLER_MAX];
extern bool cake_linux_slot_freed;
bool cake_linux_test_bit(const unsigned long* bits, int bit_index);
bool cake_linux_adopt_controller(int descriptor, const char* path, bool writable);
void cake_linux_apply_controller_event(cake_linux_controller* controller, const struct input_event* event);
void cake_linux_initialize_discovery(void);
void cake_linux_shutdown_discovery(void);
void cake_linux_refresh_discovery(uint64_t now);
#endif
#endif
