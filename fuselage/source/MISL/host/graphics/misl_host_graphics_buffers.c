/* Copy wire records into aligned native storage before typed access. */
#include "misl_host_graphics_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include <stdlib.h>
#include <string.h>

void* fuselage_guest_copy_records(uint32_t address, uint32_t count, uint32_t record_bytes) {
    uint64_t total = (uint64_t)count * record_bytes;
    if (!total || total > UINT32_MAX || total > SIZE_MAX) {
        return NULL;
    }
    const void* input = fuselage_guest_resolve_input(address, (uint32_t)total);
    if (!input) {
        return NULL;
    }
    void* copy = malloc((size_t)total);
    if (copy) {
        memcpy(copy, input, (size_t)total);
    }
    return copy;
}

void* fuselage_guest_resolve_record_output(uint32_t address, uint32_t count, uint32_t record_bytes) {
    uint64_t total = (uint64_t)count * record_bytes;
    return total && total <= UINT32_MAX ? fuselage_guest_resolve_output(address, (uint32_t)total) : NULL;
}
