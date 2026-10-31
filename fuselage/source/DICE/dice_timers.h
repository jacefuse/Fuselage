// SEE: (DICE.md, Timers and callbacks)
#ifndef DICE_TIMERS_H
#define DICE_TIMERS_H

#include <stdbool.h>
#include <stdint.h>

#define DICE_MAX_TIMERS 32

bool DICE_LockTimer(uint8_t handle);
bool DICE_UnlockTimer(uint8_t handle);
bool DICE_IsTimerLocked(uint8_t handle);

int DICE_AllocTimer(void);

typedef void (*DICE_TimerCallback)(uint8_t handle, void* userdata);

bool DICE_InitTimer(uint8_t handle, double hz, bool autostart, DICE_TimerCallback callback, void* userdata);
bool DICE_InitTimerInterval(uint8_t handle, uint64_t microseconds, bool autostart,
                            DICE_TimerCallback callback, void* userdata);

bool DICE_ReleaseTimer(uint8_t handle);

bool DICE_StartTimer(uint8_t handle);
bool DICE_StopTimer(uint8_t handle);
bool DICE_PauseTimer(uint8_t handle);
bool DICE_ResumeTimer(uint8_t handle);
bool DICE_IsTimerRunning(uint8_t handle);

uint32_t DICE_ConsumeTimer(uint8_t handle);
double DICE_GetTimerDelta(uint8_t handle);

#endif
