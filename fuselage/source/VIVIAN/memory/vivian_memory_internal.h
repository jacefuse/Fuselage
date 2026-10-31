/* Internal store notification shared with the instruction executor. */
#ifndef VIVIAN_MEMORY_INTERNAL_H
#define VIVIAN_MEMORY_INTERNAL_H
#include "../vivian.h"

/* Known boundary limits are documented in VIVIAN.md, Memory mappings. */
static inline void vivian_notify_store(VivianState* state, uint32_t address, uint32_t byte_count) {
    if (!state->watchFn) {
        return;
    }
    if (address < state->watchEnd && address + byte_count > state->watchBase) {
        state->watchFn(address, byte_count, state->watchUser);
    }
}
#endif
