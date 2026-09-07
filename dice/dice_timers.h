#ifndef DICE_TIMERS_H
#define DICE_TIMERS_H

#include <stdbool.h>
#include <stdint.h>

// DICE Timers
//
// Up to DICE_MAX_TIMERS independent timer slots. Each timer is backed by a
// platform timer primitive (Windows: timer queue; Linux: POSIX timer_create
// w/ SIGEV_THREAD; macOS: GCD dispatch source) that fires on its own worker
// thread, NOT the thread that called DICE_InitTimer. DICE guarantees its
// own internal state (the elapsed-interval counter, the handle table) is
// thread-safe; it does NOT and cannot make an arbitrary caller-supplied
// callback body thread-safe. A callback that touches state shared with
// another thread is the caller's synchronization problem, not DICE's.
//
// Every timer supports both consumption styles at once, at no extra cost;
// pick whichever fits the caller, mix freely:
//   - Poll style: call DICE_ConsumeTimer() from wherever you'd normally
//     check "has enough time passed yet" (e.g. once per main-loop
//     iteration). Costs an atomic swap, no busy-waiting.
//   - Callback style: pass a non-NULL callback at init; it fires directly
//     on the timer's worker thread every time the interval elapses, in
//     addition to (not instead of) incrementing the counter DICE_ConsumeTimer
//     reads.
// Pass NULL for callback to use a timer in pure-poll mode.

#define DICE_MAX_TIMERS 32

// Invoked on the timer's own worker thread. See thread-safety note above.
// handle identifies which timer fired, so one function can service several.
// userdata is whatever was passed to DICE_InitTimer/DICE_InitTimerInterval.
typedef void (*DICE_TimerCallback)(uint8_t handle, void* userdata);

// Lifecycle
// Both forms configure the same kind of timer; pick whichever unit is
// natural for the call site. A timer initialized via either form supports
// the full API below identically. autostart begins the timer immediately
// (equivalent to an implicit DICE_StartTimer before this call returns);
// pass false to leave it idle until an explicit DICE_StartTimer/Resume.
bool DICE_InitTimer(uint8_t handle, double hz, bool autostart,
                     DICE_TimerCallback callback, void* userdata);
bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                             DICE_TimerCallback callback, void* userdata);

// Tears down the platform timer resource and frees the slot for reuse.
// Returns false if handle is out of range or the slot isn't initialized.
bool DICE_ReleaseTimer(uint8_t handle);

// Start/stop/pause
// Stop fully tears down accounting (a subsequent Start begins a fresh
// interval from zero); Pause/Resume freeze and resume in place, preserving
// whatever partial progress had accumulated toward the next interval. All
// return false if handle is out of range or uninitialized.
bool DICE_StartTimer(uint8_t handle);
bool DICE_StopTimer(uint8_t handle);
bool DICE_PauseTimer(uint8_t handle);
bool DICE_ResumeTimer(uint8_t handle);
bool DICE_IsTimerRunning(uint8_t handle);

// Consumption
// Consume: atomically reads and resets the count of intervals that have
// elapsed since the last call. 0 if none, 1 under normal pacing, >1 if the
// caller fell behind (the standard fixed-timestep catch-up signal).
// Delta: raw seconds elapsed since the last DICE_TimerDelta call on this
// handle, independent of the interval/consume accounting -- for callers
// that want continuous variable-rate timing (e.g. render interpolation)
// rather than discrete gated ticks.
uint32_t DICE_ConsumeTimer(uint8_t handle);
double   DICE_TimerDelta(uint8_t handle);

#endif // DICE_TIMERS_H
