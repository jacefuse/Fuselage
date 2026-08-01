// dice_rng.c - DICE RNG backend. Version see DICE_VERSION in dice.h.
//
// Algorithm: PCG32 (O'Neill, "PCG: A Family of Simple Fast Space-Efficient
// Statistically Good Algorithms for Random Number Generation"). Chosen over
// xorshift128+ for a smaller per-stream footprint (two uint64_t: state +
// stream selector, vs. 128 bits of pure state) and because the stream
// selector gives DICE_MAX_RNG independent, decorrelated sequences from the
// same seed for free -- each handle seeds its own stream with the handle
// itself as PCG's "sequence" parameter, so two handles initialized with an
// identical seed still produce different output. Public domain / permissive
// reference algorithm; this is an independent reimplementation of the
// well-documented recurrence, not a copy of any particular source file.
//
// Windows entropy source: BCryptGenRandom w/ BCRYPT_USE_SYSTEM_PREFERRED_RNG
// (no explicit algorithm provider handle to open/close). Falls back to
// QueryPerformanceCounter + GetTickCount64 if BCryptGenRandom ever fails --
// not cryptographic, but keeps ENTROPY_SEEDED/CONTINUOUS streams from
// silently collapsing to all-zero entropy (and therefore fully predictable
// output) if bcrypt is ever unavailable.

#include "dice_rng.h"
#include <string.h>

#if defined(_WIN32)

#include <windows.h>
#include <bcrypt.h>

typedef struct {
    bool         initialized;
    uint64_t     state;
    uint64_t     inc;
    uint64_t     seed;   // verbatim caller-supplied seed, for DICE_GetRNGSeed
    DICE_RNGMode mode;
} DiceRngStream;

static DiceRngStream g_rng[DICE_MAX_RNG];

static bool DiceRngValid(uint8_t handle) {
    return handle < DICE_MAX_RNG && g_rng[handle].initialized;
}

// Best-effort OS entropy. Not used for anything security-sensitive -- DICE
// RNG is a gameplay/procgen system, not a crypto one -- so a non-BCrypt
// fallback is an acceptable degrade rather than a hard failure.
static void dice_rng_os_entropy(void* buf, size_t len) {
    NTSTATUS status = BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len,
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    if (status == 0) { return; }

    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    uint64_t fallback = (uint64_t)qpc.QuadPart ^ ((uint64_t)GetTickCount64() << 32);

    uint8_t*     out = (uint8_t*)buf;
    const uint8_t* src = (const uint8_t*)&fallback;
    for (size_t i = 0; i < len; i++) {
        out[i] = src[i % sizeof(fallback)];
    }

    return;
}

// Core PCG32 step. Advances state and returns one 32-bit output.
static uint32_t pcg32_next(DiceRngStream* s) {
    uint64_t oldstate = s->state;

    s->state = oldstate * 6364136223846793005ULL + s->inc;

    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot        = (uint32_t)(oldstate >> 59u);

    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

// Standard PCG32 seeding: initseq selects which of 2^63 independent
// sequences this stream walks, initstate is the starting point on it. Both
// calls to pcg32_next here are part of the canonical seeding procedure (the
// first churns the just-set inc into state before initstate is added, the
// second churns the real starting state once before any output is handed
// out), not arbitrary warm-up.
static void pcg32_seed(DiceRngStream* s, uint64_t initstate, uint64_t initseq) {
    s->state = 0u;
    s->inc   = (initseq << 1u) | 1u;
    pcg32_next(s);
    s->state += initstate;
    pcg32_next(s);

    return;
}

bool DICE_InitRNG(uint8_t handle, DICE_RNGMode mode, uint64_t seed) {
    if (handle >= DICE_MAX_RNG) { return false; }

    DiceRngStream* s = &g_rng[handle];
    memset(s, 0, sizeof(*s));

    uint64_t initstate = seed;

    switch (mode) {
        case DICE_RNG_DETERMINISTIC:
            // seed used verbatim, no entropy involved.
            break;

        case DICE_RNG_ENTROPY_SEEDED: {
            uint64_t entropy = 0;
            dice_rng_os_entropy(&entropy, sizeof(entropy));
            initstate = seed ^ entropy;
            break;
        }

        case DICE_RNG_CONTINUOUS: {
            uint64_t entropy = 0;
            dice_rng_os_entropy(&entropy, sizeof(entropy));
            initstate = seed ^ entropy;
            break;
        }

        case DICE_RNG_ACCUMULATE:
            // Never touched automatically after this -- only
            // DICE_MixRNGEntropy grows it from here.
            break;
    }

    s->mode        = mode;
    s->seed        = seed;
    s->initialized = true;
    pcg32_seed(s, initstate, (uint64_t)handle);

    return true;
}

bool DICE_ReleaseRNG(uint8_t handle) {
    if (!DiceRngValid(handle)) { return false; }

    memset(&g_rng[handle], 0, sizeof(g_rng[handle]));

    return true;
}

bool DICE_MixRNGEntropy(uint8_t handle, const void* data, size_t len) {
    if (!DiceRngValid(handle) || !data || len == 0) { return false; }

    DiceRngStream* s   = &g_rng[handle];
    const uint8_t* bytes = (const uint8_t*)data;

    // FNV-1a-style diffusion of the incoming bytes into a single 64-bit
    // value, then folded into (not replacing) the existing state, per the
    // header contract. pcg32_next() afterward churns state through the real
    // recurrence once so the mixed-in bits actually influence the next
    // output rather than sitting inert until the following natural advance.
    uint64_t mix = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        mix ^= bytes[i];
        mix *= 0x100000001b3ULL;
    }

    s->state += mix;
    pcg32_next(s);

    return true;
}

uint64_t DICE_GetRNGSeed(uint8_t handle) {
    if (!DiceRngValid(handle)) { return 0; }

    return g_rng[handle].seed;
}

uint32_t DICE_RandUint(uint8_t handle) {
    if (!DiceRngValid(handle)) { return 0; }

    DiceRngStream* s = &g_rng[handle];

    if (s->mode == DICE_RNG_CONTINUOUS) {
        uint64_t entropy = 0;

        dice_rng_os_entropy(&entropy, sizeof(entropy));
        DICE_MixRNGEntropy(handle, &entropy, sizeof(entropy));
    }

    return pcg32_next(s);
}

int DICE_RandInt(uint8_t handle, int min, int max) {
    if (!DiceRngValid(handle)) { return min; }

    if (max < min) { int t = min;

 min = max; max = t; }

    uint32_t range = (uint32_t)((int64_t)max - (int64_t)min) + 1u;
    if (range == 0u) {
        // min == INT_MIN, max == INT_MAX: the full uint32 range is valid,
        // no rejection sampling needed or even possible (every output would
        // be rejected against a modulus of 0).
        return (int)DICE_RandUint(handle);
    }

    // Lemire/Java-style rejection sampling: reject draws that would bias
    // the low bucket when UINT32_MAX+1 isn't an exact multiple of range,
    // rather than a plain "% range" (which skews toward small remainders).
    uint32_t threshold = (uint32_t)(-(int32_t)range) % range;
    uint32_t r;
    do {
        r = DICE_RandUint(handle);
    } while (r < threshold);

    return min + (int)(r % range);
}

float DICE_RandFloat(uint8_t handle) {
    if (!DiceRngValid(handle)) { return 0.0f; }

    // 24 bits of the draw -> exactly representable as a float mantissa,
    // scaled into [0, 1).
    return (float)(DICE_RandUint(handle) >> 8) * (1.0f / 16777216.0f);
}

bool DICE_RandChance(uint8_t handle, float probability) {
    if (probability <= 0.0f) { return false; }
    if (probability >= 1.0f) { return true; }

    return DICE_RandFloat(handle) < probability;
}

// LINUX / MACOS -- not yet implemented. Stubs present so the engine keeps
// building on those platforms, matching DICE Timer's own precedent (see
// dice_timers.c); a real backend needs a non-BCrypt entropy source
// (getrandom()/SecRandomCopyBytes) before this can do more than stub out.

#elif defined(__linux__) || defined(__APPLE__)

bool DICE_InitRNG(uint8_t handle, DICE_RNGMode mode, uint64_t seed) {
    (void)handle; (void)mode; (void)seed;

    return false;
}

bool DICE_ReleaseRNG(uint8_t handle) { (void)handle;

    return false; }

bool DICE_MixRNGEntropy(uint8_t handle, const void* data, size_t len) {
    (void)handle; (void)data; (void)len;

    return false;
}

uint64_t DICE_GetRNGSeed(uint8_t handle) { (void)handle;

    return 0; }

uint32_t DICE_RandUint(uint8_t handle) { (void)handle;

    return 0; }

int DICE_RandInt(uint8_t handle, int min, int max) { (void)handle; (void)max;

    return min; }

float DICE_RandFloat(uint8_t handle) { (void)handle;

    return 0.0f; }

bool DICE_RandChance(uint8_t handle, float probability) { (void)handle; (void)probability;

    return false; }

#endif