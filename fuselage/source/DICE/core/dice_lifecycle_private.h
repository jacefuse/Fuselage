#ifndef DICE_LIFECYCLE_PRIVATE_H
#define DICE_LIFECYCLE_PRIVATE_H
#include <stdbool.h>
// Orchestrator service. False retains remaining owners and the RNG/clock lifetime.
bool dice_shutdown_checked(void);
#endif
