#ifndef CAKE_WIN32_PRIVATE_H
#define CAKE_WIN32_PRIVATE_H
#include "../../core/cake_state_private.h"
#if defined(_WIN32)
#include <windows.h>

void cake_win32_handle_keyboard(RAWKEYBOARD* keyboard);
void cake_win32_handle_mouse(RAWMOUSE* mouse);
void cake_win32_flush_left_shift(void);
void cake_win32_clear_pending_keyboard(void);
void cake_win32_dispatch_raw_input(HRAWINPUT input, bool silenced);

extern const GUID cake_win32_hid_guid;

// Controller services are implemented in the separately reviewed controller files.
// Initialize resets device/cache state, configures tracing, loads XInput and scans.
// Shutdown releases device resources and clears device/cache state.
void cake_win32_initialize_controllers(void);
void cake_win32_shutdown_controllers(void);
void cake_win32_scan_controllers(void);
void cake_win32_refresh_controller_discovery(void);
void cake_win32_poll_controllers(void);
// Report storage is borrowed for this call only; never retain its address.
void cake_win32_handle_controller_input(HANDLE device, const RAWHID* report);
#endif
#endif
