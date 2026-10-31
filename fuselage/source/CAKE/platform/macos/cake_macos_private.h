#ifndef CAKE_MACOS_PRIVATE_H
#define CAKE_MACOS_PRIVATE_H
#include "../../core/cake_state_private.h"
#if defined(__APPLE__)
#include <pthread.h>

// Serializes pending input, controller callbacks and framework snapshot lifetime.
extern pthread_mutex_t cake_macos_intake_lock;
void cake_macos_intake_key(unsigned short key_code, bool down);
void cake_macos_intake_flags(unsigned long device_flags, bool caps_lock);
void cake_macos_intake_button(int button, bool down);
void cake_macos_intake_motion(double delta_x, double delta_y);
void cake_macos_intake_scroll(double scroll_delta_y, bool precise, double line_delta_y);
void cake_macos_publish_input(void);

bool cake_macos_is_main_thread(void);
bool cake_macos_attach_input(void* native_window);
void cake_macos_detach_input(void);
void cake_macos_start_controllers(void);
void cake_macos_stop_controllers(void);
bool cake_macos_read_controller(void* controller_object, CAKE_ControllerState* output);

// Only accepted slots are retained by the framework bridge; rejected pads can retry.
bool cake_macos_controller_connected(void* controller_object, const char* name);
void cake_macos_controller_disconnected(void* controller_object);
void cake_macos_enable_controllers(void);
void cake_macos_clear_controllers(void);
void cake_macos_poll_controllers(void);
int16_t cake_macos_convert_axis(float value);
uint8_t cake_macos_convert_trigger(float value);
#endif
#endif
