// SEE: (DICE.md, Dice helpers) and (DICE.md, Positional sequences)
#ifndef DICE_HELP_H
#define DICE_HELP_H

#include "dice_rng.h"

#define DICE_MAX_DICE DICE_MAX_RNG

// Die N shares RNG stream N. First use auto-seeds unless SeedDie was called.
bool DICE_SeedDie(uint8_t die, uint64_t seed);
int DICE_RollDieRange(uint8_t die, int min, int max);
float DICE_RollDieFloat(uint8_t die);
bool DICE_RollDieChance(uint8_t die, float probability);

#define DICE_MAX_SEQUENCES 32

// Default seed is the handle. Queries do not advance a stream or require DICE_Init.
bool DICE_SeedSequence(uint8_t sequence, uint64_t seed);
uint32_t DICE_GetSequenceUint(uint8_t sequence, uint64_t position);
int DICE_GetSequenceInt(uint8_t sequence, uint64_t position, int min, int max);
float DICE_GetSequenceFloat(uint8_t sequence, uint64_t position);
bool DICE_GetSequenceChance(uint8_t sequence, uint64_t position, float probability);

#endif
