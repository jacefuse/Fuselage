#include "misl_host_arguments_internal.h"
#include "misl_host_services_internal.h"
#include "../../misl_abi.h"

int fuselage_guest_gate_abi(uint32_t call) {
    if (fuselage_guest_session.abi_declared || call == MISL_SYS_ABI_DECLARE) {
        return 1;
    }
    fuselage_guest_report_fault("The ROAP did not declare its ABI version");
    return 0;
}

int fuselage_guest_declare_abi(void) {
    uint32_t requested_version = fuselage_guest_get_argument(0);
    if (requested_version != MISL_ABI_VERSION) {
        fuselage_guest_set_result(0);
        fuselage_guest_report_abi_mismatch(requested_version);
        return 0;
    }
    fuselage_guest_session.abi_declared = 1;
    fuselage_guest_set_result(1);
    return 1;
}
