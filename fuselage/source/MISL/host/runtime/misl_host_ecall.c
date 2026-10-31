/* Complete a serviced ECALL once, including yielders, but not halted calls. */
#include "misl_host_state_internal.h"
#include "misl_host_services_internal.h"
#include "../../misl_abi.h"

int fuselage_guest_service_ecall(void) {
    if (fuselage_guest_session.phase != MISL_HOST_RUNNING) {
        return 0;
    }
    VivianState* cpu = &fuselage_guest_session.cpu;
    const uint8_t* instruction = VIVIAN_ResolveMemory(cpu, cpu->pc, 4);
    if ((cpu->pc & 1u) || !instruction || instruction[0] != 0x73 || instruction[1] || instruction[2] ||
        instruction[3]) {
        fuselage_guest_report_fault("Host dispatch requires a pending ECALL instruction");
        return 0;
    }
    uint32_t call = cpu->regs[17];
    if (!fuselage_guest_gate_abi(call)) {
        return 0;
    }
    int continue_slice;
    switch (call & 0xFF00u) {
    case 0x0100u:
        continue_slice = fuselage_guest_service_system(call);
        break;
    case 0x1000u:
        continue_slice = fuselage_guest_service_text(call);
        break;
    case 0x1100u:
        continue_slice = fuselage_guest_service_sprites(call);
        break;
    case 0x1200u:
        continue_slice = fuselage_guest_service_tiles(call);
        break;
    case 0x1300u:
        continue_slice = fuselage_guest_service_pixies(call);
        break;
    case 0x1400u:
        continue_slice = fuselage_guest_service_colors(call);
        break;
    case 0x1500u:
        continue_slice = fuselage_guest_service_interactions(call);
        break;
    case 0x2000u:
        continue_slice = fuselage_guest_service_input(call);
        break;
    case 0x3000u:
        continue_slice = fuselage_guest_service_audio(call);
        break;
    case 0x4000u:
        continue_slice = fuselage_guest_service_dice(call);
        break;
    case 0x5000u:
        continue_slice = fuselage_guest_service_rom(call);
        break;
    default:
        continue_slice = fuselage_guest_unknown_ecall(call);
        break;
    }
    if (fuselage_guest_session.phase != MISL_HOST_RUNNING) {
        return 0;
    }
    VIVIAN_CompleteEcall(cpu);
    return continue_slice;
}
