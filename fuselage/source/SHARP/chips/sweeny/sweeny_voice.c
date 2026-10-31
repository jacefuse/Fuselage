// Sweeny voice. SEE: (SHARP.md, Sweeny synthesis)
#include "sweeny_private.h"
#include <math.h>
#include <string.h>

static double sharp_sweeny_envelope_seconds(unsigned rate_value) {
    double fraction = (double)rate_value / 255.0;
    return fraction * fraction * 4.0;
}

static float sharp_sweeny_attack_increment(unsigned rate_value, double rate) {
    double seconds = sharp_sweeny_envelope_seconds(rate_value);
    double duration_frames = seconds * rate;
    if (rate_value == 0 || duration_frames < 1.0) {
        return 1.0e9f;
    }
    return (float)(1.0 / duration_frames);
}

static float sharp_sweeny_exponential_coefficient(unsigned rate_value, double rate) {
    double seconds = sharp_sweeny_envelope_seconds(rate_value);
    double duration_frames = seconds * rate;
    if (rate_value == 0 || duration_frames < 1.0) {
        return 1.0f;
    }
    double time_constant = duration_frames / 5.0;
    return (float)(1.0 - exp(-1.0 / time_constant));
}

void sharp_sweeny_configure_envelope(Sweeny* chip, SweenyVoice* voice) {
    unsigned attack = (voice->r_env_ad >> 8) & 0xFF, decay = voice->r_env_ad & 0xFF;
    unsigned sustain = (voice->r_env_sr >> 8) & 0xFF, release = voice->r_env_sr & 0xFF;
    voice->env_sustain = (float)sustain / 255.0f;
    voice->env_a_inc = sharp_sweeny_attack_increment(attack, chip->device_rate);
    voice->env_d_coef = sharp_sweeny_exponential_coefficient(decay, chip->device_rate);
    voice->env_r_coef = sharp_sweeny_exponential_coefficient(release, chip->device_rate);

    return;
}

void sharp_sweeny_set_voice_gains(SweenyVoice* voice) {
    float pan = ((float)voice->r_pan - 128.0f) / 127.0f;
    if (pan < -1.0f) {
        pan = -1.0f;
    }
    if (pan > 1.0f) {
        pan = 1.0f;
    }
    float gain = (float)voice->r_volume / 255.0f;
    float angle = (pan * 0.5f + 0.5f) * 1.57079632679f;
    voice->lg = cosf(angle) * gain;
    voice->rg = sinf(angle) * gain;

    return;
}

void sharp_sweeny_refresh_step(Sweeny* chip, SweenyVoice* voice) {
    if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0) {
        return;
    }
    double hz = (double)voice->r_freq / (double)SWEENY_FREQ_ONE;
    double step = hz / chip->device_rate;
    if (isfinite(step) && step >= 0.0) {
        voice->step = step;
    }

    return;
}
