/* Independent execution core. SEE: (VIVIAN.md, Core interface) */
#ifndef VIVIAN_H
#define VIVIAN_H
#include <stdint.h>

#define VIVIAN_REGISTER_COUNT 32
#define VIVIAN_BANK_SET_COUNT 2
#define VIVIAN_READ_ONLY_SPAN_COUNT 3

/* Status values retain their existing numeric meanings. */
typedef enum {
    VIVIAN_OK = 0,
    VIVIAN_HALT,
    VIVIAN_ECALL,
    VIVIAN_ILLEGAL_INSTRUCTION,
    VIVIAN_MISALIGNED_FETCH,
    VIVIAN_BUS_ERROR,
    VIVIAN_UNSUPPORTED_ROUNDING,
} VivianStatus;

/* Receives the complete reported store range, including overlap beyond the watch. */
typedef void (*VivianStoreWatch)(uint32_t address, uint32_t byte_count, void* user);

typedef struct {
    uint8_t* const* banks;
    uint32_t base, shift, size, count;
    const uint8_t* writable;
} VivianBankSet;

/* Field spellings/layout are retained. SEE: (VIVIAN.md, State and ownership) */
typedef struct {
    uint32_t regs[VIVIAN_REGISTER_COUNT];
    uint64_t fregs[VIVIAN_REGISTER_COUNT];
    uint32_t fcsr;
    uint32_t pc;
    uint8_t* mem;
    uint32_t memSize;

    uint8_t* win;
    uint32_t winBase, winSize;
    VivianBankSet bank[VIVIAN_BANK_SET_COUNT];
    struct {
        const uint8_t* ptr;
        uint32_t base, size;
    } rom[VIVIAN_READ_ONLY_SPAN_COUNT];

    uint32_t watchBase, watchEnd;
    VivianStoreWatch watchFn;
    void* watchUser;
    int lrValid;
    uint32_t lrAddr;
} VivianState;

/* Initialize borrowed program memory/state; does not clear program bytes. */
void VIVIAN_Init(VivianState* state, uint8_t* memory, uint32_t memory_size, uint32_t entry_address);

/* Map after initialization. The caller prevents overlaps and owns all storage. */
void VIVIAN_MapWindow(VivianState* state, uint32_t base, uint8_t* memory, uint32_t byte_count);
void VIVIAN_MapReadOnlySpan(VivianState* state, uint32_t slot, uint32_t base, const uint8_t* memory,
                           uint32_t byte_count);
/* Slot-zero convenience form of MapReadOnlySpan. */
void VIVIAN_MapReadOnlyMemory(VivianState* state, uint32_t base, const uint8_t* memory, uint32_t byte_count);

/* Read-access resolution; the legacy return type does not grant write permission. */
void* VIVIAN_ResolveMemory(const VivianState* state, uint32_t address, uint32_t byte_count);
void* VIVIAN_ResolveWritableMemory(const VivianState* state, uint32_t address, uint32_t byte_count);

/* Bank stride is 1 << bank_shift. SEE: (VIVIAN.md, Memory mappings) */
void VIVIAN_MapBanks(VivianState* state, uint32_t bank_set, uint32_t base, uint32_t bank_shift,
                    uint32_t bank_size, uint32_t bank_count, uint8_t* const* banks, const uint8_t* writable);

/* Watch [base, end); a null callback disables it. */
void VIVIAN_SetStoreWatch(VivianState* state, uint32_t base, uint32_t end, VivianStoreWatch callback,
                         void* user);
/* Notify the watch after a host write; currently does not invalidate LR/SC state. */
void VIVIAN_NotifyStore(VivianState* state, uint32_t address, uint32_t byte_count);

/* Advance past one serviced ECALL. Caller must verify the pending instruction. */
void VIVIAN_CompleteEcall(VivianState* state);

/* Change the calling thread's floating controls. SEE: (VIVIAN.md, Floating-point environment) */
void VIVIAN_EstablishFloatingPointEnvironment(void);
/* Nonzero means drift was detected and correction attempted. */
int VIVIAN_EnsureFloatingPointEnvironment(void);

VivianStatus VIVIAN_Step(VivianState* state);
/* Returns one on expansion, zero otherwise. Use output only on success. */
int VIVIAN_ExpandCompressedInstruction(uint16_t instruction, uint32_t* expanded_instruction);
/* Count includes faulting attempts; steps_executed may be null. */
VivianStatus VIVIAN_Run(VivianState* state, uint32_t step_budget, uint32_t* steps_executed);
#endif
