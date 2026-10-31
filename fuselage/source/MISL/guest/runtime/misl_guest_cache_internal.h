#pragma once
#include "../../abi/misl_mmio_map.h"

// One cache per guest, defined by misl_guest_cache.c and shared across C files.
// Assembly ROM thunks use the region-base array too.
// SEE: (MISL_RUNTIME.md, Shared discovery cache)
extern const volatile MislDescriptor* misl_guest_descriptor;
extern uint32_t misl_guest_region_bases[MISL_DESC_MAX_REGIONS];
