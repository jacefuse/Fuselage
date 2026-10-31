// Platform timer workers. SEE: (DICE.md, Timers and callbacks)
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "dice_timers_private.h"
#include "dice_timers_internal.h"
#include <limits.h>
#include <math.h>
#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    bool initialized;
    bool joined;
    bool condition_ready;
    pthread_mutex_t lock;
    pthread_cond_t condition;
    pthread_t thread;
    dice_timer_state state;
    int64_t period_nanoseconds;
    int64_t next_fire;
    int64_t remaining;
    DICE_TimerCallback callback;
    void* user_data;
    _Atomic uint32_t pending_intervals;
    _Atomic int64_t last_delta;
} dice_timer;

static dice_timer dice_timers[DICE_MAX_TIMERS];

bool dice_timer_valid(uint8_t handle) {
    return handle < DICE_MAX_TIMERS && dice_timers[handle].initialized;
}

static int64_t dice_timer_now(void) {
    struct timespec sample;
    if (clock_gettime(CLOCK_MONOTONIC, &sample) != 0 || sample.tv_sec < 0 || sample.tv_nsec < 0 ||
        sample.tv_nsec >= 1000000000 || (uint64_t)sample.tv_sec > (uint64_t)INT64_MAX / 1000000000) {
        return -1;
    }
    int64_t result;
    return dice_timer_add((int64_t)sample.tv_sec * 1000000000, sample.tv_nsec, &result) ? result : -1;
}

static void* dice_timer_worker(void* parameter) {
    dice_timer_worker_enter();
    uint8_t handle = (uint8_t)(uintptr_t)parameter;
    dice_timer* timer = &dice_timers[handle];
    pthread_mutex_lock(&timer->lock);
    for (;;) {
        if (timer->state == DICE_TSTATE_RELEASING) {
            break;
        }
        if (timer->state != DICE_TSTATE_RUNNING) {
            if (pthread_cond_wait(&timer->condition, &timer->lock) != 0) {
                timer->state = DICE_TSTATE_RELEASING;
                break;
            }
            continue;
        }
        int64_t sample = dice_timer_now();
        if (sample < 0) {
            timer->state = DICE_TSTATE_RELEASING;
            break;
        }
        if (sample < timer->next_fire) {
#if defined(__APPLE__)
            int64_t remaining = timer->next_fire - sample;
            struct timespec timeout = {(time_t)(remaining / 1000000000), (long)(remaining % 1000000000)};
            int result = pthread_cond_timedwait_relative_np(&timer->condition, &timer->lock, &timeout);
#else
            struct timespec timeout = {(time_t)(timer->next_fire / 1000000000),
                                       (long)(timer->next_fire % 1000000000)};
            int result = pthread_cond_timedwait(&timer->condition, &timer->lock, &timeout);
#endif
            if (result && result != ETIMEDOUT) {
                timer->state = DICE_TSTATE_RELEASING;
                break;
            }
            continue;
        }
        if (!dice_timer_add(timer->next_fire, timer->period_nanoseconds, &timer->next_fire)) {
            timer->state = DICE_TSTATE_RELEASING;
            break;
        }
        atomic_fetch_add(&timer->pending_intervals, 1);
        DICE_TimerCallback callback = timer->callback;
        void* user_data = timer->user_data;
        pthread_mutex_unlock(&timer->lock);
        if (callback) {
            callback(handle, user_data);
        }
        pthread_mutex_lock(&timer->lock);
        if (timer->state == DICE_TSTATE_RUNNING) {
            int64_t after = dice_timer_now();
            if (after < 0) {
                timer->state = DICE_TSTATE_RELEASING;
                break;
            }
            if (after >= timer->next_fire) {
                uint64_t behind = (uint64_t)after - (uint64_t)timer->next_fire;
                if (behind >= (uint64_t)timer->period_nanoseconds) {
                    uint64_t missed = behind / (uint64_t)timer->period_nanoseconds;
                    atomic_fetch_add(&timer->pending_intervals, (uint32_t)missed);
                    timer->next_fire = after;
                }
            }
        }
    }
    pthread_mutex_unlock(&timer->lock);
    dice_timer_worker_leave();
    return NULL;
}

static bool dice_timer_init(uint8_t handle, int64_t period_nanoseconds, bool autostart,
                            DICE_TimerCallback callback, void* user_data) {
    if (dice_timer_is_worker_thread() || handle >= DICE_MAX_TIMERS || period_nanoseconds <= 0 ||
        dice_timer_claimed[handle]) {
        return false;
    }
    dice_timer* timer = &dice_timers[handle];
    if (timer->initialized && !DICE_ReleaseTimer(handle)) {
        return false;
    }
    memset(timer, 0, sizeof(*timer));
    if (pthread_mutex_init(&timer->lock, NULL) != 0) {
        return false;
    }
    timer->initialized = timer->joined = true;
    timer->state = DICE_TSTATE_RELEASING;
#if defined(__APPLE__)
    if (pthread_cond_init(&timer->condition, NULL) != 0) {
        goto fail;
    }
    timer->condition_ready = true;
#else
    pthread_condattr_t attributes;
    if (pthread_condattr_init(&attributes) != 0) {
        goto fail;
    }
    int clock_result = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    int condition_result = clock_result ? clock_result : pthread_cond_init(&timer->condition, &attributes);
    if (!condition_result) {
        timer->condition_ready = true;
    }
    int attribute_result = pthread_condattr_destroy(&attributes);
    if (condition_result || attribute_result) {
        goto fail;
    }
#endif
    atomic_init(&timer->pending_intervals, 0);
    atomic_init(&timer->last_delta, 0);
    timer->period_nanoseconds = period_nanoseconds;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->state = DICE_TSTATE_STOPPED;
    if (pthread_create(&timer->thread, NULL, dice_timer_worker, (void*)(uintptr_t)handle) != 0) {
        goto fail;
    }
    timer->joined = false;
    if (autostart && !DICE_StartTimer(handle)) {
        goto fail;
    }
    return true;
fail:
    (void)DICE_ReleaseTimer(handle);
    return false;
}

bool DICE_InitTimer(uint8_t handle, double hz, bool autostart, DICE_TimerCallback callback, void* user_data) {
    if (!isfinite(hz) || hz <= 0) {
        return false;
    }
    int64_t period;
    if (!dice_timer_integer(1000000000.0 / hz, &period) || period <= 0) {
        return false;
    }
    return dice_timer_init(handle, period, autostart, callback, user_data);
}

bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                            DICE_TimerCallback callback, void* user_data) {
    if (!microseconds || microseconds > (uint64_t)INT64_MAX / 1000) {
        return false;
    }
    return dice_timer_init(handle, (int64_t)microseconds * 1000, autostart, callback, user_data);
}

bool DICE_ReleaseTimer(uint8_t handle) {
    if (dice_timer_is_worker_thread() || !dice_timer_valid(handle) || dice_timer_claimed[handle]) {
        return false;
    }
    dice_timer* timer = &dice_timers[handle];
    if (!timer->joined) {
        pthread_mutex_lock(&timer->lock);
        timer->state = DICE_TSTATE_RELEASING;
        int signal_result = pthread_cond_signal(&timer->condition);
        pthread_mutex_unlock(&timer->lock);
        if (signal_result || pthread_join(timer->thread, NULL) != 0) {
            return false;
        }
        timer->joined = true;
    }
    timer->state = DICE_TSTATE_RELEASING;
    if (timer->condition_ready) {
        if (pthread_cond_destroy(&timer->condition) != 0) {
            return false;
        }
        timer->condition_ready = false;
    }
    if (pthread_mutex_destroy(&timer->lock) != 0) {
        return false;
    }
    memset(timer, 0, sizeof(*timer));
    return true;
}

static bool dice_timer_signal_locked(dice_timer* timer) {
    if (pthread_cond_signal(&timer->condition) == 0) {
        return true;
    }
    timer->state = DICE_TSTATE_RELEASING;
    return false;
}
bool DICE_StartTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    pthread_mutex_lock(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    int64_t sample = dice_timer_now(), target;
    if (sample < 0 || !dice_timer_add(sample, timer->period_nanoseconds, &target)) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    atomic_store(&timer->pending_intervals, 0);
    timer->next_fire = target;
    timer->state = DICE_TSTATE_RUNNING;
    bool signaled = dice_timer_signal_locked(timer);
    pthread_mutex_unlock(&timer->lock);
    return signaled;
}

bool DICE_StopTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    pthread_mutex_lock(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    timer->state = DICE_TSTATE_STOPPED;
    atomic_store(&timer->pending_intervals, 0);
    bool signaled = dice_timer_signal_locked(timer);
    pthread_mutex_unlock(&timer->lock);
    return signaled;
}

bool DICE_PauseTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    pthread_mutex_lock(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    if (timer->state == DICE_TSTATE_RUNNING) {
        int64_t sample = dice_timer_now();
        if (sample < 0) {
            pthread_mutex_unlock(&timer->lock);
            return false;
        }
        timer->remaining = timer->next_fire > sample ? timer->next_fire - sample : 0;
        if (timer->remaining < 0) {
            timer->remaining = 0;
        }
        timer->state = DICE_TSTATE_PAUSED;
    }
    bool signaled = dice_timer_signal_locked(timer);
    pthread_mutex_unlock(&timer->lock);
    return signaled;
}

bool DICE_ResumeTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    pthread_mutex_lock(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    if (timer->state == DICE_TSTATE_PAUSED) {
        int64_t sample = dice_timer_now(), target;
        if (sample < 0 || !dice_timer_add(sample, timer->remaining, &target)) {
            pthread_mutex_unlock(&timer->lock);
            return false;
        }
        timer->next_fire = target;
        timer->state = DICE_TSTATE_RUNNING;
    }
    bool signaled = dice_timer_signal_locked(timer);
    pthread_mutex_unlock(&timer->lock);
    return signaled;
}

bool DICE_IsTimerRunning(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    pthread_mutex_lock(&timer->lock);
    bool running = (timer->state == DICE_TSTATE_RUNNING);
    pthread_mutex_unlock(&timer->lock);

    return running;
}

uint32_t DICE_ConsumeTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return 0;
    }

    return atomic_exchange(&dice_timers[handle].pending_intervals, 0);
}

double DICE_GetTimerDelta(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return 0.0;
    }

    dice_timer* timer = &dice_timers[handle];
    int64_t sample = dice_timer_now();
    if (sample < 0) {
        return 0.0;
    }
    int64_t last = atomic_exchange(&timer->last_delta, sample);

    if (last == 0) {
        return 0.0;
    }

    return (double)(sample - last) / 1e9;
}

bool dice_timer_retune(uint8_t handle, int64_t period_nanoseconds, int64_t phase_shift_nanoseconds) {
    if (!dice_timer_valid(handle) || period_nanoseconds <= 0) {
        return false;
    }
    dice_timer* timer = &dice_timers[handle];
    pthread_mutex_lock(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    int64_t adjusted = 0;
    if (timer->state == DICE_TSTATE_RUNNING &&
        !dice_timer_add(timer->next_fire, phase_shift_nanoseconds, &adjusted)) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    if (timer->state == DICE_TSTATE_PAUSED &&
        !dice_timer_add(timer->remaining, phase_shift_nanoseconds, &adjusted)) {
        pthread_mutex_unlock(&timer->lock);
        return false;
    }
    timer->period_nanoseconds = period_nanoseconds;
    if (timer->state == DICE_TSTATE_RUNNING) {
        timer->next_fire = adjusted;
    }
    if (timer->state == DICE_TSTATE_PAUSED) {
        timer->remaining = adjusted < 0 ? 0 : adjusted;
    }
    bool signaled = dice_timer_signal_locked(timer);
    pthread_mutex_unlock(&timer->lock);
    return signaled;
}

#endif
