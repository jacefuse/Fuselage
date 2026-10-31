#ifndef CAKE_LINUX_PRIVATE_H
#define CAKE_LINUX_PRIVATE_H
#include "../../core/cake_state_private.h"
#if defined(__linux__)
#include <stddef.h>
void cake_linux_intake_key(uint32_t key_code, bool down);
void cake_linux_intake_button(int button, bool down);
void cake_linux_intake_motion(double delta_x, double delta_y);
void cake_linux_intake_scroll(double wheel_delta);
void cake_linux_resync_keys(const uint32_t* keys, size_t count);
void cake_linux_release_buttons(void);
void cake_linux_publish_input(void);
bool cake_linux_wayland_attach(void* display_connection);
void cake_linux_wayland_drain(void);
void cake_linux_wayland_detach(void);
bool cake_linux_x11_attach(void* display_connection);
void cake_linux_x11_drain(void);
void cake_linux_x11_detach(void);
// Implemented by the evdev controller files; no display connection is needed.
void cake_linux_initialize_controllers(void);
void cake_linux_scan_controllers(void);
void cake_linux_poll_controllers(void);
void cake_linux_shutdown_controllers(void);
#endif
#endif
