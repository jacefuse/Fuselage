/* One render/event iteration. SEE: (FUSELAGE.md, Rendering and timing) */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "fuselage_internal.h"
#include "../pacing/fuselage_pacing.h"
#include "../../GDMF/vulkan/gdmf_vulkan.h"
#include <time.h>
#include <errno.h>

static int fuselage_wait_cap_event(void) {
#if defined(_WIN32)
    DWORD result = WaitForSingleObject(fuselage_root.cap_event, 10);
    return result == WAIT_OBJECT_0 ? 1 : result == WAIT_TIMEOUT ? 0 : -1;
#else
    FuselageEvent* event = &fuselage_root.cap_event;
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline)) {
        return -1;
    }
    deadline.tv_nsec += 10000000;
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }
    if (pthread_mutex_lock(&event->mutex)) {
        return -1;
    }
    int result = event->signaled ? 0 : pthread_cond_timedwait(&event->condition, &event->mutex, &deadline);
    int signaled = event->signaled;
    if (signaled) {
        event->signaled = false;
    }
    pthread_mutex_unlock(&event->mutex);
    return result && result != ETIMEDOUT ? -1 : signaled;
#endif
}
static bool fuselage_stop_after_graphics(void) {
    if (gdmf_vulkan_should_stop()) {
        fuselage_root.exit_code = FUSELAGE_EXIT_FAILED;
    }
    atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
    return !fuselage_root_shutdown();
}
bool fuselage(void) {
    FuselagePhase phase = atomic_load(&fuselage_root.phase);
    if (phase == FUSELAGE_STOPPED) {
        return false;
    }
    if (phase == FUSELAGE_COLD) {
        (void)fuselage_root_init();
    }
    if (atomic_load(&fuselage_root.phase) == FUSELAGE_STOPPING) {
        return !fuselage_root_shutdown();
    }
    fuselage_process_signals();
    if (atomic_load(&fuselage_root.phase) == FUSELAGE_STOPPING) {
        return !fuselage_root_shutdown();
    }
    if (!GDMF_Tick()) {
        return fuselage_stop_after_graphics();
    }
    while (fuselage_root.target_timer_active) {
        int result = fuselage_wait_cap_event();
        if (result > 0) {
            break;
        }
        if (result < 0) {
            fuselage_root.exit_code = FUSELAGE_EXIT_FAILED;
            atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
        }
        fuselage_process_signals();
        if (atomic_load(&fuselage_root.phase) == FUSELAGE_STOPPING) {
            return !fuselage_root_shutdown();
        }
        if (!GDMF_Tick()) {
            return fuselage_stop_after_graphics();
        }
    }
    (void)GDMF_AcquireFrame();
    uint64_t requested_us = DICE_GetElapsedMicroseconds();
    fuselage_mutex_lock(&fuselage_root.state_lock);
    uint64_t held_us = DICE_GetElapsedMicroseconds();
    if (GDMF_PrepareFrame()) {
        fuselage_pacing_frame(requested_us, held_us, DICE_GetElapsedMicroseconds(),
                              fuselage_root.previous_submit_us);
    }
    fuselage_mutex_unlock(&fuselage_root.state_lock);
    uint64_t submit_us = DICE_GetElapsedMicroseconds();
    GDMF_SubmitFrame();
    fuselage_root.previous_submit_us = DICE_GetElapsedMicroseconds() - submit_us;
    return true;
}
