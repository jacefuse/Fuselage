/* BOOTSPUR faults and debug tracing. SEE: (MISL_HOST_SERVICES.md, ROM) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "misl_host_rom_internal.h"
#include "../../bootspur/misl_rom.h"
#include "../../../DICE/dice.h"
#include <stdio.h>

int fuselage_guest_rom_bad_pointer(uint32_t call, uint32_t address, uint32_t byte_count) {
    char reason[160];
    snprintf(reason, sizeof reason, "BOOTSPUR entry %u: %u bytes at 0x%08X are not writable or mapped",
             (unsigned)(call - MISL_ROM_ECALL_BASE), (unsigned)byte_count, (unsigned)address);
    fprintf(stderr, "[BOOTSPUR] %s\n", reason);
    fuselage_guest_report_fault(reason);
    return 0;
}

int fuselage_guest_rom_unimplemented(uint32_t call) {
    char reason[160];
    snprintf(reason, sizeof reason, "BOOTSPUR entry %u needs a newer Fuselage",
             (unsigned)(call - MISL_ROM_ECALL_BASE));
    fprintf(stderr, "[BOOTSPUR] %s\n", reason);
    fuselage_guest_report_fault(reason);
    return 0;
}

int fuselage_guest_rom_trace(uint32_t call, uint32_t address) {
    char line[512];
    MislHostStringRead read = fuselage_guest_copy_string(address, line, sizeof line);
    if (read.status == MISL_HOST_STRING_UNMAPPED) {
        uint64_t failed_address = (uint64_t)address + read.byte_count;
        if (failed_address > UINT32_MAX) {
            fuselage_guest_report_fault("A BOOTSPUR trace crossed the guest address space");
            return 0;
        }
        return fuselage_guest_rom_bad_pointer(call, (uint32_t)failed_address, 1);
    }
    uint32_t printed = 0;
#ifndef NDEBUG
    static uint64_t window_start;
    static uint32_t window_count;
    static uint32_t dropped;
    uint64_t now = DICE_GetElapsedMicroseconds();
    if (now - window_start >= 1000000ull) {
        if (dropped) {
            fprintf(stderr, "[MISL] %u trace lines dropped by the rate limit\n", (unsigned)dropped);
        }
        window_start = now;
        window_count = 0;
        dropped = 0;
    }
    if (window_count < 120) {
        fprintf(stderr, "[MISL] %s\n", line);
        window_count++;
        printed = 1;
    } else if (dropped < UINT32_MAX) {
        dropped++;
    }
#endif
    fuselage_guest_set_result(printed);
    return 1;
}
