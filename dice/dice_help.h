#ifndef DICE_HELP_H
#define DICE_HELP_H

// dice_help.h - Convenience layer over DICE RNG (dice_rng.h). Two distinct
// concepts, deliberately not unified into one API despite both being
// "random numbers", because they answer two different questions:
//
//   Dice      - "give me the next random number from this stream." Order
//                matters, history doesn't need to be reproducible out of
//                order. Backed directly by a DICE_RNG stream -- die N IS
//                DICE_RNG handle N, nothing else to release/manage; DICE
//                RNG's own DICE_MAX_RNG-sized teardown in DICE_Shutdown
//                already covers every die.
//
//   Sequences - "give me the value at position N," reproducibly, for any N,
//                in any order, without having visited positions before it
//                or stored anything from an earlier pass. Built for
//                procedural generation keyed by a stable position/
//                coordinate (world terrain, loot tables, anything a player
//                can walk away from and back to and expect unchanged)
//                where the whole point is NOT needing history. Stateless --
//                a sequence is just a seed with a small integer name, not a
//                stream, and holds no DICE_RNG resource at all.
//
// Both are addressed by a small integer handle (not a string name), on
// purpose: this needs to stay trivially callable from VPU ecalls once that
// lands, where a uint8_t argument is cheap and a string never is. Same
// isolation rule as the rest of DICE (see dice.h): no awareness of
// Fuselage, GDMF, or CAKE -- this only ever talks to dice_rng.h.

#include "dice_rng.h"

// Dice
// Auto-seeded (DICE_RNG_ENTROPY_SEEDED) on first use if DICE_SeedDie() was
// never called for that handle -- a die always gives you a usable number
// without setup; call DICE_SeedDie() first only when you actually need a
// specific, reproducible sequence of rolls.

#define DICE_MAX_DICE DICE_MAX_RNG

bool  DICE_SeedDie(uint8_t die, uint64_t seed);
int   DICE_DieRange(uint8_t die, int min, int max);
float DICE_DieFloat(uint8_t die);
bool  DICE_DieChance(uint8_t die, float probability);

// Sequences
// A sequence's seed defaults to its own handle value (not 0) until
// DICE_SeedSequence() is called, so two never-configured sequences still
// don't collide into identical output -- but real procedural-generation use
// is expected to always set a real seed explicitly (that's the entire
// point: control and reproduce it deliberately, e.g. a per-world seed), not
// lean on this fallback.

#define DICE_MAX_SEQUENCES 32

bool     DICE_SeedSequence(uint8_t seq, uint64_t seed);
uint32_t DICE_SequenceUint(uint8_t seq, uint64_t position);
int      DICE_SequenceInt(uint8_t seq, uint64_t position, int min, int max);
float    DICE_SequenceFloat(uint8_t seq, uint64_t position);
bool     DICE_SequenceChance(uint8_t seq, uint64_t position, float probability);

#endif // DICE_HELP_H