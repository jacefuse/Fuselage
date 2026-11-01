// Platform timer workers. SEE: (DICE.md, Timers and callbacks)
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "dice_timers_private.h"
#include "dice_timers_internal.h"
#include <limits.h>
#include <math.h>
#if defined(_WIN32)
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

typedef struct {
    bool initialized;
    bool joined;
    CRITICAL_SECTION lock;
    HANDLE waitable_timer;
    HANDLE wake_event;
    HANDLE thread;
    dice_timer_state state;
    LONGLONG period_ticks;
    LONGLONG next_fire;
    LONGLONG remaining;
    DICE_TimerCallback callback;
    void* user_data;
    volatile LONG pending_intervals;
    volatile LONGLONG last_delta;
} dice_timer;

static dice_timer dice_timers[DICE_MAX_TIMERS];
static LARGE_INTEGER dice_timer_frequency;
static bool dice_timer_frequency_ready = false;

static bool dice_timer_prepare_frequency(void) {
    if (dice_timer_frequency_ready) {
        return true;
    }
    if (!QueryPerformanceFrequency(&dice_timer_frequency) || dice_timer_frequency.QuadPart <= 0) {
        return false;
    }
    dice_timer_frequency_ready = true;
    return true;
}

bool dice_timer_valid(uint8_t handle) {
    return handle < DICE_MAX_TIMERS && dice_timers[handle].initialized;
}

static LONGLONG dice_timer_now(void) {
    LARGE_INTEGER sample;
    if (!QueryPerformanceCounter(&sample) || sample.QuadPart < 0) {
        return -1;
    }
    return sample.QuadPart;
}

static LONGLONG dice_timer_due_units(LONGLONG ticks) {
    if (ticks <= 0) {
        return 0;
    }
    LONGLONG seconds = ticks / dice_timer_frequency.QuadPart;
    LONGLONG fraction = ticks % dice_timer_frequency.QuadPart;
    if (seconds > INT64_MAX / 10000000) {
        return INT64_MAX;
    }
    LONGLONG whole = seconds * 10000000;
    LONGLONG remainder = fraction <= INT64_MAX / 10000000
                             ? fraction * 10000000 / dice_timer_frequency.QuadPart
                             : (LONGLONG)((long double)fraction / dice_timer_frequency.QuadPart * 10000000);
    return INT64_MAX - whole < remainder ? INT64_MAX : whole + remainder;
}

static DWORD WINAPI dice_timer_worker(LPVOID parameter) {
    dice_timer_worker_enter();
    uint8_t handle = (uint8_t)(uintptr_t)parameter;
    dice_timer* timer = &dice_timers[handle];
    for (;;) {
        EnterCriticalSection(&timer->lock);
        if (timer->state == DICE_TSTATE_RELEASING) {
            LeaveCriticalSection(&timer->lock);
            break;
        }
        if (timer->state != DICE_TSTATE_RUNNING) {
            LeaveCriticalSection(&timer->lock);
            if (WaitForSingleObject(timer->wake_event, INFINITE) != WAIT_OBJECT_0) {
                EnterCriticalSection(&timer->lock);
                timer->state = DICE_TSTATE_RELEASING;
                LeaveCriticalSection(&timer->lock);
                break;
            }
            continue;
        }
        LONGLONG sample = dice_timer_now();
        LARGE_INTEGER due;
        due.QuadPart =
            -dice_timer_due_units(timer->next_fire > sample && sample >= 0 ? timer->next_fire - sample : 0);
        if (sample < 0 || !SetWaitableTimerEx(timer->waitable_timer, &due, 0, NULL, NULL, NULL, 0)) {
            timer->state = DICE_TSTATE_RELEASING;
            LeaveCriticalSection(&timer->lock);
            break;
        }
        LeaveCriticalSection(&timer->lock);
        HANDLE waits[2] = {timer->waitable_timer, timer->wake_event};
        DWORD result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        EnterCriticalSection(&timer->lock);
        if (result != WAIT_OBJECT_0 && result != WAIT_OBJECT_0 + 1) {
            timer->state = DICE_TSTATE_RELEASING;
            LeaveCriticalSection(&timer->lock);
            break;
        }
        if (result == WAIT_OBJECT_0 && timer->state == DICE_TSTATE_RUNNING) {
            if (!dice_timer_add(timer->next_fire, timer->period_ticks, &timer->next_fire)) {
                timer->state = DICE_TSTATE_RELEASING;
                LeaveCriticalSection(&timer->lock);
                break;
            }
            InterlockedIncrement(&timer->pending_intervals);
            DICE_TimerCallback callback = timer->callback;
            void* user_data = timer->user_data;
            LeaveCriticalSection(&timer->lock);
            if (callback) {
                callback(handle, user_data);
            }
            EnterCriticalSection(&timer->lock);
            if (timer->state == DICE_TSTATE_RUNNING) {
                LONGLONG after = dice_timer_now();
                if (after < 0) {
                    timer->state = DICE_TSTATE_RELEASING;
                } else if (after >= timer->next_fire) {
                    uint64_t behind = (uint64_t)after - (uint64_t)timer->next_fire;
                    if (behind >= (uint64_t)timer->period_ticks) {
                        uint64_t missed = behind / (uint64_t)timer->period_ticks;
                        InterlockedAdd(&timer->pending_intervals, (LONG)(uint32_t)missed);
                        timer->next_fire = after;
                    }
                }
            }
        } else {
            CancelWaitableTimer(timer->waitable_timer);
        }
        LeaveCriticalSection(&timer->lock);
    }
    dice_timer_worker_leave();
    return 0;
}

static bool dice_timer_init(uint8_t handle, LONGLONG period_ticks, bool autostart,
                            DICE_TimerCallback callback, void* user_data) {
    if (handle >= DICE_MAX_TIMERS || period_ticks <= 0 || dice_timer_claimed[handle]) {
        return false;
    }
    dice_timer* timer = &dice_timers[handle];
    // The self-join trap only -- see the long note in dice_timers_posix.c. The
    // blanket `dice_timer_is_worker_thread()` refused every guest as well,
    // because a ROAP's slice runs on the simulation timer's worker.
    if (timer->initialized && GetCurrentThreadId() == GetThreadId(timer->thread)) {
        return false;
    }
    if (timer->initialized && !DICE_ReleaseTimer(handle)) {
        return false;
    }
    memset(timer, 0, sizeof(*timer));
    InitializeCriticalSection(&timer->lock);
    timer->initialized = timer->joined = true;
    timer->period_ticks = period_ticks;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->state = DICE_TSTATE_STOPPED;
    timer->waitable_timer =
        CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer->waitable_timer) {
        timer->waitable_timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    }
    if (!timer->waitable_timer) {
        goto fail;
    }
    timer->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!timer->wake_event) {
        goto fail;
    }
    timer->thread = CreateThread(NULL, 0, dice_timer_worker, (LPVOID)(uintptr_t)handle, 0, NULL);
    if (!timer->thread) {
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
    if (!isfinite(hz) || hz <= 0 || !dice_timer_prepare_frequency()) {
        return false;
    }
    int64_t period;
    if (!dice_timer_integer((double)dice_timer_frequency.QuadPart / hz, &period) || period <= 0) {
        return false;
    }
    return dice_timer_init(handle, period, autostart, callback, user_data);
}

bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                            DICE_TimerCallback callback, void* user_data) {
    if (!microseconds || !dice_timer_prepare_frequency()) {
        return false;
    }
    int64_t period;
    if (!dice_timer_integer(((double)microseconds / 1000000.0) * (double)dice_timer_frequency.QuadPart,
                            &period) ||
        period <= 0) {
        return false;
    }
    return dice_timer_init(handle, period, autostart, callback, user_data);
}

bool DICE_ReleaseTimer(uint8_t handle) {
    if (!dice_timer_valid(handle) || dice_timer_claimed[handle]) {
        return false;
    }
    dice_timer* timer = &dice_timers[handle];
    // The same self-join trap, reachable directly from a callback.
    if (GetCurrentThreadId() == GetThreadId(timer->thread)) {
        return false;
    }
    EnterCriticalSection(&timer->lock);
    timer->state = DICE_TSTATE_RELEASING;
    LeaveCriticalSection(&timer->lock);
    if (!timer->joined) {
        if (!SetEvent(timer->wake_event) || WaitForSingleObject(timer->thread, INFINITE) != WAIT_OBJECT_0) {
            return false;
        }
        timer->joined = true;
    }
    bool closed = true;
    HANDLE* owners[] = {&timer->thread, &timer->wake_event, &timer->waitable_timer};
    for (unsigned index = 0; index < 3; ++index) {
        if (*owners[index]) {
            if (CloseHandle(*owners[index])) {
                *owners[index] = NULL;
            } else {
                closed = false;
            }
        }
    }
    if (!closed) {
        return false;
    }
    DeleteCriticalSection(&timer->lock);
    memset(timer, 0, sizeof(*timer));
    return true;
}

static bool dice_timer_signal(dice_timer* timer) {
    if (SetEvent(timer->wake_event)) {
        return true;
    }
    EnterCriticalSection(&timer->lock);
    timer->state = DICE_TSTATE_RELEASING;
    LeaveCriticalSection(&timer->lock);
    return false;
}
bool DICE_StartTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    EnterCriticalSection(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    LONGLONG sample = dice_timer_now();
    int64_t target;
    if (sample < 0 || !dice_timer_add(sample, timer->period_ticks, &target)) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    InterlockedExchange(&timer->pending_intervals, 0);
    timer->next_fire = target;
    timer->state = DICE_TSTATE_RUNNING;
    LeaveCriticalSection(&timer->lock);
    return dice_timer_signal(timer);
}

bool DICE_StopTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    EnterCriticalSection(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    timer->state = DICE_TSTATE_STOPPED;
    InterlockedExchange(&timer->pending_intervals, 0);
    LeaveCriticalSection(&timer->lock);
    return dice_timer_signal(timer);
}

bool DICE_PauseTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    EnterCriticalSection(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    if (timer->state == DICE_TSTATE_RUNNING) {
        LONGLONG sample = dice_timer_now();
        if (sample < 0) {
            LeaveCriticalSection(&timer->lock);
            return false;
        }
        timer->remaining = timer->next_fire > sample ? timer->next_fire - sample : 0;
        if (timer->remaining < 0) {
            timer->remaining = 0;
        }
        timer->state = DICE_TSTATE_PAUSED;
    }
    LeaveCriticalSection(&timer->lock);
    return dice_timer_signal(timer);
}

bool DICE_ResumeTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    EnterCriticalSection(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    if (timer->state == DICE_TSTATE_PAUSED) {
        LONGLONG sample = dice_timer_now();
        int64_t target;
        if (sample < 0 || !dice_timer_add(sample, timer->remaining, &target)) {
            LeaveCriticalSection(&timer->lock);
            return false;
        }
        timer->next_fire = target;
        timer->state = DICE_TSTATE_RUNNING;
    }
    LeaveCriticalSection(&timer->lock);
    return dice_timer_signal(timer);
}

bool DICE_IsTimerRunning(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return false;
    }

    dice_timer* timer = &dice_timers[handle];

    EnterCriticalSection(&timer->lock);
    bool running = (timer->state == DICE_TSTATE_RUNNING);
    LeaveCriticalSection(&timer->lock);

    return running;
}

uint32_t DICE_ConsumeTimer(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return 0;
    }

    return (uint32_t)InterlockedExchange(&dice_timers[handle].pending_intervals, 0);
}

double DICE_GetTimerDelta(uint8_t handle) {
    if (!dice_timer_valid(handle)) {
        return 0.0;
    }

    dice_timer* timer = &dice_timers[handle];
    LONGLONG sample = dice_timer_now();
    if (sample < 0) {
        return 0.0;
    }
    LONGLONG last = InterlockedExchange64(&timer->last_delta, sample);

    if (last == 0) {
        return 0.0;
    }

    return (double)(sample - last) / (double)dice_timer_frequency.QuadPart;
}

bool dice_timer_retune(uint8_t handle, int64_t period_nanoseconds, int64_t phase_shift_nanoseconds) {
    if (!dice_timer_valid(handle) || period_nanoseconds <= 0 || !dice_timer_prepare_frequency()) {
        return false;
    }
    double ticks_per_nanosecond = (double)dice_timer_frequency.QuadPart / 1e9;
    int64_t period, shift;
    if (!dice_timer_integer((double)period_nanoseconds * ticks_per_nanosecond, &period) ||
        !dice_timer_integer((double)phase_shift_nanoseconds * ticks_per_nanosecond, &shift)) {
        return false;
    }
    if (period <= 0) {
        period = 1;
    }
    dice_timer* timer = &dice_timers[handle];
    EnterCriticalSection(&timer->lock);
    if (timer->state == DICE_TSTATE_RELEASING) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    int64_t adjusted = 0;
    if (timer->state == DICE_TSTATE_RUNNING && !dice_timer_add(timer->next_fire, shift, &adjusted)) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    if (timer->state == DICE_TSTATE_PAUSED && !dice_timer_add(timer->remaining, shift, &adjusted)) {
        LeaveCriticalSection(&timer->lock);
        return false;
    }
    timer->period_ticks = period;
    if (timer->state == DICE_TSTATE_RUNNING) {
        timer->next_fire = adjusted;
    }
    if (timer->state == DICE_TSTATE_PAUSED) {
        timer->remaining = adjusted < 0 ? 0 : adjusted;
    }
    LeaveCriticalSection(&timer->lock);
    return dice_timer_signal(timer);
}

#endif
