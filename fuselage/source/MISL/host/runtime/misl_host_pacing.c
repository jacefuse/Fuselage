/* Yield-delimited execution estimates. SEE: (MISL_HOST_RUNTIME.md, Slice pacing) */
#include "misl_host_state_internal.h"
#include "misl_host_scheduler_internal.h"
#include "../../../BASE/fuselage_runtime.h"

uint64_t fuselage_guest_slice_budget(void) {
    double rate = FuselageGetSimRate();
    if (!(rate > 0) || rate > 100000) {
        rate = 60;
    }
    /* Such periods already exceed the final ceiling; avoid an overflowing cast. */
    if (rate < 1) {
        return MISL_HOST_SLICE_CEILING_US;
    }
    uint64_t period = (uint64_t)(1000000.0 / rate);
    uint64_t grace = period / 5;
    if (grace < 1500) {
        grace = 1500;
    }
    uint64_t budget = period <= grace + MISL_HOST_MIN_SLICE_US ? MISL_HOST_MIN_SLICE_US : period - grace;
    uint64_t estimate = fuselage_guest_session.frame_estimate_us;
    if (estimate > MISL_HOST_SLICE_CEILING_US) {
        estimate = MISL_HOST_SLICE_CEILING_US;
    }
    uint64_t needed = estimate + estimate / 4;
    if (needed > budget) {
        budget = needed;
    }
    return budget > MISL_HOST_SLICE_CEILING_US ? MISL_HOST_SLICE_CEILING_US : budget;
}
void fuselage_guest_end_frame(uint64_t slice_us) {
    uint64_t accumulated = fuselage_guest_session.frame_accumulated_us;
    uint64_t total =
        slice_us > MISL_HOST_SLICE_CEILING_US || accumulated > MISL_HOST_SLICE_CEILING_US - slice_us
            ? MISL_HOST_SLICE_CEILING_US
            : accumulated + slice_us;
    if (!fuselage_guest_session.frame_abandoned) {
        uint64_t* estimate = &fuselage_guest_session.frame_estimate_us;
        if (total > *estimate) {
            *estimate = total;
        } else {
            *estimate -= (*estimate - total) / 8;
        }
        if (*estimate > MISL_HOST_SLICE_CEILING_US) {
            *estimate = MISL_HOST_SLICE_CEILING_US;
        }
        if (*estimate < MISL_HOST_MIN_SLICE_US) {
            *estimate = MISL_HOST_MIN_SLICE_US;
        }
    }
    fuselage_guest_session.frame_accumulated_us = 0;
    fuselage_guest_session.frame_abandoned = 0;
}
void fuselage_guest_cut_slice(uint64_t slice_us) {
    if (!fuselage_guest_session.frame_abandoned) {
        uint64_t accumulated = fuselage_guest_session.frame_accumulated_us;
        if (slice_us > MISL_HOST_SLICE_CEILING_US || accumulated > MISL_HOST_SLICE_CEILING_US - slice_us) {
            fuselage_guest_session.frame_abandoned = 1;
            fuselage_guest_session.frame_accumulated_us = 0;
        } else {
            fuselage_guest_session.frame_accumulated_us += slice_us;
        }
    }
    fuselage_guest_session.frame_estimate_us -= fuselage_guest_session.frame_estimate_us / 8;
    if (fuselage_guest_session.frame_estimate_us < MISL_HOST_MIN_SLICE_US) {
        fuselage_guest_session.frame_estimate_us = MISL_HOST_MIN_SLICE_US;
    }
}
