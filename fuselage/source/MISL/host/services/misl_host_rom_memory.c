/* Checked BOOTSPUR memory/string operations. SEE: (MISL_HOST_SERVICES.md, ROM) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "misl_host_rom_internal.h"
#include "../../misl_abi.h"
#include "../../format/misl_format.h"
#include <string.h>

static const unsigned char* fuselage_guest_rom_read_byte(uint32_t call, uint64_t address) {
    if (address > UINT32_MAX) {
        fuselage_guest_report_fault("A BOOTSPUR string crossed the guest address space");
        return NULL;
    }
    const unsigned char* input = fuselage_guest_resolve_input((uint32_t)address, 1);
    if (!input) {
        fuselage_guest_rom_bad_pointer(call, (uint32_t)address, 1);
    }
    return input;
}

static int fuselage_guest_rom_strlen(uint32_t call, uint32_t address) {
    uint64_t length = 0;
    for (;;) {
        const unsigned char* input = fuselage_guest_rom_read_byte(call, (uint64_t)address + length);
        if (!input) {
            return 0;
        }
        if (!*input) {
            fuselage_guest_set_result((uint32_t)length);
            return 1;
        }
        length++;
    }
}

int fuselage_guest_rom_memory(uint32_t call) {
    uint32_t destination_address = fuselage_guest_get_argument(0);
    uint32_t byte_count = fuselage_guest_get_argument(2);
    switch (call) {
    case MISL_ROM_MEMCPY:
    case MISL_ROM_MEMMOVE:
        if (byte_count) {
            uint32_t source_address = fuselage_guest_get_argument(1);
            void* output = fuselage_guest_resolve_output(destination_address, byte_count);
            const void* input = fuselage_guest_resolve_input(source_address, byte_count);
            if (!output) {
                return fuselage_guest_rom_bad_pointer(call, destination_address, byte_count);
            }
            if (!input) {
                return fuselage_guest_rom_bad_pointer(call, source_address, byte_count);
            }
            /* Also tolerate overlap for the memcpy entry at the host boundary. */
            memmove(output, input, byte_count);
        }
        fuselage_guest_set_result(destination_address);
        return 1;
    case MISL_ROM_MEMSET:
        if (byte_count) {
            void* output = fuselage_guest_resolve_output(destination_address, byte_count);
            if (!output) {
                return fuselage_guest_rom_bad_pointer(call, destination_address, byte_count);
            }
            memset(output, (int)fuselage_guest_get_argument(1), byte_count);
        }
        fuselage_guest_set_result(destination_address);
        return 1;
    case MISL_ROM_MEMCMP: {
        int comparison = 0;
        if (byte_count) {
            uint32_t second_address = fuselage_guest_get_argument(1);
            const void* first = fuselage_guest_resolve_input(destination_address, byte_count);
            const void* second = fuselage_guest_resolve_input(second_address, byte_count);
            if (!first) {
                return fuselage_guest_rom_bad_pointer(call, destination_address, byte_count);
            }
            if (!second) {
                return fuselage_guest_rom_bad_pointer(call, second_address, byte_count);
            }
            comparison = memcmp(first, second, byte_count);
        }
        fuselage_guest_set_result((uint32_t)(int32_t)((comparison > 0) - (comparison < 0)));
        return 1;
    }
    case MISL_ROM_STRLEN:
        return fuselage_guest_rom_strlen(call, destination_address);
    case MISL_ROM_APPENDSTR: {
        uint32_t capacity = fuselage_guest_get_argument(1);
        uint32_t position = fuselage_guest_get_argument(2);
        uint64_t source_address = fuselage_guest_get_argument(3);
        char* output = fuselage_guest_resolve_output(destination_address, capacity);
        if (!output) {
            return fuselage_guest_rom_bad_pointer(call, destination_address, capacity);
        }
        if (position >= capacity) {
            position = capacity - 1;
        }
        for (;;) {
            char piece[128];
            uint32_t copied = 0;
            int ended = 0;
            while (copied + 1 < sizeof piece) {
                const unsigned char* input = fuselage_guest_rom_read_byte(call, source_address + copied);
                if (!input) {
                    return 0;
                }
                if (!*input) {
                    ended = 1;
                    break;
                }
                piece[copied++] = (char)*input;
            }
            piece[copied] = '\0';
            position = MISL_AppendStr(output, capacity, position, piece);
            if (ended || position >= capacity - 1) {
                break;
            }
            source_address += copied;
        }
        fuselage_guest_set_result(position);
        return 1;
    }
    case MISL_ROM_APPENDINT:
    case MISL_ROM_APPENDFIXED: {
        uint32_t capacity = fuselage_guest_get_argument(1);
        char* output = fuselage_guest_resolve_output(destination_address, capacity);
        if (!output) {
            return fuselage_guest_rom_bad_pointer(call, destination_address, capacity);
        }
        uint32_t decimals = call == MISL_ROM_APPENDFIXED ? fuselage_guest_get_argument(4) : 0;
        fuselage_guest_set_result(MISL_AppendFixed(output, capacity, fuselage_guest_get_argument(2),
                                                   (int)fuselage_guest_get_argument(3), decimals));
        return 1;
    }
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
