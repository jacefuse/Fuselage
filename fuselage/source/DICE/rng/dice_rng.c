// PCG32 streams. SEE: (DICE.md, Random streams)
#include "../dice_rng.h"
#include "dice_rng_private.h"
#include <limits.h>
#include <string.h>
_Static_assert(INT_MAX == INT32_MAX && INT_MIN == INT32_MIN, "DICE integer ranges require 32-bit int");
typedef struct {
    bool initialized;
    uint64_t state;
    uint64_t increment;
    uint64_t seed;
    DICE_RNGMode mode;
} dice_rng_stream;

static dice_rng_stream dice_rng_streams[DICE_MAX_RNG];

static bool dice_rng_valid(uint8_t handle) {
    return handle < DICE_MAX_RNG && dice_rng_streams[handle].initialized;
}

static uint32_t dice_rng_next(dice_rng_stream* stream) {
    uint64_t previous_state = stream->state;
    stream->state = previous_state * 6364136223846793005ULL + stream->increment;

    uint32_t permuted_bits = (uint32_t)(((previous_state >> 18u) ^ previous_state) >> 27u);
    uint32_t rotation = (uint32_t)(previous_state >> 59u);

    return (permuted_bits >> rotation) | (permuted_bits << ((-rotation) & 31u));
}

static void dice_rng_seed(dice_rng_stream* stream, uint64_t initial_state, uint64_t sequence) {
    stream->state = 0u;
    stream->increment = (sequence << 1u) | 1u;
    dice_rng_next(stream);
    stream->state += initial_state;
    dice_rng_next(stream);

    return;
}

bool DICE_InitRNG(uint8_t handle, DICE_RNGMode mode, uint64_t seed) {
    if (handle >= DICE_MAX_RNG || (unsigned)mode > DICE_RNG_ACCUMULATE) {
        return false;
    }

    dice_rng_stream* stream = &dice_rng_streams[handle];
    memset(stream, 0, sizeof(*stream));

    uint64_t initial_state = seed;

    switch (mode) {
    case DICE_RNG_DETERMINISTIC:

        break;

    case DICE_RNG_ENTROPY_SEEDED:
    case DICE_RNG_CONTINUOUS: {
        uint64_t entropy = dice_rng_entropy_read();
        initial_state = seed ^ entropy;
        break;
    }

    case DICE_RNG_ACCUMULATE:

        break;
    }

    stream->mode = mode;
    stream->seed = seed;
    stream->initialized = true;
    dice_rng_seed(stream, initial_state, (uint64_t)handle);

    return true;
}

bool DICE_ReleaseRNG(uint8_t handle) {
    if (!dice_rng_valid(handle)) {
        return false;
    }

    memset(&dice_rng_streams[handle], 0, sizeof(dice_rng_streams[handle]));

    return true;
}

bool DICE_MixRNGEntropy(uint8_t handle, const void* data, size_t byte_count) {
    if (!dice_rng_valid(handle) || !data || byte_count == 0) {
        return false;
    }

    dice_rng_stream* stream = &dice_rng_streams[handle];
    const uint8_t* bytes = (const uint8_t*)data;

    uint64_t mix = 0xcbf29ce484222325ULL;
    for (size_t byte_index = 0; byte_index < byte_count; byte_index++) {
        mix ^= bytes[byte_index];
        mix *= 0x100000001b3ULL;
    }

    stream->state += mix;
    dice_rng_next(stream);

    return true;
}

uint64_t DICE_GetRNGSeed(uint8_t handle) {
    if (!dice_rng_valid(handle)) {
        return 0;
    }

    return dice_rng_streams[handle].seed;
}

uint32_t DICE_RandUint(uint8_t handle) {
    if (!dice_rng_valid(handle)) {
        return 0;
    }

    dice_rng_stream* stream = &dice_rng_streams[handle];

    if (stream->mode == DICE_RNG_CONTINUOUS) {
        uint64_t entropy = dice_rng_entropy_read();
        DICE_MixRNGEntropy(handle, &entropy, sizeof(entropy));
    }

    return dice_rng_next(stream);
}

int DICE_RandInt(uint8_t handle, int minimum, int maximum) {
    if (!dice_rng_valid(handle)) {
        return minimum;
    }

    if (maximum < minimum) {
        int saved_bound = minimum;
        minimum = maximum;
        maximum = saved_bound;
    }

    uint32_t range = (uint32_t)((int64_t)maximum - (int64_t)minimum) + 1u;
    if (range == 0u) {

        uint32_t draw = DICE_RandUint(handle);
        return draw <= INT32_MAX ? (int)draw : (int)((int64_t)draw - INT64_C(4294967296));
    }

    uint32_t threshold = (UINT32_C(0) - range) % range;
    uint32_t draw;
    do {
        draw = DICE_RandUint(handle);
    } while (draw < threshold);

    return (int)((int64_t)minimum + (int64_t)(draw % range));
}

float DICE_RandFloat(uint8_t handle) {
    if (!dice_rng_valid(handle)) {
        return 0.0f;
    }

    return (float)(DICE_RandUint(handle) >> 8) * (1.0f / 16777216.0f);
}

bool DICE_RandChance(uint8_t handle, float probability) {
    if (probability <= 0.0f) {
        return false;
    }
    if (probability >= 1.0f) {
        return true;
    }

    return DICE_RandFloat(handle) < probability;
}

bool dice_rng_is_initialized(uint8_t handle) {
    return dice_rng_valid(handle);
}

bool dice_rng_has_resources(void) {
    for (uint8_t handle = 0; handle < DICE_MAX_RNG; ++handle)
        if (dice_rng_valid(handle)) return true;
    return false;
}
