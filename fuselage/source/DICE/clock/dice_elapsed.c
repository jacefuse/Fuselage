// Monotonic elapsed time. SEE: (DICE.md, Elapsed time)
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../dice.h"
#include "dice_clock_private.h"
static bool dice_clock_ready;
#if defined(_WIN32)

#include <windows.h>

static LARGE_INTEGER dice_clock_frequency;
static LONGLONG dice_clock_epoch;

static void dice_clock_start_native(void) {
    QueryPerformanceFrequency(&dice_clock_frequency);

    LARGE_INTEGER sample;
    QueryPerformanceCounter(&sample);
    dice_clock_epoch = sample.QuadPart;

    return;
}

static uint64_t dice_clock_micros(void) {
    if (!dice_clock_ready) {
        return 0;
    }

    LARGE_INTEGER sample;
    QueryPerformanceCounter(&sample);

    LONGLONG ticks = sample.QuadPart - dice_clock_epoch;
    if (ticks < 0) {
        ticks = 0;
    }
    LONGLONG seconds = ticks / dice_clock_frequency.QuadPart;
    LONGLONG fraction = ticks % dice_clock_frequency.QuadPart;

    return (uint64_t)seconds * 1000000u + (uint64_t)((fraction * 1000000) / dice_clock_frequency.QuadPart);
}

#elif defined(__APPLE__)

#include <mach/mach_time.h>

static mach_timebase_info_data_t dice_timebase;
static uint64_t dice_clock_epoch;

static void dice_clock_start_native(void) {
    mach_timebase_info(&dice_timebase);
    dice_clock_epoch = mach_absolute_time();

    return;
}

static uint64_t dice_clock_micros(void) {
    if (!dice_clock_ready) {
        return 0;
    }

    uint64_t ticks = mach_absolute_time() - dice_clock_epoch;

    uint64_t nanoseconds = (ticks / dice_timebase.denom) * dice_timebase.numer +
                           ((ticks % dice_timebase.denom) * dice_timebase.numer) / dice_timebase.denom;

    return nanoseconds / 1000u;
}

#else

#include <time.h>

static struct timespec dice_clock_epoch;

static void dice_clock_start_native(void) {
    clock_gettime(CLOCK_MONOTONIC, &dice_clock_epoch);

    return;
}

static uint64_t dice_clock_micros(void) {
    if (!dice_clock_ready) {
        return 0;
    }

    struct timespec sample;
    clock_gettime(CLOCK_MONOTONIC, &sample);

    int64_t seconds = (int64_t)sample.tv_sec - (int64_t)dice_clock_epoch.tv_sec;
    int64_t nanoseconds = (int64_t)sample.tv_nsec - (int64_t)dice_clock_epoch.tv_nsec;
    if (nanoseconds < 0) {
        seconds -= 1;
        nanoseconds += 1000000000;
    }
    if (seconds < 0) {
        return 0;
    }

    return (uint64_t)seconds * 1000000u + (uint64_t)(nanoseconds / 1000);
}

#endif

uint64_t DICE_GetElapsedMicroseconds(void) {
    return dice_clock_micros();
}

double DICE_GetElapsedSeconds(void) {
    return (double)dice_clock_micros() / 1000000.0;
}

void dice_clock_start(void) {
    dice_clock_start_native();
    dice_clock_ready = true;
}
void dice_clock_stop(void) {
    dice_clock_ready = false;
}
