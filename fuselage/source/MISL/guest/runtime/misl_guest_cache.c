#include "misl_guest_cache_internal.h"

// Strong definitions prevent an accidental second cache from linking silently.
const volatile MislDescriptor* misl_guest_descriptor = 0;
uint32_t misl_guest_region_bases[MISL_DESC_MAX_REGIONS];
