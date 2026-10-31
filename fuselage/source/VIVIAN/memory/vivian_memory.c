/* SEE: (VIVIAN.md, Memory mappings) */
#include "vivian_memory_internal.h"
#include <stddef.h>

static uint8_t* vivian_resolve_bank(const VivianState* state, uint32_t address, uint32_t byte_count,
                                   int for_write) {
    for (uint32_t bank_set = 0; bank_set < VIVIAN_BANK_SET_COUNT; bank_set++) {
        const VivianBankSet* mapping = &state->bank[bank_set];
        if (!mapping->banks || !mapping->count || address < mapping->base) {
            continue;
        }
        uint32_t offset = address - mapping->base;
        uint32_t bank_index = offset >> mapping->shift;
        if (bank_index >= mapping->count) {
            continue;
        }

        if (for_write && (!mapping->writable || !mapping->writable[bank_index])) {
            return NULL;
        }
        uint8_t* bank_memory = mapping->banks[bank_index];
        if (!bank_memory) {
            return NULL;
        }

        uint32_t bank_offset = offset & ((1u << mapping->shift) - 1u);
        if ((uint64_t)bank_offset + byte_count > (uint64_t)mapping->size) {
            return NULL;
        }
        return bank_memory + bank_offset;
    }
    return NULL;
}

void* VIVIAN_ResolveMemory(const VivianState* state, uint32_t address, uint32_t byte_count) {
    if ((uint64_t)address + byte_count <= (uint64_t)state->memSize) {
        return state->mem + address;
    }
    if (state->win && state->winSize && address >= state->winBase &&
        (uint64_t)address + byte_count <= (uint64_t)state->winBase + state->winSize) {
        return state->win + (address - state->winBase);
    }
    for (uint32_t slot = 0; slot < VIVIAN_READ_ONLY_SPAN_COUNT; slot++) {
        if (state->rom[slot].ptr && state->rom[slot].size && address >= state->rom[slot].base &&
            (uint64_t)address + byte_count <= (uint64_t)state->rom[slot].base + state->rom[slot].size) {

            return (void*)(uintptr_t)(state->rom[slot].ptr + (address - state->rom[slot].base));
        }
    }
    return vivian_resolve_bank(state, address, byte_count, 0);
}

void* VIVIAN_ResolveWritableMemory(const VivianState* state, uint32_t address, uint32_t byte_count) {
    if ((uint64_t)address + byte_count <= (uint64_t)state->memSize) {
        return state->mem + address;
    }
    if (state->win && state->winSize && address >= state->winBase &&
        (uint64_t)address + byte_count <= (uint64_t)state->winBase + state->winSize) {
        return state->win + (address - state->winBase);
    }

    return vivian_resolve_bank(state, address, byte_count, 1);
}

void VIVIAN_MapBanks(VivianState* state, uint32_t bank_set, uint32_t base, uint32_t bank_shift,
                    uint32_t bank_size, uint32_t bank_count, uint8_t* const* banks, const uint8_t* writable) {
    if (bank_set >= VIVIAN_BANK_SET_COUNT) {
        return;
    }
    VivianBankSet* mapping = &state->bank[bank_set];

    if (!banks || bank_count == 0 || bank_shift >= 32 || bank_size > (1u << bank_shift)) {
        mapping->banks = NULL;
        mapping->count = 0;
        return;
    }
    mapping->banks = banks;
    mapping->base = base;
    mapping->shift = bank_shift;
    mapping->size = bank_size;
    mapping->count = bank_count;
    mapping->writable = writable;

    return;
}

void VIVIAN_MapWindow(VivianState* state, uint32_t base, uint8_t* memory, uint32_t byte_count) {
    state->win = memory;
    state->winBase = base;
    state->winSize = memory ? byte_count : 0u;
}

void VIVIAN_MapReadOnlySpan(VivianState* state, uint32_t slot, uint32_t base, const uint8_t* memory,
                           uint32_t byte_count) {
    if (slot >= VIVIAN_READ_ONLY_SPAN_COUNT) {
        return;
    }
    state->rom[slot].ptr = memory;
    state->rom[slot].base = base;
    state->rom[slot].size = memory ? byte_count : 0u;
}

void VIVIAN_MapReadOnlyMemory(VivianState* state, uint32_t base, const uint8_t* memory, uint32_t byte_count) {
    VIVIAN_MapReadOnlySpan(state, 0u, base, memory, byte_count);
}

void VIVIAN_SetStoreWatch(VivianState* state, uint32_t base, uint32_t end, VivianStoreWatch callback,
                         void* user) {
    state->watchBase = base;
    state->watchEnd = end;
    state->watchFn = callback;
    state->watchUser = user;
}

void VIVIAN_NotifyStore(VivianState* state, uint32_t address, uint32_t byte_count) {
    vivian_notify_store(state, address, byte_count);
}
