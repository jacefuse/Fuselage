#include "../vivian.h"
#include "vivian_compressed_internal.h"

int VIVIAN_ExpandCompressedInstruction(uint16_t instruction, uint32_t* expanded_instruction) {
    return vivian_expand_compressed_instruction(instruction, expanded_instruction);
}
