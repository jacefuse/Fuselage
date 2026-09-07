// dicetest.c - DICE RNG cross-platform determinism harness.
//
// A DETERMINISTIC stream is a contract: the same seed must produce the
// byte-identical sequence on every platform Fuselage runs on, forever --
// SeedSpace worlds (InfiniteWorldWalker and everything built on its guts)
// are reconstructed from nothing but a seed, so a single differing draw
// means a different world. This harness pins that contract with golden
// vectors: sequences generated once, checked in below, and verified by
// every platform that builds the engine.
//
//   dicetest            verify against the embedded golden vectors + run
//                       structural checks; exit 0 on PASS, 1 on any FAIL
//   dicetest --dump     print the current platform's values as the C golden
//                       block below (for regenerating after a DELIBERATE
//                       algorithm change -- which is a compatibility break
//                       for every seed in the wild, so think twice)
//
// Build (needs only the RNG, not the engine):
//   cc -std=c11 -Wall -Wextra -o dicetest dicetest.c dice_rng.c        (mac/linux)
//   clang -std=c11 -Wall -Wextra -o dicetest.exe dicetest.c dice_rng.c -lbcrypt   (windows)
// Or `make dicetest` at the engine root.
//
// Coverage: raw draws across seeds x handles (the handle is PCG's stream
// selector -- part of the contract), RandInt range reduction including the
// full-int-range and degenerate cases, RandFloat compared as BIT PATTERNS
// (float equality is exactly the kind of thing that drifts silently),
// MixRNGEntropy's deterministic fold, ACCUMULATE-mode seeding, and replay
// (release + re-init same seed = same sequence).

#include "dice_rng.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

// --- Test shape --------------------------------------------------------------

#define SEQ_SEED_COUNT   5
#define SEQ_HANDLE_COUNT 3
#define SEQ_DRAWS        8

static const uint64_t seq_seeds[SEQ_SEED_COUNT] = {
    0x0000000000000000ULL,   // zero seed is legal and must be stable too
    0x0000000000000001ULL,
    0x00000000DEADBEEFULL,
    0x123456789ABCDEF0ULL,
    0xFFFFFFFFFFFFFFFFULL,
};

static const uint8_t seq_handles[SEQ_HANDLE_COUNT] = { 0, 7, 31 };

#define RI_SMALL_DRAWS  8   // RandInt(0, 9)
#define RI_SIGNED_DRAWS 8   // RandInt(-100, 100)
#define RI_FULL_DRAWS   4   // RandInt(INT_MIN, INT_MAX) -- the range==0 path
#define RI_SWAP_DRAWS   4   // RandInt(10, -10) -- swapped bounds normalize
#define RF_DRAWS        8   // RandFloat, compared as bit patterns
#define MIX_DRAWS       4   // draws after a deterministic MixRNGEntropy
#define ACC_DRAWS       2   // ACCUMULATE mode seeds verbatim, no entropy

typedef struct {
    uint32_t sequences[SEQ_SEED_COUNT][SEQ_HANDLE_COUNT][SEQ_DRAWS];
    int32_t  ri_small[RI_SMALL_DRAWS];
    int32_t  ri_signed[RI_SIGNED_DRAWS];
    int32_t  ri_full[RI_FULL_DRAWS];
    int32_t  ri_swap[RI_SWAP_DRAWS];
    uint32_t rf_bits[RF_DRAWS];
    uint32_t mix_after[MIX_DRAWS];
    uint32_t acc[ACC_DRAWS];
} DiceTestResults;

// Fills every result the goldens pin, using only DETERMINISTIC/ACCUMULATE
// streams -- the modes that never touch OS entropy.
static void fill_results(DiceTestResults* r) {
    // Raw sequences: every seed x every handle. The handle matters -- it is
    // PCG's stream selector, so handle 7 seeded with X is a different (but
    // equally pinned) sequence than handle 0 seeded with X.
    for (int si = 0; si < SEQ_SEED_COUNT; si++) {
        for (int hi = 0; hi < SEQ_HANDLE_COUNT; hi++) {
            uint8_t h = seq_handles[hi];
            DICE_InitRNG(h, DICE_RNG_DETERMINISTIC, seq_seeds[si]);
            for (int d = 0; d < SEQ_DRAWS; d++) {
                r->sequences[si][hi][d] = DICE_RandUint(h);
            }
            DICE_ReleaseRNG(h);
        }
    }

    // Range reduction, all paths: small, signed, full-int-range (range==0
    // shortcut), and swapped bounds.
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0xC0FFEEULL);
    for (int d = 0; d < RI_SMALL_DRAWS; d++)  { r->ri_small[d]  = DICE_RandInt(0, 0, 9); }
    for (int d = 0; d < RI_SIGNED_DRAWS; d++) { r->ri_signed[d] = DICE_RandInt(0, -100, 100); }
    for (int d = 0; d < RI_FULL_DRAWS; d++)   { r->ri_full[d]   = DICE_RandInt(0, INT_MIN, INT_MAX); }
    for (int d = 0; d < RI_SWAP_DRAWS; d++)   { r->ri_swap[d]   = DICE_RandInt(0, 10, -10); }
    DICE_ReleaseRNG(0);

    // Floats, pinned as bit patterns.
    DICE_InitRNG(3, DICE_RNG_DETERMINISTIC, 0x5EEDULL);
    for (int d = 0; d < RF_DRAWS; d++) {
        float f = DICE_RandFloat(3);
        memcpy(&r->rf_bits[d], &f, sizeof(f));
    }
    DICE_ReleaseRNG(3);

    // MixRNGEntropy with fixed bytes is itself deterministic.
    DICE_InitRNG(1, DICE_RNG_DETERMINISTIC, 0x77ULL);
    (void)DICE_RandUint(1);
    (void)DICE_RandUint(1);
    DICE_MixRNGEntropy(1, "SeedSpace", 9);
    for (int d = 0; d < MIX_DRAWS; d++) { r->mix_after[d] = DICE_RandUint(1); }
    DICE_ReleaseRNG(1);

    // ACCUMULATE mode: seed used verbatim, no entropy at init -- so its
    // untouched output is pinned exactly like DETERMINISTIC.
    DICE_InitRNG(2, DICE_RNG_ACCUMULATE, 0xABULL);
    for (int d = 0; d < ACC_DRAWS; d++) { r->acc[d] = DICE_RandUint(2); }
    DICE_ReleaseRNG(2);

    return;
}

// --- GOLDEN VECTORS (paste of `dicetest --dump` output) ----------------------

static const DiceTestResults golden = {
    .sequences = {
        { // seed 0x0000000000000000
            { 0xE4C14788u, 0x379C6516u, 0x5C4AB3BBu, 0x601D23E0u, 0x1C382B8Cu, 0xD1FAAB16u, 0x67680A2Du, 0x92014A6Eu },  // handle 0
            { 0x2CCD4594u, 0xC426752Bu, 0xA955F79Fu, 0xCB38A12Fu, 0xAB04D9F4u, 0xD8463976u, 0x62C7B9FDu, 0x37990536u },  // handle 7
            { 0xCE0BC70Au, 0xE853B1BDu, 0x638F2670u, 0x7249CE26u, 0xA65CEF83u, 0x99682333u, 0x0545CA8Du, 0x4CA9F68Bu },  // handle 31
        },
        { // seed 0x0000000000000001
            { 0xE2393051u, 0x01112F35u, 0xD3509D35u, 0x0B932F4Au, 0x8AA46776u, 0x8C532036u, 0xA0CD21D8u, 0xB8E6A8D0u },  // handle 0
            { 0x7263A3ECu, 0x7A3B1163u, 0xAB7A4FF0u, 0x6A9358FAu, 0x04004DE3u, 0x96DF987Du, 0x06F9F757u, 0x0AD00A04u },  // handle 7
            { 0x23E87263u, 0x885513A0u, 0xC4CC0FF3u, 0x8B34B5FEu, 0x24A89CD5u, 0x6E711B3Bu, 0x1F583C54u, 0x796FCAEDu },  // handle 31
        },
        { // seed 0x00000000DEADBEEF
            { 0x14D45B8Bu, 0xEC21C400u, 0x1426A5CFu, 0x6EF02C0Au, 0xBE3B319Au, 0xB8867E12u, 0xCB0B2F0Fu, 0xEBBC7179u },  // handle 0
            { 0x19BF3FE4u, 0x3B4E25B8u, 0x6C32198Au, 0x05715932u, 0xFD5E44DFu, 0x1C2D7A89u, 0xADCE1A8Fu, 0x6182DFF4u },  // handle 7
            { 0xCC6EE022u, 0x4B91878Bu, 0x5228CA30u, 0x5617196Bu, 0x43E6399Bu, 0x2BFB6268u, 0xC36D7476u, 0xC4DBEE00u },  // handle 31
        },
        { // seed 0x123456789ABCDEF0
            { 0x5A6FF4ECu, 0x16212336u, 0x768750EDu, 0xC9A932CFu, 0xADFDE520u, 0xB7BBBEC8u, 0xDDCA2038u, 0x3635F595u },  // handle 0
            { 0x11905EAAu, 0x20B66266u, 0x145F726Eu, 0x801BDF1Fu, 0xE0519863u, 0x50667F15u, 0xB5CBBA45u, 0x514AABBFu },  // handle 7
            { 0x84A0623Eu, 0xB60CB080u, 0xABDADE9Au, 0x23D13791u, 0x791B4C90u, 0xB8D11B99u, 0xF455B0F4u, 0x7270D1E9u },  // handle 31
        },
        { // seed 0xFFFFFFFFFFFFFFFF
            { 0x00000000u, 0xE4C14788u, 0x379C6516u, 0x5C4AB3BBu, 0x601D23E0u, 0x1C382B8Cu, 0xD1FAAB16u, 0x67680A2Du },  // handle 0
            { 0xDB7B2723u, 0x0F3E04F4u, 0x4A7B1F29u, 0x8DF76DE2u, 0x2E1D2830u, 0x92D15361u, 0x9BBDE4F3u, 0x33959E9Fu },  // handle 7
            { 0x1D87B274u, 0x6A41D6D7u, 0x7619A311u, 0x9EF87E6Fu, 0x94B14723u, 0x15CD07D9u, 0xF48AEC61u, 0x91E81DD6u },  // handle 31
        },
    },
    .ri_small  = { 3, 6, 7, 6, 7, 2, 3, 3 },
    .ri_signed = { 22, 84, 59, 78, -26, -38, 16, 24 },
    .ri_full   = { -1065702404, -1706561317, 1380630745, -459384733 },
    .ri_swap   = { 2, 2, -9, -6 },
    .rf_bits   = { 0x3EAD7152u, 0x3F59A6F8u, 0x3C69E480u, 0x3E284668u, 0x3EA019D4u, 0x3F27A0EBu, 0x3ECBCEA0u, 0x3F36A738u },
    .mix_after = { 0x35D45700u, 0xE07EBB7Cu, 0x5EBAC337u, 0x28FE2544u },
    .acc       = { 0xFC4C5415u, 0xA5CED4AAu },
};

// --- END GOLDEN --------------------------------------------------------------

static void dump_results(const DiceTestResults* r) {
    printf("static const DiceTestResults golden = {\n    .sequences = {\n");
    for (int si = 0; si < SEQ_SEED_COUNT; si++) {
        printf("        { // seed 0x%016llX\n", (unsigned long long)seq_seeds[si]);
        for (int hi = 0; hi < SEQ_HANDLE_COUNT; hi++) {
            printf("            { ");
            for (int d = 0; d < SEQ_DRAWS; d++) {
                printf("0x%08Xu%s", r->sequences[si][hi][d], d + 1 < SEQ_DRAWS ? ", " : "");
            }
            printf(" },  // handle %u\n", seq_handles[hi]);
        }
        printf("        },\n");
    }
    printf("    },\n    .ri_small  = { ");
    for (int d = 0; d < RI_SMALL_DRAWS; d++)  { printf("%d%s", r->ri_small[d],  d + 1 < RI_SMALL_DRAWS ? ", " : ""); }
    printf(" },\n    .ri_signed = { ");
    for (int d = 0; d < RI_SIGNED_DRAWS; d++) { printf("%d%s", r->ri_signed[d], d + 1 < RI_SIGNED_DRAWS ? ", " : ""); }
    printf(" },\n    .ri_full   = { ");
    for (int d = 0; d < RI_FULL_DRAWS; d++)   { printf("%d%s", r->ri_full[d],   d + 1 < RI_FULL_DRAWS ? ", " : ""); }
    printf(" },\n    .ri_swap   = { ");
    for (int d = 0; d < RI_SWAP_DRAWS; d++)   { printf("%d%s", r->ri_swap[d],   d + 1 < RI_SWAP_DRAWS ? ", " : ""); }
    printf(" },\n    .rf_bits   = { ");
    for (int d = 0; d < RF_DRAWS; d++)        { printf("0x%08Xu%s", r->rf_bits[d], d + 1 < RF_DRAWS ? ", " : ""); }
    printf(" },\n    .mix_after = { ");
    for (int d = 0; d < MIX_DRAWS; d++)       { printf("0x%08Xu%s", r->mix_after[d], d + 1 < MIX_DRAWS ? ", " : ""); }
    printf(" },\n    .acc       = { ");
    for (int d = 0; d < ACC_DRAWS; d++)       { printf("0x%08Xu%s", r->acc[d], d + 1 < ACC_DRAWS ? ", " : ""); }
    printf(" },\n};\n");

    return;
}

int main(int argc, char** argv) {
    DiceTestResults r;
    memset(&r, 0, sizeof(r));
    fill_results(&r);

    if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
        dump_results(&r);
        return 0;
    }

    int fails = 0;

    // Golden comparison -- one memcmp would do, but per-section reporting
    // says WHERE the platforms disagree, which is the whole point.
    for (int si = 0; si < SEQ_SEED_COUNT; si++) {
        for (int hi = 0; hi < SEQ_HANDLE_COUNT; hi++) {
            if (memcmp(r.sequences[si][hi], golden.sequences[si][hi],
                       sizeof(r.sequences[si][hi])) != 0) {
                printf("FAIL sequence seed 0x%016llX handle %u (first: got 0x%08X want 0x%08X)\n",
                       (unsigned long long)seq_seeds[si], seq_handles[hi],
                       r.sequences[si][hi][0], golden.sequences[si][hi][0]);
                fails++;
            }
        }
    }
    if (memcmp(r.ri_small,  golden.ri_small,  sizeof(r.ri_small))  != 0) { printf("FAIL RandInt(0,9)\n");            fails++; }
    if (memcmp(r.ri_signed, golden.ri_signed, sizeof(r.ri_signed)) != 0) { printf("FAIL RandInt(-100,100)\n");       fails++; }
    if (memcmp(r.ri_full,   golden.ri_full,   sizeof(r.ri_full))   != 0) { printf("FAIL RandInt(INT_MIN,INT_MAX)\n"); fails++; }
    if (memcmp(r.ri_swap,   golden.ri_swap,   sizeof(r.ri_swap))   != 0) { printf("FAIL RandInt(10,-10)\n");         fails++; }
    if (memcmp(r.rf_bits,   golden.rf_bits,   sizeof(r.rf_bits))   != 0) { printf("FAIL RandFloat bit patterns\n");  fails++; }
    if (memcmp(r.mix_after, golden.mix_after, sizeof(r.mix_after)) != 0) { printf("FAIL MixRNGEntropy fold\n");      fails++; }
    if (memcmp(r.acc,       golden.acc,       sizeof(r.acc))       != 0) { printf("FAIL ACCUMULATE seeding\n");      fails++; }

    if (fails == 0) { printf("PASS golden vectors (%d sequences + range/float/mix/accumulate)\n",
                              SEQ_SEED_COUNT * SEQ_HANDLE_COUNT); }

    // Structural checks -- properties, not vectors.

    // Same seed, different handle -> different sequence (the stream
    // selector is doing its job).
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0xFACEULL);
    DICE_InitRNG(1, DICE_RNG_DETERMINISTIC, 0xFACEULL);
    if (DICE_RandUint(0) == DICE_RandUint(1)) { printf("FAIL handle decorrelation\n"); fails++; }
    else                                      { printf("PASS handle decorrelation\n"); }
    DICE_ReleaseRNG(0); DICE_ReleaseRNG(1);

    // Replay: release + re-init with the same seed = the same sequence.
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0xB0A7ULL);
    uint32_t first[4];
    for (int d = 0; d < 4; d++) { first[d] = DICE_RandUint(0); }
    DICE_ReleaseRNG(0);
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0xB0A7ULL);
    bool replay_ok = true;
    for (int d = 0; d < 4; d++) { if (DICE_RandUint(0) != first[d]) { replay_ok = false; } }
    DICE_ReleaseRNG(0);
    printf("%s replay (release + re-init same seed)\n", replay_ok ? "PASS" : "FAIL");
    if (!replay_ok) { fails++; }

    // Bounds: RandInt never leaves [min, max] (1000 draws on an awkward range).
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0x1234ULL);
    bool bounds_ok = true;
    for (int d = 0; d < 1000; d++) {
        int v = DICE_RandInt(0, -7, 13);
        if (v < -7 || v > 13) { bounds_ok = false; }
    }
    DICE_ReleaseRNG(0);
    printf("%s RandInt bounds\n", bounds_ok ? "PASS" : "FAIL");
    if (!bounds_ok) { fails++; }

    // RandFloat stays in [0, 1) (1000 draws).
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0x4321ULL);
    bool float_ok = true;
    for (int d = 0; d < 1000; d++) {
        float f = DICE_RandFloat(0);
        if (f < 0.0f || f >= 1.0f) { float_ok = false; }
    }
    DICE_ReleaseRNG(0);
    printf("%s RandFloat range\n", float_ok ? "PASS" : "FAIL");
    if (!float_ok) { fails++; }

    // GetRNGSeed hands back the verbatim seed.
    DICE_InitRNG(0, DICE_RNG_DETERMINISTIC, 0xABCDEF0123456789ULL);
    bool seed_ok = (DICE_GetRNGSeed(0) == 0xABCDEF0123456789ULL);
    DICE_ReleaseRNG(0);
    printf("%s GetRNGSeed identity\n", seed_ok ? "PASS" : "FAIL");
    if (!seed_ok) { fails++; }

    printf(fails == 0 ? "ALL PASS\n" : "FAILURES: %d\n", fails);

    return fails == 0 ? 0 : 1;
}
