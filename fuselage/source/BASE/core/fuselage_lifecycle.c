/* Commit startup only after resources/timers are ready. SEE: (FUSELAGE.md, Lifecycle) */
#include "fuselage_internal.h"
#include "../pacing/fuselage_pacing.h"
#include "../../MISL/host/runtime/misl_host_state_internal.h"
#include "../../MISL/host/runtime/misl_host_save_internal.h"
#include "../../MISL/host/runtime/misl_host_scheduler_internal.h"
#include "../../DICE/core/dice_lifecycle_private.h"
#include "../../SRAM/sram.h"
#include "../../GDMF/vulkan/gdmf_vulkan.h"
#include <stdio.h>

static bool fuselage_startup_failure(const char* reason) {
    fprintf(stderr, "[Fuselage] %s\n", reason);
    fuselage_root.exit_code = FUSELAGE_EXIT_FAILED;
    atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
    return false;
}
bool fuselage_root_init(void) {
    atomic_store(&fuselage_root.phase, FUSELAGE_STARTING);
    fuselage_prepare_process();
    fuselage_root.graphics_owned = true;
    if (GDMF_Init() != 0) {
        return fuselage_startup_failure("Graphics initialization failed.");
    }
    CAKE_AttachWindow(GDMF_GetNativeWindowHandle());
    fuselage_root.input_owned = true;
    InitColorsDeepPalettes();
    if (InitSprites() <= 0) {
        return fuselage_startup_failure("Sprite storage initialization failed.");
    }
    if (!fuselage_mutex_init_checked(&fuselage_root.state_lock)) {
        return fuselage_startup_failure("State mutex creation failed.");
    }
    fuselage_root.lock_ready = true;
    /* No sound device is permitted; the facade still owns any initialized state. */
    fuselage_root.audio_owned = true;
    (void)SHARP_Init();
    if (!DICE_Init()) {
        return fuselage_startup_failure("Clock initialization failed.");
    }
    fuselage_root.dice_owned = true;
    if (!fuselage_event_init_checked(&fuselage_root.frame_event)) {
        return fuselage_startup_failure("Frame event creation failed.");
    }
    fuselage_root.frame_event_ready = true;
    if (!fuselage_event_init_checked(&fuselage_root.cap_event)) {
        return fuselage_startup_failure("Render-cap event creation failed.");
    }
    fuselage_root.cap_event_ready = true;
    for (uint8_t handle = 0; handle < 3; handle++) {
        if (!DICE_LockTimer(handle)) {
            return fuselage_startup_failure("An engine timer handle is already owned.");
        }
    }
    fuselage_pacing_init(FUSELAGE_SIM_TIMER, FUSELAGE_WAIT_TIMER, fuselage_root.simulation_rate);
    fuselage_root.pacing_owned = true;
    if (!fuselage_create_owned_timer(FUSELAGE_SIM_TIMER, fuselage_root.simulation_rate,
                                     fuselage_simulation_tick) ||
        !fuselage_create_owned_timer(FUSELAGE_WAIT_TIMER, 60, fuselage_frame_tick)) {
        return fuselage_startup_failure("Engine timer creation failed.");
    }
    double fps = atomic_load(&fuselage_root.target_fps);
    if (fps > 0) {
        if (!fuselage_create_owned_timer(FUSELAGE_CAP_TIMER, fps, fuselage_cap_tick)) {
            return fuselage_startup_failure("Render-cap timer creation failed.");
        }
        fuselage_root.target_timer_active = true;
    }
    if (!DICE_StartTimer(FUSELAGE_WAIT_TIMER) ||
        (fuselage_root.target_timer_active && !DICE_StartTimer(FUSELAGE_CAP_TIMER)) ||
        !DICE_StartTimer(FUSELAGE_SIM_TIMER)) {
        return fuselage_startup_failure("Engine timer start failed.");
    }
    if (fuselage_root.banner) {
        fuselage_print_banner();
    }
    fuselage_root.startup_complete = true;
    atomic_store(&fuselage_root.phase, FUSELAGE_RUNNING);
    return true;
}
bool fuselage_root_shutdown(void) {
    atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
    if (fuselage_root.frame_event_ready) {
        fuselage_event_set(&fuselage_root.frame_event);
    }
    if (fuselage_root.cap_event_ready) {
        fuselage_event_set(&fuselage_root.cap_event);
    }
    if (fuselage_root.dice_owned) {
        if (!dice_shutdown_checked()) {
            fprintf(stderr, "[Fuselage] Timer owners retained; shutdown will retry.\n");
            return false;
        }
        fuselage_root.dice_owned = false;
    }
    if (fuselage_root.pacing_owned) {
        fuselage_pacing_shutdown();
        fuselage_root.pacing_owned = false;
    }
    if (!fuselage_root.save_finalized) {
        bool clean = fuselage_root.startup_complete && fuselage_root.exit_code == FUSELAGE_EXIT_OK;
        bool saved;
        if (fuselage_guest_session.image) {
            saved = fuselage_guest_finish_save(clean);
        } else {
            if (!clean) {
                SRAM_Abandon();
            }
            saved = SRAM_Save();
        }
        if (!saved) {
            fprintf(stderr, "[Fuselage] Save publication failed: %s\n", SRAM_GetFailureReason());
            fuselage_root.exit_code = FUSELAGE_EXIT_SAVE_FAILED;
        }
        fuselage_guest_release_memory();
        (void)fuselage_guest_close_save();
        fuselage_root.save_finalized = true;
    }
    if (fuselage_root.audio_owned) {
        fuselage_guest_reset_devices();
        SHARP_Shutdown();
        fuselage_root.audio_owned = false;
    }
    if (fuselage_root.input_owned) {
        CAKE_Shutdown();
        fuselage_root.input_owned = false;
    }
    if (fuselage_root.graphics_owned) {
        /* GDMF quiesces and releases its layer owners before destroying the device. */
        GDMF_Shutdown();
        if (gdmf_vulkan_has_resources()) {
            fprintf(stderr, "[Fuselage] Renderer owners retained; shutdown will retry.\n");
            return false;
        }
        fuselage_root.graphics_owned = false;
    }
    if (fuselage_root.frame_event_ready) {
        fuselage_event_destroy(&fuselage_root.frame_event);
        fuselage_root.frame_event_ready = false;
    }
    if (fuselage_root.cap_event_ready) {
        fuselage_event_destroy(&fuselage_root.cap_event);
        fuselage_root.cap_event_ready = false;
    }
    fuselage_root.target_timer_active = false;
    if (fuselage_root.lock_ready) {
        fuselage_mutex_destroy(&fuselage_root.state_lock);
        fuselage_root.lock_ready = false;
    }
    atomic_store(&fuselage_root.phase, FUSELAGE_STOPPED);
    return true;
}
