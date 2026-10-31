/* Host placement limits, not universal ROAP format limits. */
#ifndef MISL_HOST_LIMITS_H
#define MISL_HOST_LIMITS_H
#include "../../abi/misl_mmio_map.h"
#define FUSELAGE_GUEST_MAX_IMAGE_BYTES ((uint32_t)(MISL_SHARED_TOP - MISL_STACK_RESERVE))
#define FUSELAGE_GUEST_MAX_FILE_BYTES ((uint32_t)MISL_SHARED_TOP)
#endif
