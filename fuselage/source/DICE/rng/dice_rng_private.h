#ifndef DICE_RNG_PRIVATE_H
#define DICE_RNG_PRIVATE_H
#include <stdint.h>
#include <stdbool.h>
// Best-effort entropy with a clock fallback; not a security guarantee.
uint64_t dice_rng_entropy_read(void);
// Validate shared die/RNG lifetime without exposing stream state publicly.
bool dice_rng_is_initialized(uint8_t handle);
bool dice_rng_has_resources(void);
#endif
