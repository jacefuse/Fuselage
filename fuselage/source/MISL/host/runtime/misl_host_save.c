/* Persistence binding. SEE: (SRAM.md, MISL integration) */
#include "misl_host_state_internal.h"
#include "misl_host_save_internal.h"
#include "../../../SRAM/sram_host.h"

int fuselage_guest_commit_save_load(void) {
    if (fuselage_guest_session.phase != MISL_HOST_PREPARED) {
        return 0;
    }
    const MislHostImagePlan* plan = &fuselage_guest_session.plan;
    if (!SRAM_Configure(plan->save_id, plan->save_version, plan->save_bytes) ||
        !SRAM_Open(fuselage_guest_session.program_path[0] ? fuselage_guest_session.program_path
                                                          : "program.roap")) {
        fuselage_guest_fail_load(MISL_GUEST_HALT_SRAM, SRAM_GetFailureReason());
        return 0;
    }
    uint32_t byte_count;
    const uint8_t* storage = sram_get_host_span(&byte_count);
    if (!fuselage_guest_commit_load(storage, byte_count)) {
        SRAM_Close();
        return 0;
    }
    return 1;
}
bool fuselage_guest_write_save(uint32_t offset, const void* input, uint32_t byte_count) {
    uint32_t mapped_bytes;
    const uint8_t* storage = sram_get_host_span(&mapped_bytes);
    return fuselage_guest_session.phase == MISL_HOST_RUNNING && storage &&
           storage == fuselage_guest_session.save_span &&
           mapped_bytes == fuselage_guest_session.save_byte_count && SRAM_Write(offset, input, byte_count);
}
uint32_t fuselage_guest_get_save_size(void) {
    return SRAM_GetSize();
}
bool fuselage_guest_finish_save(bool clean_exit) {
    if (!clean_exit || fuselage_guest_session.halt.kind == MISL_GUEST_HALT_FAULT ||
        fuselage_guest_session.halt.kind == MISL_GUEST_HALT_SRAM) {
        SRAM_Abandon();
    }
    return SRAM_Save();
}
bool fuselage_guest_close_save(void) {
    if (fuselage_guest_session.save_span) {
        return false;
    }
    SRAM_Close();
    return true;
}
