/* Native presentation of host admission/dispatch faults. */
#include "../runtime/misl_host_state_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../GDMF/gdmf_colors.h"
#include "../../../GDMF/textlayer/gdmf_textlayer.h"

void fuselage_guest_report_fault(const char* reason) {
    tlActivate();
    tlPrintFormattedC(RED, "[MISL] %s\n", reason);
    fuselage_guest_halt(MISL_GUEST_HALT_FAULT, reason);
}

void fuselage_guest_report_abi_mismatch(uint32_t requested_version) {
    tlActivate();
    tlPrintFormattedC(RED, "[MISL] ROAP ABI %u, host ABI %u. Rebuild against the current headers.\n",
                      requested_version, (unsigned)MISL_ABI_VERSION);
    fuselage_guest_halt(MISL_GUEST_HALT_FAULT, "ABI mismatch. Rebuild the ROAP against the current headers");
}

int fuselage_guest_unknown_ecall(uint32_t call) {
    tlActivate();
    tlPrintFormattedC(RED, "[MISL] Unknown or unimplemented ecall 0x%08x\n", call);
    fuselage_guest_halt(MISL_GUEST_HALT_FAULT, "Unknown or unimplemented ecall");
    return 0;
}
