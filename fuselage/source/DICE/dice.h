#ifndef DICE_H
#define DICE_H
#include <stdbool.h>
#include <stdint.h>

// Independent clocks, timers and random streams. SEE: (DICE.md, Lifecycle)
#define DICE_VERSION "0.5.2026102801 ENTERON"
bool DICE_Init(void);
void DICE_Shutdown(void);

// Monotonic elapsed time since initialization; zero outside that lifetime.
// Keep a previous timestamp per caller when calculating a delta.
double DICE_GetElapsedSeconds(void);
uint64_t DICE_GetElapsedMicroseconds(void);

// Guest-visible layout: preserve field order and widths. SEE: (DICE.md, Calendar time)
typedef struct {
    uint64_t utcSeconds;
    uint32_t milliseconds;
    int32_t utcOffsetMinutes;
    uint32_t isDst;
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t weekday;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t yearDay;
    uint32_t _pad;
} DICE_DateTime;
// Available before initialization. NULL is ignored; no error status is returned.
void DICE_GetRealTime(DICE_DateTime* out);

#include "dice_timers.h"
#include "dice_rng.h"
#include "dice_help.h"
#endif
