/* Root pacing. SEE: (FUSELAGE_PACING.md, Controller) */
#include "fuselage_pacing_internal.h"
#include "../../DICE/dice.h"
#include "../../DICE/timers/dice_timers_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
FuselagePacingState fuselage_pacing_state;
void fuselage_pacing_init(uint8_t simulation_timer, uint8_t wait_timer, double simulation_rate) {
    fuselage_pacing_state = (FuselagePacingState){0};
    fuselage_pacing_state.simulation_timer = simulation_timer;
    fuselage_pacing_state.wait_timer = wait_timer;
    double period_ns = 1e9 / simulation_rate;
    if (!isfinite(period_ns) || period_ns < 1 || period_ns >= 0x1p63) {
        period_ns = 1e9 / 60.0;
    }
    fuselage_pacing_state.declared_us = period_ns / 1000.0;
    const char* mode = getenv("FUSELAGE_PACING");
    fuselage_pacing_state.enabled = !(mode && strcmp(mode, "off") == 0);
    const char* trace = getenv("FUSELAGE_PACING_TRACE");
    fuselage_pacing_state.trace = trace != NULL;
    fuselage_pacing_state.trace_frames = trace && strcmp(trace, "2") == 0;
    fuselage_pacing_state.last_error = 1.0;
    fuselage_pacing_state.initialized = true;

    if (fuselage_pacing_state.trace) {
        fprintf(stderr, "[pacing] trace on; steering %s; declared sim %.3f Hz (%.1f us)\n",
                fuselage_pacing_state.enabled ? "on" : "OFF (FUSELAGE_PACING=off)", simulation_rate,
                fuselage_pacing_state.declared_us);
    }

    return;
}

void fuselage_pacing_shutdown(void) {
    fuselage_pacing_state = (FuselagePacingState){0};

    return;
}

bool fuselage_pacing_status(double* refresh_rate, int* ratio, double* period_us) {
    if (refresh_rate) {
        *refresh_rate =
            (fuselage_pacing_state.refresh_us > 0.0) ? 1e6 / fuselage_pacing_state.refresh_us : 0.0;
    }
    if (ratio) {
        *ratio = fuselage_pacing_state.locked ? fuselage_pacing_state.ratio : 0;
    }
    if (period_us) {
        *period_us = !fuselage_pacing_state.initialized || fuselage_pacing_state.retune_failed ? 0.0
                     : fuselage_pacing_state.locked ? fuselage_pacing_state.applied_us
                                                    : fuselage_pacing_state.declared_us;
    }

    return fuselage_pacing_state.locked;
}

void fuselage_pacing_tick(uint64_t begin_us, uint64_t end_us) {
    if (!fuselage_pacing_state.initialized) {
        return;
    }

    if (end_us < begin_us) {
        return;
    }
    fuselage_pacing_state.tick_count++;
    fuselage_pacing_state.last_tick_end_us = end_us;

    uint64_t duration_us = end_us - begin_us;
    fuselage_pacing_state.tick_duration_sum_us += duration_us;
    if (duration_us > fuselage_pacing_state.tick_duration_max_us) {
        fuselage_pacing_state.tick_duration_max_us = duration_us;
    }
    fuselage_pacing_state.tick_duration_count++;

    if (fuselage_pacing_state.last_sample_us && end_us >= fuselage_pacing_state.last_sample_us &&
        end_us - fuselage_pacing_state.last_sample_us > FUSELAGE_PACING_GAP_US &&
        fuselage_pacing_state.refresh_us > 0.0) {
        fuselage_pacing_reset_estimator("no frames");
        fuselage_pacing_state.last_sample_us = 0;
    }

    return;
}

void fuselage_pacing_frame(uint64_t lock_request_us, uint64_t lock_held_us, uint64_t sample_us,
                           uint64_t previous_submit_us) {
    if (!fuselage_pacing_state.initialized) {
        return;
    }

    if (lock_held_us < lock_request_us || sample_us < lock_held_us ||
        (fuselage_pacing_state.last_sample_us && sample_us < fuselage_pacing_state.last_sample_us)) {
        fuselage_pacing_reset_estimator("clock moved backwards");
        fuselage_pacing_state.last_sample_us = 0;
        return;
    }
    if (sample_us == fuselage_pacing_state.last_sample_us) {
        return;
    }
    uint64_t consumed = fuselage_pacing_state.tick_count - fuselage_pacing_state.ticks_seen;
    fuselage_pacing_state.ticks_seen = fuselage_pacing_state.tick_count;

    double lock_wait_us = (double)(lock_held_us - lock_request_us);
    double prepare_us = (double)(sample_us - lock_held_us);

    if (fuselage_pacing_state.trace) {
        if (!fuselage_pacing_state.trace_start_us) {
            fuselage_pacing_state.trace_start_us = sample_us;
        }
        fuselage_pacing_state.trace_frames_count++;
        fuselage_pacing_state.trace_histogram[consumed > 3 ? 3 : consumed]++;
        if (lock_wait_us > fuselage_pacing_state.trace_wait_max) {
            fuselage_pacing_state.trace_wait_max = lock_wait_us;
        }
        if (prepare_us > fuselage_pacing_state.trace_prepare_max) {
            fuselage_pacing_state.trace_prepare_max = prepare_us;
        }
        if ((double)previous_submit_us > fuselage_pacing_state.trace_submit_max) {
            fuselage_pacing_state.trace_submit_max = (double)previous_submit_us;
        }
        if (consumed >= 1) {
            double lag_us = (sample_us >= fuselage_pacing_state.last_tick_end_us
                                 ? (double)(sample_us - fuselage_pacing_state.last_tick_end_us)
                                 : 0.0);
            fuselage_pacing_state.trace_lag_sum += lag_us;
            if (lag_us > fuselage_pacing_state.trace_lag_max) {
                fuselage_pacing_state.trace_lag_max = lag_us;
            }
        }

        uint64_t expected_ticks =
            (fuselage_pacing_state.ratio > 0) ? (uint64_t)fuselage_pacing_state.ratio : 1;
        bool anomaly = (fuselage_pacing_state.ratio < 0) ? (consumed > 1) : (consumed != expected_ticks);
        bool report =
            fuselage_pacing_state.refresh_us > 0.0 &&
            (fuselage_pacing_state.locked || fuselage_pacing_state.last_error <= FUSELAGE_PACING_LOCK_TOL);
        if (anomaly && report) {
            fuselage_pacing_state.trace_anomalies++;
            if (fuselage_pacing_state.trace_anomalies <= 10) {
                fprintf(stderr, "[pacing] %7.3fs frame consumed %llu ticks (expected %llu) %s\n",
                        (double)sample_us / 1e6, (unsigned long long)consumed,
                        (unsigned long long)expected_ticks, fuselage_pacing_state.locked ? "LOCKED" : "free");
            }
        }
    }

    if (fuselage_pacing_state.last_sample_us == 0) {
        fuselage_pacing_state.last_sample_us = sample_us;
        return;
    }

    double interval_us = (double)(sample_us - fuselage_pacing_state.last_sample_us);
    fuselage_pacing_state.last_sample_us = sample_us;

    if (interval_us > (double)FUSELAGE_PACING_GAP_US) {
        fuselage_pacing_reset_estimator("frame gap");
        return;
    }

    if (fuselage_pacing_state.refresh_us == 0.0) {
        fuselage_pacing_state.bootstrap[fuselage_pacing_state.bootstrap_count++] = interval_us;
        if (fuselage_pacing_state.bootstrap_count == FUSELAGE_PACING_BOOT) {
            qsort(fuselage_pacing_state.bootstrap, FUSELAGE_PACING_BOOT, sizeof(double),
                  fuselage_pacing_cmp_double);
            fuselage_pacing_state.refresh_us =
                0.5 * (fuselage_pacing_state.bootstrap[FUSELAGE_PACING_BOOT / 2 - 1] +
                       fuselage_pacing_state.bootstrap[FUSELAGE_PACING_BOOT / 2]);
            fuselage_pacing_state.refresh_index = 0;
            fuselage_pacing_state.ring[0] = (FuselagePacingMark){sample_us, 0};
            fuselage_pacing_state.ring_head = 1;
            fuselage_pacing_state.ring_count = 1;
            if (fuselage_pacing_state.trace) {
                fprintf(stderr, "[pacing] %7.3fs first estimate %.3f Hz from %d intervals\n",
                        (double)sample_us / 1e6, 1e6 / fuselage_pacing_state.refresh_us,
                        FUSELAGE_PACING_BOOT);
            }
        }
        return;
    }

    double interval_multiple = round(interval_us / fuselage_pacing_state.refresh_us);
    bool good = (interval_multiple >= 1.0) &&
                (fabs(interval_us - interval_multiple * fuselage_pacing_state.refresh_us) <=
                 FUSELAGE_PACING_RESIDUAL_TOL * fuselage_pacing_state.refresh_us);
    if (!good) {
        if (++fuselage_pacing_state.bad_run >= FUSELAGE_PACING_BAD_RUN) {
            fuselage_pacing_reset_estimator("cadence lost");
            return;
        }

        if (interval_multiple >= 3.0) {
            fuselage_pacing_state.ring[0] =
                (FuselagePacingMark){sample_us, fuselage_pacing_state.refresh_index};
            fuselage_pacing_state.ring_head = 1;
            fuselage_pacing_state.ring_count = 1;
        } else if (interval_multiple >= 1.0) {
            fuselage_pacing_state.refresh_index += (uint64_t)interval_multiple;
        }
        return;
    }
    fuselage_pacing_state.bad_run = 0;

    if (interval_multiple >= 3.0 ||
        (interval_multiple > 1.0 && fuselage_pacing_state.ring_count < FUSELAGE_PACING_SETTLED)) {
        fuselage_pacing_state.ring_head = fuselage_pacing_state.ring_count = 0;
        fuselage_pacing_state.ring[0] = (FuselagePacingMark){sample_us, fuselage_pacing_state.refresh_index};
        fuselage_pacing_state.ring_head = 1;
        fuselage_pacing_state.ring_count = 1;
        return;
    }

    fuselage_pacing_state.good_intervals++;
    fuselage_pacing_state.refresh_index += (uint64_t)interval_multiple;

    const FuselagePacingMark* oldest =
        &fuselage_pacing_state.ring[(fuselage_pacing_state.ring_head + FUSELAGE_PACING_RING -
                                     fuselage_pacing_state.ring_count) %
                                    FUSELAGE_PACING_RING];
    fuselage_pacing_state.ring[fuselage_pacing_state.ring_head] =
        (FuselagePacingMark){sample_us, fuselage_pacing_state.refresh_index};
    fuselage_pacing_state.ring_head = (fuselage_pacing_state.ring_head + 1) % FUSELAGE_PACING_RING;
    if (fuselage_pacing_state.ring_count < FUSELAGE_PACING_RING) {
        fuselage_pacing_state.ring_count++;
    } else {
        oldest = &fuselage_pacing_state.ring[fuselage_pacing_state.ring_head];
    }
    if (fuselage_pacing_state.refresh_index > oldest->refresh_index) {
        fuselage_pacing_state.refresh_us =
            (double)(sample_us - oldest->sample_us) /
            (double)(fuselage_pacing_state.refresh_index - oldest->refresh_index);
    }

    int ratio_multiple;
    double target;
    double ratio_error = fuselage_pacing_evaluate(&ratio_multiple, &target);
    fuselage_pacing_state.last_error = ratio_error;

    if (!fuselage_pacing_state.locked) {
        if (fuselage_pacing_state.enabled &&
            fuselage_pacing_state.good_intervals >= FUSELAGE_PACING_MIN_GOOD &&
            ratio_error <= FUSELAGE_PACING_LOCK_TOL) {
            fuselage_pacing_state.locked = true;
            fuselage_pacing_state.ratio = ratio_multiple;
            fuselage_pacing_state.target_us = target;
            fuselage_pacing_apply_period(target);
            if (fuselage_pacing_state.trace && fuselage_pacing_state.locked) {
                fprintf(stderr,
                        "[pacing] %7.3fs LOCK refresh=%.3fHz n=%+d -- sim period %.1f us (declared %.1f, "
                        "%+.2f%%)\n",
                        (double)sample_us / 1e6, 1e6 / fuselage_pacing_state.refresh_us, ratio_multiple,
                        target, fuselage_pacing_state.declared_us,
                        100.0 * (target - fuselage_pacing_state.declared_us) /
                            fuselage_pacing_state.declared_us);
            }
        }
    } else if (ratio_error > FUSELAGE_PACING_UNLOCK_TOL || ratio_multiple != fuselage_pacing_state.ratio) {
        fuselage_pacing_unlock("ratio left tolerance");
    } else {
        fuselage_pacing_state.target_us = target;
        if (fabs(target - fuselage_pacing_state.applied_us) >= FUSELAGE_PACING_PERIOD_EPS_US) {
            fuselage_pacing_apply_period(target);
        }
    }

    double frame_shift_us = 0.0;
    if (fuselage_pacing_state.locked && consumed >= 1) {
        double shift_us = 0.0;
        double maximum_shift_us = FUSELAGE_PACING_MAX_SHIFT_FRAC * fuselage_pacing_state.applied_us;
        double lag_us = (sample_us >= fuselage_pacing_state.last_tick_end_us
                             ? (double)(sample_us - fuselage_pacing_state.last_tick_end_us)
                             : 0.0);

        if (lock_wait_us > FUSELAGE_PACING_OVERLAP_US) {

            shift_us = -(lock_wait_us + 0.25 * FUSELAGE_PACING_MARGIN_US);
        } else {
            double phase_error_us = lag_us - FUSELAGE_PACING_MARGIN_US;
            if (phase_error_us > 0.0) {
                shift_us = phase_error_us * FUSELAGE_PACING_ADVANCE_GAIN;
            } else if (phase_error_us < 0.0) {
                shift_us = phase_error_us * 0.5;
            }
        }
        if (shift_us > maximum_shift_us) {
            shift_us = maximum_shift_us;
        }
        if (shift_us < -maximum_shift_us) {
            shift_us = -maximum_shift_us;
        }

        if (fabs(shift_us) >= 1.0) {
            int64_t nanoseconds = (int64_t)llround(fuselage_pacing_state.applied_us * 1000.0);
            if (dice_timer_retune(fuselage_pacing_state.simulation_timer, nanoseconds,
                                  (int64_t)llround(shift_us * 1000.0))) {
                fuselage_pacing_state.trace_shift_sum += shift_us;
                frame_shift_us = shift_us;
            } else {
                fuselage_pacing_unlock("phase retune failed");
            }
        }
    }
    if (fuselage_pacing_state.trace_frames && fuselage_pacing_state.locked) {
        fprintf(stderr,
                "[pacing.f] %.4f consumed=%llu lag=%.0f lockwait=%.0f prep=%.0f shift=%+.0f period=%.1f\n",
                (double)sample_us / 1e6, (unsigned long long)consumed,
                (sample_us >= fuselage_pacing_state.last_tick_end_us
                     ? (double)(sample_us - fuselage_pacing_state.last_tick_end_us)
                     : 0.0),
                lock_wait_us, prepare_us, frame_shift_us, fuselage_pacing_state.applied_us);
    }

    if (fuselage_pacing_state.trace && sample_us - fuselage_pacing_state.trace_start_us >= 1000000u) {
        fuselage_pacing_trace_second(sample_us);
    }

    return;
}
