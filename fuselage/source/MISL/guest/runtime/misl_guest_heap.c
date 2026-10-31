#include "misl_guest_cache_internal.h"
#include "misl_guest_heap.h"
#include <stddef.h>

// Weak entry points permit an application-provided heap policy.
// SEE: (MISL_RUNTIME.md, Heap break)
#ifndef MISL_RT
#define MISL_RT __attribute__((weak))
#endif

// The guest linker defines the first available, eight-byte-aligned address.
extern char _end[];

MISL_RT void* _sbrk(long increment) {
    static uintptr_t current_break;
    const uintptr_t heap_floor = (uintptr_t)_end;
    if (!current_break) {
        current_break = heap_floor;
    }

    uint32_t program_bytes = MISL_MEM_SIZE;
    const volatile MislDescriptor* descriptor = misl_guest_descriptor;
    if (descriptor) {
        const uint32_t region_count = descriptor->regionCount;
        if (region_count > MISL_DESC_MAX_REGIONS ||
            descriptor->descBytes < offsetof(MislDescriptor, region) + region_count * sizeof(MislRegion)) {
            return (void*)-1;
        }
        for (uint32_t region_index = 0; region_index < region_count; ++region_index) {
            if (descriptor->region[region_index].id == MISL_REGION_PROGRAM) {
                program_bytes = descriptor->region[region_index].size;
                break;
            }
        }
    }

    if (program_bytes < MISL_STACK_RESERVE) {
        return (void*)-1;
    }
    const uintptr_t heap_ceiling = (uintptr_t)(program_bytes - MISL_STACK_RESERVE);
    if (current_break < heap_floor || current_break > heap_ceiling) {
        return (void*)-1;
    }

    uintptr_t next_break;
    if (increment >= 0) {
        const uintptr_t requested_bytes = (uintptr_t)increment;
        if (requested_bytes > UINTPTR_MAX - 7u) {
            return (void*)-1;
        }
        const uintptr_t aligned_bytes = (requested_bytes + 7u) & ~(uintptr_t)7u;
        if (aligned_bytes > heap_ceiling - current_break) {
            return (void*)-1;
        }
        next_break = current_break + aligned_bytes;
    } else {
        // Avoid negating LONG_MIN. Negative requests round toward positive infinity.
        const uintptr_t requested_bytes = (uintptr_t)(-(increment + 1L)) + 1u;
        const uintptr_t aligned_bytes = requested_bytes & ~(uintptr_t)7u;
        if (aligned_bytes > current_break - heap_floor) {
            return (void*)-1;
        }
        next_break = current_break - aligned_bytes;
    }

    const uintptr_t previous_break = current_break;
    current_break = next_break;
    return (void*)previous_break;
}

// Call the selected _sbrk definition, including a strong application override.
MISL_RT void* sbrk(long increment) {
    return _sbrk(increment);
}
