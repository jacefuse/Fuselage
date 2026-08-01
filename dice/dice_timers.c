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

// LINUX / MACOS -- not yet implemented. Stubs present so the engine keeps
// building on those platforms; see dice_timers.h for the contract these
// need to satisfy once a real backend (POSIX timer_create w/ SIGEV_THREAD
// on Linux, GCD dispatch_source on macOS) lands.

#elif defined(__linux__) || defined(__APPLE__)

bool DICE_InitTimer(uint8_t handle, double hz, bool autostart,
                     DICE_TimerCallback callback, void* userdata) {
    (void)handle; (void)hz; (void)autostart; (void)callback; (void)userdata;

    return false;
}

bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                             DICE_TimerCallback callback, void* userdata) {
    (void)handle; (void)microseconds; (void)autostart; (void)callback; (void)userdata;

    return false;
}

bool DICE_ReleaseTimer(uint8_t handle)   { (void)handle;

    return false; }
bool DICE_StartTimer(uint8_t handle)     { (void)handle;

    return false; }
bool DICE_StopTimer(uint8_t handle)      { (void)handle;

    return false; }
bool DICE_PauseTimer(uint8_t handle)     { (void)handle;

    return false; }
bool DICE_ResumeTimer(uint8_t handle)    { (void)handle;

    return false; }
bool DICE_IsTimerRunning(uint8_t handle) { (void)handle;

    return false; }

uint32_t DICE_ConsumeTimer(uint8_t handle) { (void)handle;

    return 0; }
double DICE_TimerDelta(uint8_t handle)     { (void)handle;

    return 0.0; }

#endif