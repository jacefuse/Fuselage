#ifndef FUSELAGE_INTERNAL_H
#define FUSELAGE_INTERNAL_H
#include "../../fuselage.h"
#include "../fuselage_sync.h"
#include <stdatomic.h>
typedef enum {
    FUSELAGE_COLD,
    FUSELAGE_STARTING,
    FUSELAGE_RUNNING,
    FUSELAGE_STOPPING,
    FUSELAGE_STOPPED
} FuselagePhase;
enum { FUSELAGE_SIM_TIMER = 0, FUSELAGE_WAIT_TIMER = 1, FUSELAGE_CAP_TIMER = 2 };
typedef struct {
    _Atomic FuselagePhase phase;
    FuselageMutex state_lock;
    FuselageEvent frame_event, cap_event;
    bool lock_ready, frame_event_ready, cap_event_ready;
    bool graphics_owned, input_owned, audio_owned, dice_owned, pacing_owned;
    bool save_finalized, startup_complete;
    bool target_timer_active;
    _Atomic(FuselageTickCallback) tick_callback;
    double simulation_rate;
    _Atomic double target_fps;
    _Atomic bool input_requires_focus;
    _Atomic bool focus_policy_changed;
    bool previously_focused, banner;
    _Atomic bool signal_quit, signal_windowed, signal_borderless, signal_exclusive, signal_cap;
    int exit_code;
    uint64_t previous_submit_us;
} FuselageRootState;
extern FuselageRootState fuselage_root;
void fuselage_prepare_process(void);
bool fuselage_root_init(void);
bool fuselage_root_shutdown(void);
void fuselage_process_signals(void);
bool fuselage_create_owned_timer(uint8_t handle, double rate, DICE_TimerCallback callback);
bool fuselage_release_owned_timer(uint8_t handle);
void fuselage_simulation_tick(uint8_t handle, void* context);
void fuselage_frame_tick(uint8_t handle, void* context);
void fuselage_cap_tick(uint8_t handle, void* context);
void fuselage_print_banner(void);
#endif
