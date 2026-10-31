/* SEE: (VIVIAN.md, Instruction fetch) */
#ifndef VIVIAN_FETCH_INTERNAL_H
#define VIVIAN_FETCH_INTERNAL_H
#include "../memory/vivian_access_internal.h"
#include "../compressed/vivian_compressed_internal.h"
#include <stddef.h>

static inline VivianStatus vivian_fetch_instruction(const VivianState* state, uint32_t* instruction,
                                                    uint32_t* instruction_size) {
    if (state->pc % 2 != 0) {
        return VIVIAN_MISALIGNED_FETCH;
    }

    uint32_t decoded_instruction;
    uint32_t decoded_size;
    const uint8_t* instruction_bytes = NULL;
    if ((uint64_t)state->pc + 4u <= (uint64_t)state->memSize) {
        instruction_bytes = state->mem + state->pc;
    } else {
        for (uint32_t slot = 0; slot < VIVIAN_READ_ONLY_SPAN_COUNT; slot++) {
            if (state->rom[slot].ptr && state->pc >= state->rom[slot].base &&
                (uint64_t)state->pc + 4u <= (uint64_t)state->rom[slot].base + state->rom[slot].size) {
                instruction_bytes = state->rom[slot].ptr + (state->pc - state->rom[slot].base);
                break;
            }
        }
    }
    if (instruction_bytes) {
        uint32_t instruction_word = (uint32_t)instruction_bytes[0] | ((uint32_t)instruction_bytes[1] << 8) |
                                    ((uint32_t)instruction_bytes[2] << 16) |
                                    ((uint32_t)instruction_bytes[3] << 24);
        if ((instruction_word & 3u) != 3u) {
            if (!vivian_expand_compressed_instruction((uint16_t)instruction_word, &decoded_instruction)) {
                return VIVIAN_ILLEGAL_INSTRUCTION;
            }
            decoded_size = 2;
        } else {
            decoded_instruction = instruction_word;
            decoded_size = 4;
        }
    } else {
        uint16_t first_halfword;
        if (!vivian_load_halfword(state, state->pc, &first_halfword)) {
            return VIVIAN_BUS_ERROR;
        }
        if ((first_halfword & 3u) != 3u) {
            if (!vivian_expand_compressed_instruction(first_halfword, &decoded_instruction)) {
                return VIVIAN_ILLEGAL_INSTRUCTION;
            }
            decoded_size = 2;
        } else {
            if (!vivian_load_word(state, state->pc, &decoded_instruction)) {
                return VIVIAN_BUS_ERROR;
            }
            decoded_size = 4;
        }
    }

    *instruction = decoded_instruction;
    *instruction_size = decoded_size;
    return VIVIAN_OK;
}
#endif
