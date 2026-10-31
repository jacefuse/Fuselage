/* SEE: (VIVIAN.md, State and ownership) */
#include "../vivian.h"

void VIVIAN_Init(VivianState* state, uint8_t* memory, uint32_t memory_size, uint32_t entry_address) {
    for (int register_index = 0; register_index < VIVIAN_REGISTER_COUNT; register_index++) {
        state->regs[register_index] = 0;
        state->fregs[register_index] = 0;
    }
    state->fcsr = 0;

    VIVIAN_EstablishFloatingPointEnvironment();
    state->mem = memory;
    state->memSize = memory_size;
    state->pc = entry_address;
    state->watchBase = 0;
    state->watchEnd = 0;
    state->watchFn = 0;
    state->watchUser = 0;

    state->win = 0;
    state->winBase = 0;
    state->winSize = 0;
    for (uint32_t mapping_index = 0; mapping_index < VIVIAN_READ_ONLY_SPAN_COUNT; mapping_index++) {
        state->rom[mapping_index].ptr = 0;
        state->rom[mapping_index].base = 0;
        state->rom[mapping_index].size = 0;
    }

    for (uint32_t mapping_index = 0; mapping_index < VIVIAN_BANK_SET_COUNT; mapping_index++) {
        state->bank[mapping_index].banks = 0;
        state->bank[mapping_index].count = 0;
        state->bank[mapping_index].writable = 0;
    }

    state->lrValid = 0;
    state->lrAddr = 0;
}

void VIVIAN_CompleteEcall(VivianState* state) {
    state->pc += 4u;
}
