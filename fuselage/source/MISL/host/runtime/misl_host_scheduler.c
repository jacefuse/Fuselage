/* Load, pump and execute one host tick. SEE: (MISL_HOST_RUNTIME.md, Tick order) */
#include "misl_host_state_internal.h"
#include "misl_host_services_internal.h"
#include "../graphics/misl_host_graphics_internal.h"
#include "misl_host_save_internal.h"
#include "misl_host_scheduler_internal.h"
#include "../../../DICE/dice.h"
#include <stdio.h>

static void fuselage_guest_run_slice(void) {
    uint64_t start_us = DICE_GetElapsedMicroseconds();
    uint64_t budget_us = fuselage_guest_slice_budget();
    uint32_t since_check = 0;
    if (VIVIAN_EnsureFloatingPointEnvironment()) {
#ifndef NDEBUG
        static int reported;
        if (!reported) {
            fprintf(stderr,
                    "[MISL] Host floating controls changed; attempted restoration before execution.\n");
            reported = 1;
        }
#endif
    }
    for (;;) {
        uint32_t steps_used = 0;
        VivianStatus status = VIVIAN_Run(&fuselage_guest_session.cpu, MISL_HOST_STEP_CHUNK, &steps_used);
        since_check += steps_used;
        fuselage_guest_session.steps_executed += steps_used;
        if (since_check >= MISL_HOST_STEP_CHUNK) {
            since_check = 0;
            if (DICE_GetElapsedMicroseconds() - start_us >= budget_us) {
                if (status == VIVIAN_ECALL) {
                    if (!fuselage_guest_service_ecall()) {
                        fuselage_guest_end_frame(DICE_GetElapsedMicroseconds() - start_us);
                        return;
                    }
                    status = VIVIAN_OK;
                }
                if (status == VIVIAN_OK) {
                    fuselage_guest_cut_slice(DICE_GetElapsedMicroseconds() - start_us);
                    return;
                }
            }
        }
        switch (status) {
        case VIVIAN_OK:
            break;
        case VIVIAN_ECALL:
            if (!fuselage_guest_service_ecall()) {
                fuselage_guest_end_frame(DICE_GetElapsedMicroseconds() - start_us);
                return;
            }
            break;
        case VIVIAN_HALT:
            fuselage_guest_halt(MISL_GUEST_HALT_FINISHED, "It reached EBREAK and stopped normally");
            return;
        default: {
            const char* reason = status == VIVIAN_ILLEGAL_INSTRUCTION    ? "Illegal instruction"
                                 : status == VIVIAN_MISALIGNED_FETCH     ? "Misaligned fetch"
                                 : status == VIVIAN_BUS_ERROR            ? "Bus error"
                                 : status == VIVIAN_UNSUPPORTED_ROUNDING ? "Unsupported guest rounding mode"
                                                                         : "Unknown execution fault";
            fprintf(stderr, "[MISL] %s at pc=0x%08X\n", reason, fuselage_guest_session.cpu.pc);
            fuselage_guest_report_fault(reason);
            return;
        }
        }
    }
}
void FuselageRunGuest(const MislGuestImage* image) {
    if (fuselage_guest_session.phase == MISL_HOST_UNLOADED || fuselage_guest_session.image != image) {
        fuselage_guest_reset_devices();
        if (fuselage_guest_session.image) {
            fuselage_guest_finish_save(false);
        }
        if (!fuselage_guest_prepare_load(image)) {
            fuselage_guest_close_save();
            return;
        }
        if (!fuselage_guest_commit_save_load()) {
            return;
        }
        fuselage_guest_install_graphics_watch();
    }
    if (fuselage_guest_session.phase != MISL_HOST_RUNNING) {
        return;
    }
    fuselage_guest_pump_devices();
    if (fuselage_guest_session.sleep_ticks) {
        fuselage_guest_session.sleep_ticks--;
    } else {
        fuselage_guest_run_slice();
    }
    fuselage_guest_flush_graphics();
    fuselage_guest_drain_audio();
    if (fuselage_guest_session.halt.kind == MISL_GUEST_HALT_FAULT) {
        fuselage_guest_finish_save(false);
    }
}
