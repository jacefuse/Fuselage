/* Root pacing. SEE: (FUSELAGE_PACING.md, Controller) */
#include "fuselage_pacing_internal.h"
#include "../../DICE/dice.h"
#include "../../DICE/timers/dice_timers_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
bool fuselage_pacing_apply_period(double period_us) {
    double requested = period_us * 1000.0;
    if (isfinite(requested) && requested >= 1 && requested < 0x1p63) {
        int64_t nanoseconds = (int64_t)llround(requested);
        bool simulation = dice_timer_retune(fuselage_pacing_state.simulation_timer, nanoseconds, 0);
        bool waiter = dice_timer_retune(fuselage_pacing_state.wait_timer, nanoseconds, 0);
        if (simulation && waiter) {
            fuselage_pacing_state.applied_us = period_us;
            return true;
        }
    }
    double fallback = fuselage_pacing_state.declared_us * 1000.0;
    if (isfinite(fallback) && fallback >= 1 && fallback < 0x1p63) {
        (void)dice_timer_retune(fuselage_pacing_state.simulation_timer, (int64_t)llround(fallback), 0);
        (void)dice_timer_retune(fuselage_pacing_state.wait_timer, (int64_t)llround(fallback), 0);
    }
    fuselage_pacing_state.retune_failed = true;
    fuselage_pacing_state.enabled = fuselage_pacing_state.locked = false;
    fuselage_pacing_state.applied_us = 0;
    fuselage_pacing_state.ratio = 0;
    return false;
}

void fuselage_pacing_unlock(const char* why) {
    if (fuselage_pacing_state.locked && fuselage_pacing_state.trace) {
        fprintf(stderr, "[pacing] %.3fs UNLOCK (%s) -- request declared %.1f us\n",
                (double)DICE_GetElapsedMicroseconds() / 1e6, why, fuselage_pacing_state.declared_us);
    }
    if (fuselage_pacing_state.locked || fuselage_pacing_state.applied_us != 0.0) {
        fuselage_pacing_apply_period(fuselage_pacing_state.declared_us);
        fuselage_pacing_state.applied_us = 0.0;
    }
    fuselage_pacing_state.locked = false;
    fuselage_pacing_state.ratio = 0;
    fuselage_pacing_state.target_us = 0.0;

    return;
}

void fuselage_pacing_reset_estimator(const char* why) {
    fuselage_pacing_unlock(why);
    fuselage_pacing_state.ring_head = fuselage_pacing_state.ring_count = 0;
    fuselage_pacing_state.refresh_index = 0;
    fuselage_pacing_state.refresh_us = 0.0;
    fuselage_pacing_state.good_intervals = fuselage_pacing_state.bad_run = 0;
    fuselage_pacing_state.bootstrap_count = 0;
    fuselage_pacing_state.last_error = 1.0;

    return;
}

int fuselage_pacing_cmp_double(const void* a, const void* b) {
    double left = *(const double*)a, right = *(const double*)b;

    return (left > right) - (left < right);
}

double fuselage_pacing_evaluate(int* ratio_output, double* target_output) {
    double ratio = fuselage_pacing_state.refresh_us / fuselage_pacing_state.declared_us;
    if (!isfinite(ratio) || ratio <= 0) {
        *ratio_output = 0;
        *target_output = 0;
        return INFINITY;
    }
    int ratio_multiple;
    double effective_ratio;

    if (ratio >= 1.0) {
        ratio_multiple = ratio >= FUSELAGE_PACING_MAX_RATIO ? FUSELAGE_PACING_MAX_RATIO : (int)lround(ratio);
        if (ratio_multiple < 1) {
            ratio_multiple = 1;
        }
        if (ratio_multiple > FUSELAGE_PACING_MAX_RATIO) {
            ratio_multiple = FUSELAGE_PACING_MAX_RATIO;
        }
        effective_ratio = ratio / (double)ratio_multiple;
        *ratio_output = ratio_multiple;
        *target_output = fuselage_pacing_state.refresh_us / (double)ratio_multiple;
    } else {
        double inverse = 1.0 / ratio;
        ratio_multiple =
            inverse >= FUSELAGE_PACING_MAX_RATIO ? FUSELAGE_PACING_MAX_RATIO : (int)lround(inverse);
        if (ratio_multiple < 1) {
            ratio_multiple = 1;
        }
        if (ratio_multiple > FUSELAGE_PACING_MAX_RATIO) {
            ratio_multiple = FUSELAGE_PACING_MAX_RATIO;
        }
        effective_ratio = (1.0 / ratio) / (double)ratio_multiple;
        *ratio_output = (ratio_multiple == 1) ? 1 : -ratio_multiple;
        *target_output = fuselage_pacing_state.refresh_us * (double)ratio_multiple;
    }

    return fabs(effective_ratio - 1.0);
}
