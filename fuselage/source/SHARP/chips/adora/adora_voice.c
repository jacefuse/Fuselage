// Adora voice. SEE: (SHARP.md, Adora sample playback)
#include "adora_private.h"
#include <math.h>
#include <string.h>

static float sharp_adora_envelope_increment(unsigned rate_value, double rate, float span) {
    if (rate_value == 0 || span <= 0.0f) {
        return 1.0e9f;
    }
    double fraction = (double)rate_value / 255.0;
    double seconds = fraction * fraction * 4.0;
    double duration_frames = seconds * rate;
    if (duration_frames < 1.0) {
        return 1.0e9f;
    }
    return (float)((double)span / duration_frames);
}

void sharp_adora_configure_envelope(Adora* chip, AdoraVoice* voice) {
    unsigned attack = (voice->r_env_ad >> 8) & 0xFF, decay = voice->r_env_ad & 0xFF;
    unsigned sustain = (voice->r_env_sr >> 8) & 0xFF, release = voice->r_env_sr & 0xFF;
    voice->env_sustain = (float)sustain / 255.0f;
    voice->env_a_inc = sharp_adora_envelope_increment(attack, chip->device_rate, 1.0f);
    voice->env_d_inc = sharp_adora_envelope_increment(decay, chip->device_rate, 1.0f - voice->env_sustain);
    voice->env_r_inc = sharp_adora_envelope_increment(release, chip->device_rate, 1.0f);
}

void sharp_adora_set_voice_gains(AdoraVoice* voice) {
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
}

static double sharp_adora_get_voice_step(const Adora* chip, const AdoraVoice* voice) {
    const double base = (voice->r_base_rate != 0u) ? (double)voice->r_base_rate : chip->sample_rate;

    return (base / chip->device_rate) * ((double)voice->r_pitch / (double)ADORA_PITCH_ONE);
}

void sharp_adora_update_step(Adora* chip, AdoraVoice* voice) {
    if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0) {
        return;
    }
    double step = sharp_adora_get_voice_step(chip, voice);
    if (isfinite(step) && step >= 0.0) {
        voice->step = step;
    }
}

void sharp_adora_trigger_voice(Adora* chip, AdoraVoice* voice) {
    if (!chip->pool || voice->r_length == 0) {
        return;
    }
    if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0 || !isfinite(chip->sample_rate) ||
        chip->sample_rate <= 0.0) {
        return;
    }
    double step = sharp_adora_get_voice_step(chip, voice);
    if (!isfinite(step) || step < 0.0) {
        return;
    }
    uint32_t sample_offset = voice->r_offset, sample_length = voice->r_length;
    if (sample_offset >= chip->pool_size) {
        return;
    }
    if (sample_length > chip->pool_size - sample_offset) {
        sample_length = chip->pool_size - sample_offset;
    }

    int format = (int)voice->r_format;
    int bytes_per_sample = (format == ADORA_FMT_S16LE || format == ADORA_FMT_S16BE) ? 2 : 1;
    uint32_t sample_count = sample_length / (uint32_t)bytes_per_sample;
    if (sample_count == 0) {
        return;
    }
    voice->fmt = format;
    voice->bps = bytes_per_sample;

    voice->sample = chip->pool + sample_offset;
    voice->nsamp = sample_count;
    voice->pos = 0.0;

    voice->loop_mode = voice->r_loop_mode;
    voice->loop_start = voice->r_loop_start / (uint32_t)voice->bps;
    voice->loop_len = voice->r_loop_len / (uint32_t)voice->bps;
    if (voice->loop_start >= voice->nsamp) {
        voice->loop_mode = ADORA_LOOP_OFF;
        voice->loop_len = 0;
    } else if (voice->loop_len > voice->nsamp - voice->loop_start) {
        voice->loop_len = voice->nsamp - voice->loop_start;
    }
    if (voice->loop_len < 2) {
        voice->loop_mode = ADORA_LOOP_OFF;
    }
    voice->in_loop = 0;
    voice->reverse = 0;
    voice->step = step;
    sharp_adora_set_voice_gains(voice);

    sharp_adora_configure_envelope(chip, voice);
    voice->env_level = 0.0f;
    voice->env_stage = ADORA_ENV_ATTACK;

    const int was_active = voice->active;
    voice->dc_l = was_active ? voice->last_l : 0.0f;
    voice->dc_r = was_active ? voice->last_r : 0.0f;
    voice->dc_n = ADORA_DECLICK;
    voice->fading = 0;

    voice->cur_lg = voice->lg;
    voice->cur_rg = voice->rg;

    voice->active = 1;
}
