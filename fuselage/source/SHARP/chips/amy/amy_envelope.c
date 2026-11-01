#include "amy_private.h"
#include <math.h>

void sharp_amy_refresh_envelope_step(Amy* chip, AmyEnv* envelope) {
    if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0) {
        return;
    }
    unsigned period = ((unsigned)envelope->r_period_hi << 16) | envelope->r_period_lo;

    /* Period zero acts as one, as it does for tone and noise. */
    const double effective_period = period ? (double)period : 1.0;
    const double cycle_hz = chip->clock / (256.0 * effective_period);
    const double steps_per_second = cycle_hz * (double)AMY_VOL_LEVELS;

    /* A period or clock write retimes the running step at once: the counter
     * compares against the new period on its next tick, so a step already owed
     * fires once at the next sample rather than as a burst. */
    envelope->pending_len = chip->device_rate / steps_per_second;
    envelope->step_len = envelope->pending_len;
    if (envelope->acc > envelope->step_len) {
        envelope->acc = envelope->step_len;
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
