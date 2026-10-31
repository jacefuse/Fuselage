/* Own stop reasons and capture execution state before mappings can change. */
#include "misl_host_state_internal.h"
#include <string.h>

static void fuselage_guest_copy_halt_reason(const char* reason) {
    if (!reason) {
        reason = "halted";
    }
    char copy[sizeof fuselage_guest_session.halt_reason];
    size_t length = 0;
    while (length < sizeof copy - 1 && reason[length]) {
        copy[length] = reason[length];
        length++;
    }
    copy[length] = '\0';
    memcpy(fuselage_guest_session.halt_reason, copy, length + 1);
    fuselage_guest_session.halt.reason = fuselage_guest_session.halt_reason;
}

static uint32_t fuselage_guest_read_stopped_instruction(const VivianState* cpu) {
    const uint8_t* instruction = VIVIAN_ResolveMemory(cpu, cpu->pc, 2);
    if (!instruction) {
        return 0;
    }
    uint32_t first_half = (uint32_t)instruction[0] | ((uint32_t)instruction[1] << 8);
    if ((first_half & 3u) != 3u) {
        return first_half;
    }
    instruction = VIVIAN_ResolveMemory(cpu, cpu->pc, 4);
    if (!instruction) {
        return 0;
    }
    return first_half | ((uint32_t)instruction[2] << 16) | ((uint32_t)instruction[3] << 24);
}

void fuselage_guest_fail_load(MislGuestHaltKind kind, const char* reason) {
    /* The reason may be borrowed from memory that release_memory will free. */
    fuselage_guest_session.halt = (MislGuestHaltInfo){.kind = kind};
    fuselage_guest_copy_halt_reason(reason);
    fuselage_guest_release_memory();
    fuselage_guest_session.phase = MISL_HOST_HALTED;
}

void fuselage_guest_halt(MislGuestHaltKind kind, const char* reason) {
    if (fuselage_guest_session.phase == MISL_HOST_HALTED) {
        return;
    }
    if (fuselage_guest_session.phase != MISL_HOST_RUNNING) {
        fuselage_guest_fail_load(kind, reason);
        return;
    }
    MislGuestHaltInfo halt = {.kind = kind};
    if (fuselage_guest_session.phase == MISL_HOST_RUNNING) {
        halt.program_counter = fuselage_guest_session.cpu.pc;
        halt.instruction = fuselage_guest_read_stopped_instruction(&fuselage_guest_session.cpu);
        halt.stack_pointer = fuselage_guest_session.cpu.regs[2];
        halt.return_address = fuselage_guest_session.cpu.regs[1];
    }
    fuselage_guest_session.halt = halt;
    fuselage_guest_copy_halt_reason(reason);
    fuselage_guest_session.phase = MISL_HOST_HALTED;
}

int FuselageIsGuestHalted(void) {
    return fuselage_guest_session.phase == MISL_HOST_HALTED;
}

int FuselageGetGuestHaltInfo(MislGuestHaltInfo* output) {
    int halted = FuselageIsGuestHalted();
    if (output) {
        *output = halted ? fuselage_guest_session.halt : (MislGuestHaltInfo){0};
    }
    return halted;
}
