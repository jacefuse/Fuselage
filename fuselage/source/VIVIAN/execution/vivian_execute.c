/* SEE: (VIVIAN.md, Execution loop and instruction families) */
#include "vivian_fetch_internal.h"
#include "vivian_registers_internal.h"
#include "../floating/vivian_float_internal.h"
#include "../memory/vivian_memory_internal.h"

/* Faults share an epilogue that counts the stopping attempt once. */

#define VIVIAN_FAULT(fault_status)                                                                           \
    do {                                                                                                     \
        status = (fault_status);                                                                             \
        goto instruction_stopped;                                                                            \
    } while (0)

#define VIVIAN_ARM_FLOAT_FLAGS()                                                                             \
    do {                                                                                                     \
        if (!floating_flags_pending) {                                                                       \
            floating_flags_pending = 1;                                                                      \
            vivian_clear_host_float_flags();                                                                 \
        }                                                                                                    \
    } while (0)

static VivianStatus vivian_run(VivianState* state, uint32_t step_budget, uint32_t* steps_executed) {
    uint32_t attempts = 0;
    VivianStatus status;

    /* Integer-only slices do not touch host floating exception flags. */
    int floating_flags_pending = 0;

next_instruction:
    if (attempts >= step_budget) {
        status = VIVIAN_OK;
        goto execution_complete;
    }

    uint32_t instruction, instruction_size;
    VivianStatus fetch_status = vivian_fetch_instruction(state, &instruction, &instruction_size);
    if (fetch_status != VIVIAN_OK) {
        VIVIAN_FAULT(fetch_status);
    }
    uint32_t opcode = instruction & 0x7F;
    uint32_t destination_register = (instruction >> 7) & 0x1F;
    uint32_t function3 = (instruction >> 12) & 0x7;
    uint32_t source_register1 = (instruction >> 15) & 0x1F;
    uint32_t source_register2 = (instruction >> 20) & 0x1F;
    uint32_t function7 = (instruction >> 25) & 0x7F;

    int32_t immediate_i = (int32_t)instruction >> 20;
    int32_t immediate_s = ((int32_t)(instruction & 0xFE000000) >> 20) | (int32_t)((instruction >> 7) & 0x1F);
    int32_t immediate_b = ((int32_t)(instruction & 0x80000000) >> 19) | (int32_t)((instruction & 0x80) << 4) |
                          (int32_t)((instruction >> 20) & 0x7E0) | (int32_t)((instruction >> 7) & 0x1E);
    int32_t immediate_u = (int32_t)(instruction & 0xFFFFF000);
    int32_t immediate_j = ((int32_t)(instruction & 0x80000000) >> 11) | (int32_t)(instruction & 0xFF000) |
                          (int32_t)((instruction >> 9) & 0x800) | (int32_t)((instruction >> 20) & 0x7FE);

    /* Jump links use the original instruction size, including compressed forms. */
    uint32_t next_pc = state->pc + instruction_size;
    int32_t signed_operand1 = (int32_t)vivian_read_register(state, source_register1);
    int32_t signed_operand2 = (int32_t)vivian_read_register(state, source_register2);

    switch (opcode) {
/* Family fragments share this loop's decoded fields and fault epilogue. */
#include "vivian_instructions_control.inc"

#include "vivian_instructions_memory.inc"

#include "vivian_instructions_atomic.inc"

#include "vivian_instructions_integer.inc"

    case 0x0F: /* FENCE; FENCE.I remains unsupported. */
        if (function3 != 0x0) {
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;

#include "vivian_instructions_float_memory.inc"

#include "vivian_instructions_float.inc"

#include "vivian_instructions_system.inc"

    default:
        VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
    }

    state->pc = next_pc;
    attempts++;
    goto next_instruction;

instruction_stopped:

    attempts++;

execution_complete:

    if (floating_flags_pending) {
        vivian_accrue_float_flags(state);
    }
    if (steps_executed) {
        *steps_executed = attempts;
    }
    return status;
}

#undef VIVIAN_FAULT
#undef VIVIAN_ARM_FLOAT_FLAGS

VivianStatus VIVIAN_Step(VivianState* state) {
    return vivian_run(state, 1u, 0);
}

VivianStatus VIVIAN_Run(VivianState* state, uint32_t step_budget, uint32_t* steps_executed) {
    return vivian_run(state, step_budget, steps_executed);
}
