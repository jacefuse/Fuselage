// Platform entropy boundary. SEE: (DICE.md, Random streams)
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "dice_rng_private.h"

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>

uint64_t dice_rng_entropy_read(void) {
    uint64_t entropy = 0;
    if (BCryptGenRandom(NULL, (PUCHAR)&entropy, sizeof(entropy), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        return entropy;
    }
    LARGE_INTEGER counter = {0};
    if (!QueryPerformanceCounter(&counter)) {
        counter.QuadPart = 0;
    }
    return (uint64_t)counter.QuadPart ^ ((uint64_t)GetTickCount64() << 32);
}
#else
#include <sys/random.h>
#include <time.h>

uint64_t dice_rng_entropy_read(void) {
    uint64_t entropy = 0;
    if (getentropy(&entropy, sizeof(entropy)) == 0) {
        return entropy;
    }
    struct timespec sample = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &sample) != 0) {
        sample = (struct timespec){0};
    }
    return ((uint64_t)sample.tv_sec << 32) ^ (uint64_t)sample.tv_nsec;
}
#endif
