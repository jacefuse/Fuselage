#ifndef DICE_H
#define DICE_H

#include <stdbool.h>

// DICE - Timer, RNG (and eventually FUZZ)
//
// A self-contained system, structurally similar to CAKE: DICE has no
// awareness of Fuselage, GDMF, CAKE, or any other subsystem, and none of
// those systems know how DICE works internally. Everything DICE exposes is
// consumed by handle, exactly like the rest of the engine's subsystems;
// init a component, then use it.
//
// Split across files by concern (dice_timers.*, dice_rng.*, dice_help.*,
// and later dice_fuzz.*), but every public symbol carries the DICE_
// prefix regardless of which file it lives in.

#define DICE_VERSION "0.3.2026070401 COLON"

// One-time setup/teardown for whatever DICE needs internally shared across
// timers and RNG (clock calibration, entropy pool bootstrap). Individual
// timers and RNG streams still need their own DICE_InitTimer/DICE_InitRNG
// calls. Init only prepares DICE itself to be used; it returns true on
// success, false if that internal setup fails.
bool DICE_Init(void);
void DICE_Shutdown(void);

#include "dice_timers.h"
#include "dice_rng.h"
#include "dice_help.h"

#endif // DICE_H