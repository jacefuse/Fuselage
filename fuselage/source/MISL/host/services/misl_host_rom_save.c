/* Save-write admission; persistence remains behind its private interface. */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "misl_host_rom_internal.h"
#include "../runtime/misl_host_save_internal.h"
#include <stdio.h>

int fuselage_guest_rom_write_save(uint32_t call) {
    uint32_t offset = fuselage_guest_get_argument(0);
    uint32_t address = fuselage_guest_get_argument(1);
    uint32_t byte_count = fuselage_guest_get_argument(2);
    if (!byte_count) {
        fuselage_guest_set_result(0);
        return 1;
    }
    const void* input = fuselage_guest_resolve_input(address, byte_count);
    if (!input) {
        return fuselage_guest_rom_bad_pointer(call, address, byte_count);
    }
    if (!fuselage_guest_write_save(offset, input, byte_count)) {
        char reason[160];
        snprintf(reason, sizeof reason, "Save write of %u bytes at offset %u exceeds the %u-byte region",
                 (unsigned)byte_count, (unsigned)offset, (unsigned)fuselage_guest_get_save_size());
        fprintf(stderr, "[MISL] %s\n", reason);
        fuselage_guest_report_fault(reason);
        return 0;
    }
    fuselage_guest_set_result(1);
    return 1;
}
