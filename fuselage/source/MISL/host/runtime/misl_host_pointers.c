#include "misl_host_pointers_internal.h"
#include "misl_host_state_internal.h"

const void* fuselage_guest_resolve_input(uint32_t address, uint32_t byte_count) {
    if (!fuselage_guest_session.cpu.mem) {
        return NULL;
    }
    return VIVIAN_ResolveMemory(&fuselage_guest_session.cpu, address, byte_count);
}

void* fuselage_guest_resolve_output(uint32_t address, uint32_t capacity) {
    if (!capacity || !fuselage_guest_session.cpu.mem) {
        return NULL;
    }
    void* output = VIVIAN_ResolveWritableMemory(&fuselage_guest_session.cpu, address, capacity);
    if (output) {
        /* Current dirty watchers consume the range after the guest slice. */
        VIVIAN_NotifyStore(&fuselage_guest_session.cpu, address, capacity);
    }
    return output;
}

unsigned char* fuselage_guest_resolve_batch_output(uint32_t address, uint32_t byte_count) {
    if (!byte_count || !fuselage_guest_session.cpu.mem ||
        (uint64_t)address + byte_count > fuselage_guest_session.cpu.memSize) {
        return NULL;
    }
    return fuselage_guest_session.cpu.mem + address;
}

int fuselage_guest_batch_ranges_valid(uint32_t input_address, uint32_t input_bytes, uint32_t output_address,
                                      uint32_t output_bytes) {
    if (input_address & 1u) {
        return 0;
    }
    return !((uint64_t)input_address < (uint64_t)output_address + output_bytes &&
             (uint64_t)output_address < (uint64_t)input_address + input_bytes);
}

MislHostStringRead fuselage_guest_copy_string(uint32_t address, char* output, uint32_t capacity) {
    MislHostStringRead result = {0, MISL_HOST_STRING_ARGUMENT};
    if (!output || !capacity) {
        return result;
    }
    output[0] = '\0';
    for (;;) {
        uint64_t current_address = (uint64_t)address + result.byte_count;
        const uint8_t* input =
            current_address <= UINT32_MAX ? fuselage_guest_resolve_input((uint32_t)current_address, 1) : NULL;
        if (!input) {
            result.status = MISL_HOST_STRING_UNMAPPED;
            return result;
        }
        if (!*input) {
            result.status = MISL_HOST_STRING_COMPLETE;
            return result;
        }
        if (result.byte_count == capacity - 1) {
            result.status = MISL_HOST_STRING_TRUNCATED;
            return result;
        }
        output[result.byte_count++] = (char)*input;
        output[result.byte_count] = '\0';
    }
}
