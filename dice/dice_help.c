// dice_help.c - see dice_help.h for the Dice/Sequence split and why they're
// not the same API.

#include "dice_help.h"

// ---------------------------------------------------------------------
// Dice
// ---------------------------------------------------------------------

static bool g_die_ready[DICE_MAX_DICE];

static void dice_help_ensure_seeded(uint8_t die) {
    if (die >= DICE_MAX_DICE || g_die_ready[die]) { return; }

    DICE_InitRNG(die, DICE_RNG_ENTROPY_SEEDED, 0);
    g_die_ready[die] = true;

    return;
}

bool DICE_SeedDie(uint8_t die, uint64_t seed) {
    if (die >= DICE_MAX_DICE) { return false; }

    bool ok = DICE_InitRNG(die, DICE_RNG_DETERMINISTIC, seed);
    g_die_ready[die] = ok;

    return ok;
}

int DICE_DieRange(uint8_t die, int min, int max) {
    dice_help_ensure_seeded(die);

    return DICE_RandInt(die, min, max);
}

float DICE_DieFloat(uint8_t die) {
    dice_help_ensure_seeded(die);

    return DICE_RandFloat(die);
}

bool DICE_DieChance(uint8_t die, float probability) {
    dice_help_ensure_seeded(die);

    return DICE_RandChance(die, probability);
}

// ---------------------------------------------------------------------
// Sequences
// ---------------------------------------------------------------------

typedef struct {
    bool     configured;
    uint64_t seed;
} DiceSequence;

static DiceSequence g_sequences[DICE_MAX_SEQUENCES];

// splitmix64's own finalizer (Steele/Vigna) used here purely as a stateless
// avalanche step, not as a running generator -- pure fixed-width unsigned
// arithmetic (wraparound is well-defined, no signed overflow, no rotate
// whose behavior could vary by platform), which is the actual property that
// makes a sequence's output identical across platforms. Nothing here reuses
// DICE_RNG's PCG32 state machine on purpose: PCG32 only defines "the next
// value after this one", which is exactly the sequential dependency a
// position-addressable sequence can't have.
static uint64_t dice_seq_mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;

    return x ^ (x >> 31);
}

// Combines a sequence's seed with a query position into one deterministic
// 64-bit value. Position is avalanched on its own first (so neighboring
// positions decorrelate immediately, not just "eventually" the way a
// stream's next-value step would), then the seed is folded in and the
// result avalanched again (so different seeds decorrelate from each other
// too, not just offset by a constant).
static uint64_t dice_seq_hash(uint64_t seed, uint64_t position) {
    uint64_t h = dice_seq_mix(position);
    h ^= seed;

    return dice_seq_mix(h);
}

static uint64_t dice_seq_effective_seed(uint8_t seq) {
    if (g_sequences[seq].configured) { return g_sequences[seq].seed; }

    // Unconfigured default: the handle's own value, not 0 -- keeps two
    // never-seeded sequences from producing byte-identical output, without
    // pretending this is a substitute for actually calling
    // DICE_SeedSequence() when reproducibility is the point.
    return (uint64_t)seq;
}

bool DICE_SeedSequence(uint8_t seq, uint64_t seed) {
    if (seq >= DICE_MAX_SEQUENCES) { return false; }

    g_sequences[seq].seed       = seed;
    g_sequences[seq].configured = true;

    return true;
}

uint32_t DICE_SequenceUint(uint8_t seq, uint64_t position) {
    if (seq >= DICE_MAX_SEQUENCES) { return 0; }

    uint64_t h = dice_seq_hash(dice_seq_effective_seed(seq), position);

    return (uint32_t)(h >> 32);
}

int DICE_SequenceInt(uint8_t seq, uint64_t position, int min, int max) {
    if (seq >= DICE_MAX_SEQUENCES) { return min; }

    if (max < min) { int t = min; min = max; max = t; }

    uint32_t range = (uint32_t)((int64_t)max - (int64_t)min) + 1u;
    if (range == 0u) {
        // min == INT_MIN, max == INT_MAX: every uint32 output is valid.
        return (int)DICE_SequenceUint(seq, position);
    }

    // Lemire's single-multiply bounding (Lemire, "Fast Random Integer
    // Generation in an Interval") rather than DICE_RandInt's exact
    // rejection-sampling loop: a sequence's entire point is exactly one
    // hash query per position, always -- a retry loop would need its own
    // deterministic "attempt" input threaded through the hash to stay
    // reproducible, which is more moving parts for a bias that's already
    // bounded by range/2^32 and negligible for any game-sized range.
    uint32_t r      = DICE_SequenceUint(seq, position);
    uint64_t scaled = (uint64_t)r * (uint64_t)range;

    return min + (int)(scaled >> 32);
}

float DICE_SequenceFloat(uint8_t seq, uint64_t position) {
    if (seq >= DICE_MAX_SEQUENCES) { return 0.0f; }

    return (float)(DICE_SequenceUint(seq, position) >> 8) * (1.0f / 16777216.0f);
}

bool DICE_SequenceChance(uint8_t seq, uint64_t position, float probability) {
    if (probability <= 0.0f) { return false; }
    if (probability >= 1.0f) { return true; }

    return DICE_SequenceFloat(seq, position) < probability;
}
