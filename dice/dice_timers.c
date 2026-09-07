// dice_timers.c - DICE timer backend. Version see DICE_VERSION in dice.h.
//
// Windows: one dedicated thread per active handle, waiting on a
// high-resolution waitable timer. Each fire re-arms against a fixed-cadence
// target (nextFireQpc += periodQpc) rather than relying on the OS's
// periodic-reload field -- that field is only millisecond-granular even on
// a high-resolution waitable timer, and re-arming against a running target
// (instead of "sleep periodMs again") avoids accumulating drift from wake
// latency. A second handle (hWake) rides along in every wait so
// Pause/Resume/Stop/Release can interrupt an in-progress wait immediately
// rather than waiting for it to time out on its own.

// Linux builds with strict -std=c11: clock_gettime and
// pthread_condattr_setclock must be requested before any header lands
// (macOS exposes them unasked, so this is Linux-only by necessity).
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "dice_timers.h"

#if defined(_WIN32)

#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// Internal state

typedef enum {
    DICE_TSTATE_STOPPED,
    DICE_TSTATE_RUNNING,
    DICE_TSTATE_PAUSED,
    DICE_TSTATE_RELEASING,
} DiceTimerState;

typedef struct {
    bool                initialized;
    CRITICAL_SECTION    lock;          // guards everything below except pendingIntervals/lastDeltaQpc
    HANDLE              hTimer;        // high-resolution waitable timer
    HANDLE              hWake;         // manual control event -- interrupts an in-progress wait on any state change
    HANDLE              hThread;
    DiceTimerState      state;
    LONGLONG            periodQpc;     // interval length, in QPC ticks
    LONGLONG            nextFireQpc;   // absolute QPC target for the next fire, valid while RUNNING
    LONGLONG            remainingQpc;  // ticks left until fire, valid while PAUSED
    DICE_TimerCallback  callback;
    void*               userdata;
    volatile LONG       pendingIntervals; // consumed atomically by DICE_ConsumeTimer
    volatile LONGLONG   lastDeltaQpc;     // last timestamp read by DICE_TimerDelta; 0 = never called
} DiceTimer;

static DiceTimer    g_timers[DICE_MAX_TIMERS];
static LARGE_INTEGER g_qpc_freq;
static bool          g_qpc_ready = false;

static void dice_timers_ensure_qpc(void) {
    if (g_qpc_ready) { return; }

    QueryPerformanceFrequency(&g_qpc_freq);
    g_qpc_ready = true;

    return;
}

static bool DiceTimerValid(uint8_t handle) {
    return handle < DICE_MAX_TIMERS && g_timers[handle].initialized;
}

static LONGLONG dice_qpc_now(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return now.QuadPart;
}

// Converts a duration in QPC ticks to 100ns units (SetWaitableTimerEx's due
// time granularity), split into whole seconds + remainder so the
// multiplication can't overflow a LONGLONG for any period worth supporting.
static LONGLONG dice_qpc_to_100ns(LONGLONG ticks) {
    if (ticks < 0) { ticks = 0; }

    LONGLONG seconds  = ticks / g_qpc_freq.QuadPart;
    LONGLONG fraction = ticks % g_qpc_freq.QuadPart;

    return seconds * 10000000LL + (fraction * 10000000LL) / g_qpc_freq.QuadPart;
}

// Timer thread -- one per active handle. See file header for why re-arming
// happens against a fixed-cadence target rather than a reloading period.
// The callback (if any) fires here, on this thread -- see dice_timers.h's
// thread-safety note. DICE_ReleaseTimer joins this thread before tearing
// down its handles, so it always exits cleanly on RELEASING.
static DWORD WINAPI dice_timer_thread_proc(LPVOID param) {
    uint8_t    handle = (uint8_t)(uintptr_t)param;
    DiceTimer* t      = &g_timers[handle];

    for (;;) {
        EnterCriticalSection(&t->lock);
        DiceTimerState state = t->state;

        if (state == DICE_TSTATE_RELEASING) {
            LeaveCriticalSection(&t->lock);
            break;
        }

        if (state != DICE_TSTATE_RUNNING) {
            LeaveCriticalSection(&t->lock);
            WaitForSingleObject(t->hWake, INFINITE);
            continue;
        }

        LARGE_INTEGER due;
        due.QuadPart = -dice_qpc_to_100ns(t->nextFireQpc - dice_qpc_now());
        SetWaitableTimerEx(t->hTimer, &due, 0, NULL, NULL, NULL, 0);
        LeaveCriticalSection(&t->lock);

        HANDLE waits[2] = { t->hTimer, t->hWake };
        DWORD  result   = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        EnterCriticalSection(&t->lock);
        if (result == WAIT_OBJECT_0 && t->state == DICE_TSTATE_RUNNING) {
            t->nextFireQpc += t->periodQpc;
            InterlockedIncrement(&t->pendingIntervals);

            DICE_TimerCallback cb = t->callback;
            void*              ud = t->userdata;
            LeaveCriticalSection(&t->lock);

            if (cb) { cb(handle, ud); }
        } else {
            CancelWaitableTimer(t->hTimer);
            LeaveCriticalSection(&t->lock);
        }
    }

    return 0;
}

// Shared setup for both public Init forms -- everything but computing
// periodQpc from the caller's chosen unit is identical.
static bool dice_timer_init(uint8_t handle, LONGLONG periodQpc, bool autostart,
                             DICE_TimerCallback callback, void* userdata) {
    if (handle >= DICE_MAX_TIMERS || periodQpc <= 0) { return false; }

    DiceTimer* t = &g_timers[handle];
    if (t->initialized) { DICE_ReleaseTimer(handle); }

    memset(t, 0, sizeof(*t));

    t->hTimer = CreateWaitableTimerExW(NULL, NULL,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!t->hTimer) {
        // High-resolution timers need Windows 10 1803+; fall back to a
        // standard waitable timer on anything older.
        t->hTimer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    }
    if (!t->hTimer) {
        printf("[DICE] handle %d: CreateWaitableTimerEx failed\n", handle);
        return false;
    }

    t->hWake = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!t->hWake) {
        printf("[DICE] handle %d: CreateEvent failed\n", handle);
        CloseHandle(t->hTimer);
        t->hTimer = NULL;
        return false;
    }

    InitializeCriticalSection(&t->lock);

    t->periodQpc   = periodQpc;
    t->callback    = callback;
    t->userdata    = userdata;
    t->state       = DICE_TSTATE_STOPPED;
    t->initialized = true;

    t->hThread = CreateThread(NULL, 0, dice_timer_thread_proc,
                               (LPVOID)(uintptr_t)handle, 0, NULL);
    if (!t->hThread) {
        printf("[DICE] handle %d: CreateThread failed\n", handle);
        DeleteCriticalSection(&t->lock);
        CloseHandle(t->hWake);
        CloseHandle(t->hTimer);
        memset(t, 0, sizeof(*t));
        return false;
    }

    if (autostart) { DICE_StartTimer(handle); }

    return true;
}

bool DICE_InitTimer(uint8_t handle, double hz, bool autostart,
                     DICE_TimerCallback callback, void* userdata) {
    if (hz <= 0.0) { return false; }

    dice_timers_ensure_qpc();

    return dice_timer_init(handle, (LONGLONG)((double)g_qpc_freq.QuadPart / hz),
                            autostart, callback, userdata);
}

bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                             DICE_TimerCallback callback, void* userdata) {
    if (microseconds == 0) { return false; }

    dice_timers_ensure_qpc();

    LONGLONG periodQpc = (LONGLONG)(((double)microseconds / 1000000.0) *
                                     (double)g_qpc_freq.QuadPart);

    return dice_timer_init(handle, periodQpc, autostart, callback, userdata);
}

bool DICE_ReleaseTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    EnterCriticalSection(&t->lock);
    t->state = DICE_TSTATE_RELEASING;
    LeaveCriticalSection(&t->lock);
    SetEvent(t->hWake);

    WaitForSingleObject(t->hThread, INFINITE);

    CloseHandle(t->hThread);
    CloseHandle(t->hWake);
    CloseHandle(t->hTimer);
    DeleteCriticalSection(&t->lock);

    memset(t, 0, sizeof(*t));

    return true;
}

bool DICE_StartTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    EnterCriticalSection(&t->lock);
    t->pendingIntervals = 0;
    t->nextFireQpc      = dice_qpc_now() + t->periodQpc;
    t->state            = DICE_TSTATE_RUNNING;
    LeaveCriticalSection(&t->lock);
    SetEvent(t->hWake);

    return true;
}

bool DICE_StopTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    EnterCriticalSection(&t->lock);
    t->state            = DICE_TSTATE_STOPPED;
    t->pendingIntervals = 0;
    LeaveCriticalSection(&t->lock);
    SetEvent(t->hWake);

    return true;
}

bool DICE_PauseTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    EnterCriticalSection(&t->lock);
    if (t->state == DICE_TSTATE_RUNNING) {
        t->remainingQpc = t->nextFireQpc - dice_qpc_now();
        if (t->remainingQpc < 0) { t->remainingQpc = 0; }
        t->state = DICE_TSTATE_PAUSED;
    }
    LeaveCriticalSection(&t->lock);
    SetEvent(t->hWake);

    return true;
}

bool DICE_ResumeTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    EnterCriticalSection(&t->lock);
    if (t->state == DICE_TSTATE_PAUSED) {
        t->nextFireQpc = dice_qpc_now() + t->remainingQpc;
        t->state       = DICE_TSTATE_RUNNING;
    }
    LeaveCriticalSection(&t->lock);
    SetEvent(t->hWake);

    return true;
}

bool DICE_IsTimerRunning(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    EnterCriticalSection(&t->lock);
    bool running = (t->state == DICE_TSTATE_RUNNING);
    LeaveCriticalSection(&t->lock);

    return running;
}

uint32_t DICE_ConsumeTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return 0; }

    return (uint32_t)InterlockedExchange(&g_timers[handle].pendingIntervals, 0);
}

double DICE_TimerDelta(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return 0.0; }

    DiceTimer* t    = &g_timers[handle];
    LONGLONG   now  = dice_qpc_now();
    LONGLONG   last = InterlockedExchange64(&t->lastDeltaQpc, now);

    if (last == 0) { return 0.0; }

    return (double)(now - last) / (double)g_qpc_freq.QuadPart;
}

// LINUX / MACOS -- pthread port of the Windows backend above, same design:
// one dedicated thread per active handle, fixed-cadence re-arming
// (nextFireNs += periodNs) so wake latency never accumulates as drift, and
// every wait interruptible by a state change. The waitable-timer/hWake pair
// becomes a single condvar: the timed wait doubles as the timer, and
// signaling the condvar is the wake. Time is CLOCK_MONOTONIC nanoseconds.
//
// The one platform split inside this branch is the timed wait itself: a
// monotonic-clock condvar (pthread_condattr_setclock) is standard on Linux
// but unsupported on macOS, where Apple instead provides
// pthread_cond_timedwait_relative_np -- a relative-duration wait, which
// suits a fixed-cadence target just as well (remaining = nextFire - now).

#elif defined(__linux__) || defined(__APPLE__)

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum {
    DICE_TSTATE_STOPPED,
    DICE_TSTATE_RUNNING,
    DICE_TSTATE_PAUSED,
    DICE_TSTATE_RELEASING,
} DiceTimerState;

typedef struct {
    bool                initialized;
    pthread_mutex_t     lock;          // guards everything below except pendingIntervals/lastDeltaNs
    pthread_cond_t      cond;          // the timed wait AND the wake -- signaled on any state change
    pthread_t           thread;
    DiceTimerState      state;
    int64_t             periodNs;      // interval length
    int64_t             nextFireNs;    // absolute CLOCK_MONOTONIC target for the next fire, valid while RUNNING
    int64_t             remainingNs;   // time left until fire, valid while PAUSED
    DICE_TimerCallback  callback;
    void*               userdata;
    _Atomic uint32_t    pendingIntervals; // consumed atomically by DICE_ConsumeTimer
    _Atomic int64_t     lastDeltaNs;      // last timestamp read by DICE_TimerDelta; 0 = never called
} DiceTimer;

static DiceTimer g_timers[DICE_MAX_TIMERS];

static bool DiceTimerValid(uint8_t handle) {
    return handle < DICE_MAX_TIMERS && g_timers[handle].initialized;
}

static int64_t dice_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// Timer thread -- one per active handle. See file header for why re-arming
// happens against a fixed-cadence target rather than a reloading period.
// The callback (if any) fires here, on this thread -- see dice_timers.h's
// thread-safety note. DICE_ReleaseTimer joins this thread before tearing
// down its handles, so it always exits cleanly on RELEASING.
static void* dice_timer_thread_proc(void* param) {
    uint8_t    handle = (uint8_t)(uintptr_t)param;
    DiceTimer* t      = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    for (;;) {
        if (t->state == DICE_TSTATE_RELEASING) { break; }

        if (t->state != DICE_TSTATE_RUNNING) {
            pthread_cond_wait(&t->cond, &t->lock);
            continue;   // re-derive everything on wake (state may have changed)
        }

        int64_t now = dice_now_ns();
        if (now < t->nextFireNs) {
            // Not due yet: timed wait until the target (or a state-change
            // signal, or a spurious wake -- the loop re-derives regardless).
#if defined(__APPLE__)
            int64_t remaining = t->nextFireNs - now;
            struct timespec rel = { (time_t)(remaining / 1000000000LL),
                                    (long)(remaining % 1000000000LL) };
            pthread_cond_timedwait_relative_np(&t->cond, &t->lock, &rel);
#else
            struct timespec abs = { (time_t)(t->nextFireNs / 1000000000LL),
                                    (long)(t->nextFireNs % 1000000000LL) };
            pthread_cond_timedwait(&t->cond, &t->lock, &abs);
#endif
            continue;
        }

        t->nextFireNs += t->periodNs;
        atomic_fetch_add(&t->pendingIntervals, 1);

        DICE_TimerCallback cb = t->callback;
        void*              ud = t->userdata;
        pthread_mutex_unlock(&t->lock);

        if (cb) { cb(handle, ud); }

        pthread_mutex_lock(&t->lock);
    }
    pthread_mutex_unlock(&t->lock);

    return NULL;
}

// Shared setup for both public Init forms -- everything but computing
// periodNs from the caller's chosen unit is identical.
static bool dice_timer_init(uint8_t handle, int64_t periodNs, bool autostart,
                             DICE_TimerCallback callback, void* userdata) {
    if (handle >= DICE_MAX_TIMERS || periodNs <= 0) { return false; }

    DiceTimer* t = &g_timers[handle];
    if (t->initialized) { DICE_ReleaseTimer(handle); }

    memset(t, 0, sizeof(*t));

    if (pthread_mutex_init(&t->lock, NULL) != 0) {
        printf("[DICE] handle %d: mutex init failed\n", handle);
        return false;
    }

    // Plain condattr on macOS (the relative wait needs no clock choice);
    // CLOCK_MONOTONIC on Linux so the absolute wait can't be yanked around
    // by wall-clock changes.
#if defined(__APPLE__)
    int cond_ok = pthread_cond_init(&t->cond, NULL);
#else
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    int cond_ok = pthread_cond_init(&t->cond, &cattr);
    pthread_condattr_destroy(&cattr);
#endif
    if (cond_ok != 0) {
        printf("[DICE] handle %d: cond init failed\n", handle);
        pthread_mutex_destroy(&t->lock);
        return false;
    }

    t->periodNs    = periodNs;
    t->callback    = callback;
    t->userdata    = userdata;
    t->state       = DICE_TSTATE_STOPPED;
    t->initialized = true;

    if (pthread_create(&t->thread, NULL, dice_timer_thread_proc,
                       (void*)(uintptr_t)handle) != 0) {
        printf("[DICE] handle %d: thread create failed\n", handle);
        pthread_cond_destroy(&t->cond);
        pthread_mutex_destroy(&t->lock);
        memset(t, 0, sizeof(*t));
        return false;
    }

    if (autostart) { DICE_StartTimer(handle); }

    return true;
}

bool DICE_InitTimer(uint8_t handle, double hz, bool autostart,
                     DICE_TimerCallback callback, void* userdata) {
    if (hz <= 0.0) { return false; }

    return dice_timer_init(handle, (int64_t)(1000000000.0 / hz),
                            autostart, callback, userdata);
}

bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                             DICE_TimerCallback callback, void* userdata) {
    if (microseconds == 0) { return false; }

    return dice_timer_init(handle, (int64_t)microseconds * 1000LL,
                            autostart, callback, userdata);
}

bool DICE_ReleaseTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    t->state = DICE_TSTATE_RELEASING;
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);

    pthread_join(t->thread, NULL);

    pthread_cond_destroy(&t->cond);
    pthread_mutex_destroy(&t->lock);

    memset(t, 0, sizeof(*t));

    return true;
}

bool DICE_StartTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    atomic_store(&t->pendingIntervals, 0);
    t->nextFireNs = dice_now_ns() + t->periodNs;
    t->state      = DICE_TSTATE_RUNNING;
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);

    return true;
}

bool DICE_StopTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    t->state = DICE_TSTATE_STOPPED;
    atomic_store(&t->pendingIntervals, 0);
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);

    return true;
}

bool DICE_PauseTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    if (t->state == DICE_TSTATE_RUNNING) {
        t->remainingNs = t->nextFireNs - dice_now_ns();
        if (t->remainingNs < 0) { t->remainingNs = 0; }
        t->state = DICE_TSTATE_PAUSED;
    }
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);

    return true;
}

bool DICE_ResumeTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    if (t->state == DICE_TSTATE_PAUSED) {
        t->nextFireNs = dice_now_ns() + t->remainingNs;
        t->state      = DICE_TSTATE_RUNNING;
    }
    pthread_cond_signal(&t->cond);
    pthread_mutex_unlock(&t->lock);

    return true;
}

bool DICE_IsTimerRunning(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return false; }

    DiceTimer* t = &g_timers[handle];

    pthread_mutex_lock(&t->lock);
    bool running = (t->state == DICE_TSTATE_RUNNING);
    pthread_mutex_unlock(&t->lock);

    return running;
}

uint32_t DICE_ConsumeTimer(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return 0; }

    return atomic_exchange(&g_timers[handle].pendingIntervals, 0);
}

double DICE_TimerDelta(uint8_t handle) {
    if (!DiceTimerValid(handle)) { return 0.0; }

    DiceTimer* t    = &g_timers[handle];
    int64_t    now  = dice_now_ns();
    int64_t    last = atomic_exchange(&t->lastDeltaNs, now);

    if (last == 0) { return 0.0; }

    return (double)(now - last) / 1e9;
}

#endif
