#include "amy_private.h"
#include <math.h>

void sharp_amy_refresh_envelope_step(Amy* chip, AmyEnv* envelope) {
    if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0) {
        return;
    }
    unsigned period = ((unsigned)envelope->r_period_hi << 16) | envelope->r_period_lo;

    const double effective_period = period ? (double)period : 0.5;
    const double cycle_hz = chip->clock / (256.0 * effective_period);
    const double steps_per_second = cycle_hz * (double)AMY_VOL_LEVELS;

    /* A period write changes the next step; the running step keeps its length. */
    envelope->pending_len = chip->device_rate / steps_per_second;
    if (envelope->step_len <= 0.0) {
        envelope->step_len = envelope->pending_len;
    }

    return;
}

void sharp_amy_refresh_envelope_steps(Amy* chip) {
    for (int generator_index = 0; generator_index < AMY_ENV_GENS; generator_index++) {
        sharp_amy_refresh_envelope_step(chip, &chip->env[generator_index]);
    }

    return;
}

void sharp_amy_restart_envelope(AmyEnv* envelope) {
    envelope->rising = (envelope->r_shape & AMY_ENV_ATTACK) ? 1 : 0;
    envelope->level = envelope->rising ? 0 : (AMY_VOL_LEVELS - 1);
    envelope->holding = 0;
    envelope->acc = 0.0;
    envelope->step_len = envelope->pending_len;

    return;
}
