/* Little-endian memory access. SEE: (VIVIAN.md, Instruction memory access) */
#ifndef VIVIAN_ACCESS_INTERNAL_H
#define VIVIAN_ACCESS_INTERNAL_H
#include "../vivian.h"

/* Keep ordinary RAM accesses in the execution unit; mapped ranges use the resolver. */
static inline void* vivian_resolve_read_access(const VivianState* state, uint32_t address,
                                              uint32_t byte_count) {
    if ((uint64_t)address + byte_count <= (uint64_t)state->memSize) {
        return state->mem + address;
    }
    return VIVIAN_ResolveMemory(state, address, byte_count);
}

static inline void* vivian_resolve_write_access(const VivianState* state, uint32_t address,
                                               uint32_t byte_count) {
    if ((uint64_t)address + byte_count <= (uint64_t)state->memSize) {
        return state->mem + address;
    }
    return VIVIAN_ResolveWritableMemory(state, address, byte_count);
}

/* Resolve the whole access before reading or writing. Notifications belong to the caller. */
static inline int vivian_load_byte(const VivianState* state, uint32_t address, uint8_t* value) {
    const uint8_t* memory = (const uint8_t*)vivian_resolve_read_access(state, address, 1);
    if (!memory) {
        return 0;
    }
    *value = memory[0];
    return 1;
}

static inline int vivian_load_halfword(const VivianState* state, uint32_t address, uint16_t* value) {
    const uint8_t* memory = (const uint8_t*)vivian_resolve_read_access(state, address, 2);
    if (!memory) {
        return 0;
    }
    *value = (uint16_t)(memory[0] | (memory[1] << 8));
    return 1;
}

static inline int vivian_load_word(const VivianState* state, uint32_t address, uint32_t* value) {
    const uint8_t* memory = (const uint8_t*)vivian_resolve_read_access(state, address, 4);
    if (!memory) {
        return 0;
    }
    *value = (uint32_t)memory[0] | ((uint32_t)memory[1] << 8) | ((uint32_t)memory[2] << 16) |
             ((uint32_t)memory[3] << 24);
    return 1;
}

static inline int vivian_load_doubleword(const VivianState* state, uint32_t address, uint64_t* value) {
    const uint8_t* memory = (const uint8_t*)vivian_resolve_read_access(state, address, 8);
    if (!memory) {
        return 0;
    }
    uint64_t loaded_value = 0;
    for (int byte_index = 0; byte_index < 8; byte_index++) {
        loaded_value |= (uint64_t)memory[byte_index] << (8 * byte_index);
    }
    *value = loaded_value;
    return 1;
}

static inline int vivian_store_byte(VivianState* state, uint32_t address, uint8_t value) {
    uint8_t* memory = (uint8_t*)vivian_resolve_write_access(state, address, 1);
    if (!memory) {
        return 0;
    }
    memory[0] = value;
    return 1;
}

static inline int vivian_store_halfword(VivianState* state, uint32_t address, uint16_t value) {
    uint8_t* memory = (uint8_t*)vivian_resolve_write_access(state, address, 2);
    if (!memory) {
        return 0;
    }
    memory[0] = (uint8_t)(value);
    memory[1] = (uint8_t)(value >> 8);
    return 1;
}

static inline int vivian_store_word(VivianState* state, uint32_t address, uint32_t value) {
    uint8_t* memory = (uint8_t*)vivian_resolve_write_access(state, address, 4);
    if (!memory) {
        return 0;
    }
    memory[0] = (uint8_t)(value);
    memory[1] = (uint8_t)(value >> 8);
    memory[2] = (uint8_t)(value >> 16);
    memory[3] = (uint8_t)(value >> 24);
    return 1;
}

static inline int vivian_store_doubleword(VivianState* state, uint32_t address, uint64_t value) {
    uint8_t* memory = (uint8_t*)vivian_resolve_write_access(state, address, 8);
    if (!memory) {
        return 0;
    }
    for (int byte_index = 0; byte_index < 8; byte_index++) {
        memory[byte_index] = (uint8_t)(value >> (8 * byte_index));
    }
    return 1;
}

#endif
