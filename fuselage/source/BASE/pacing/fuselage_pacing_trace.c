/* Root pacing. SEE: (FUSELAGE_PACING.md, Controller) */
#include "fuselage_pacing_internal.h"
#include "../../DICE/dice.h"
#include "../../DICE/timers/dice_timers_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void fuselage_pacing_trace_second(uint64_t now_us) {
    double seconds = (double)now_us / 1e6;
    const char* state = fuselage_pacing_state.retune_failed ? "RETUNE-FAILED"
                        : !fuselage_pacing_state.refresh_us ? "MEASURING"
                        : fuselage_pacing_state.locked      ? "LOCKED"
                                                            : "FREE";

    fprintf(
        stderr,
        "[pacing] %7.2fs %-9s refresh=%.3fHz(%.1fus) n=%+d period=%.1fus"
        " frames=%u ticks/frame{0:%u 1:%u 2:%u 3+:%u} anomalies=%u"
        " lag avg/max=%.2f/%.2fms lockwait=%.2fms prep=%.2fms submit=%.2fms"
        " tick avg/max=%.2f/%.2fms shift=%+.0fus\n",
        seconds, state, fuselage_pacing_state.refresh_us > 0.0 ? 1e6 / fuselage_pacing_state.refresh_us : 0.0,
        fuselage_pacing_state.refresh_us, fuselage_pacing_state.ratio,
        fuselage_pacing_state.retune_failed ? 0.0
        : fuselage_pacing_state.locked      ? fuselage_pacing_state.applied_us
                                            : fuselage_pacing_state.declared_us,
        fuselage_pacing_state.trace_frames_count, fuselage_pacing_state.trace_histogram[0],
        fuselage_pacing_state.trace_histogram[1], fuselage_pacing_state.trace_histogram[2],
        fuselage_pacing_state.trace_histogram[3], fuselage_pacing_state.trace_anomalies,
        fuselage_pacing_state.trace_frames_count
            ? fuselage_pacing_state.trace_lag_sum / fuselage_pacing_state.trace_frames_count / 1000.0
            : 0.0,
        fuselage_pacing_state.trace_lag_max / 1000.0, fuselage_pacing_state.trace_wait_max / 1000.0,
        fuselage_pacing_state.trace_prepare_max / 1000.0, fuselage_pacing_state.trace_submit_max / 1000.0,
        fuselage_pacing_state.tick_duration_count ? (double)fuselage_pacing_state.tick_duration_sum_us /
                                                        fuselage_pacing_state.tick_duration_count / 1000.0
                                                  : 0.0,
        (double)fuselage_pacing_state.tick_duration_max_us / 1000.0, fuselage_pacing_state.trace_shift_sum);

    fuselage_pacing_state.trace_frames_count = 0;
    memset(fuselage_pacing_state.trace_histogram, 0, sizeof(fuselage_pacing_state.trace_histogram));
    fuselage_pacing_state.trace_lag_sum = fuselage_pacing_state.trace_lag_max = 0.0;
    fuselage_pacing_state.trace_wait_max = fuselage_pacing_state.trace_prepare_max =
        fuselage_pacing_state.trace_submit_max = 0.0;
    fuselage_pacing_state.trace_shift_sum = 0.0;
    fuselage_pacing_state.trace_anomalies = 0;
    fuselage_pacing_state.tick_duration_sum_us = fuselage_pacing_state.tick_duration_max_us = 0;
    fuselage_pacing_state.tick_duration_count = 0;
    fuselage_pacing_state.trace_start_us = now_us;

    return;
}
