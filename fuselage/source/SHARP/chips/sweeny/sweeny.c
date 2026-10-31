// Sweeny instances and source adapters. SEE: (SHARP.md, Sweeny synthesis)
#include "sweeny_private.h"
#include <math.h>
#include <string.h>

void InitSweeny(Sweeny* chip, double device_rate) {
    memset(chip, 0, sizeof *chip);
    chip->device_rate = device_rate;
    chip->r_master_volume = 255;

    for (int filter_index = 0; filter_index < SWEENY_FILTERS; filter_index++) {
        chip->filter[filter_index].r_cutoff = 65535;
        chip->filter[filter_index].r_resonance = 0;
        chip->filter[filter_index].r_mode = SWEENY_FILT_LP;
    }

    for (int voice_index = 0; voice_index < SWEENY_VOICES; voice_index++) {
        SweenyVoice* voice = &chip->voices[voice_index];
        voice->r_pulse_width = 32768;
        voice->r_volume = 255;
        voice->r_pan = 128;
        voice->r_env_ad = 0x0000;
        voice->r_env_sr = 0xFF00;

        voice->noise_lfsr = 0x7FFFF8u ^ (uint32_t)(voice_index * 0x9E3779B9u);
        if (voice->noise_lfsr == 0) {
            voice->noise_lfsr = 1;
        }
    }

    return;
}

int GetSweenyActiveVoiceCount(const Sweeny* chip) {
    int active_count = 0;
    for (int voice_index = 0; voice_index < SWEENY_VOICES; voice_index++) {
        if (chip->voices[voice_index].active) {
            active_count++;
        }
    }

    return active_count;
}

static void sharp_sweeny_render_callback(void* context, float* output, uint32_t frames) {
    RenderSweeny((Sweeny*)context, output, frames);
}

static void sharp_sweeny_write_callback(void* context, uint16_t reg, uint16_t value) {
    WriteSweenyRegister((Sweeny*)context, reg, value);
}

SharpChip GetSweenySource(Sweeny* chip) {
    SharpChip source;
    source.ctx = chip;
    source.render = sharp_sweeny_render_callback;
    source.write_reg = sharp_sweeny_write_callback;
    source.name = "Sweeny";
    return source;
}
