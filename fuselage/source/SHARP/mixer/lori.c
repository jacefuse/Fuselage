// Stereo balance, bus fold and master clipping. SEE: (SHARP.md, LORI composition interface)
#include "lori.h"
#include <math.h>
#include <string.h>

void InitLori(Lori* mixer, float master_gain) {
    memset(mixer, 0, sizeof *mixer);
    mixer->master_gain = master_gain;
    for (int bus_index = 0; bus_index < LORI_BUSES; ++bus_index) {
        mixer->bus[bus_index].volume = 1.0f;
        mixer->bus[bus_index].balance = 0.0f;
        mixer->bus[bus_index].enabled = 0;
    }
}

void SetLoriMasterGain(Lori* mixer, float gain) {
    mixer->master_gain = gain;
}

int EnableLoriBus(Lori* mixer, int bus, int enabled) {
    if (bus < 0 || bus >= LORI_BUSES) {
        return 0;
    }
    mixer->bus[bus].enabled = enabled ? 1 : 0;
    return 1;
}

int SetLoriBusVolume(Lori* mixer, int bus, float volume) {
    if (bus < 0 || bus >= LORI_BUSES) {
        return 0;
    }
    if (volume < 0.0f) {
        volume = 0.0f;
    }
    mixer->bus[bus].volume = volume;
    return 1;
}

int SetLoriBusBalance(Lori* mixer, int bus, float balance) {
    if (bus < 0 || bus >= LORI_BUSES) {
        return 0;
    }
    if (balance < -1.0f) {
        balance = -1.0f;
    }
    if (balance > 1.0f) {
        balance = 1.0f;
    }
    mixer->bus[bus].balance = balance;
    return 1;
}

int IsLoriBusEnabled(const Lori* mixer, int bus) {
    if (bus < 0 || bus >= LORI_BUSES) {
        return 0;
    }
    return mixer->bus[bus].enabled;
}

static void sharp_lori_balance_gains(float balance, float* left_gain, float* right_gain) {
    *left_gain = balance <= 0.0f ? 1.0f : 1.0f - balance;
    *right_gain = balance >= 0.0f ? 1.0f : 1.0f + balance;
}

static void sharp_lori_apply_master(Lori* mixer, float* stereo, uint32_t frames) {
    size_t samples = (size_t)frames * 2;
    for (size_t sample_index = 0; sample_index < samples; ++sample_index) {
        stereo[sample_index] = tanhf(stereo[sample_index] * mixer->master_gain);
    }
}

void ProcessLori(Lori* mixer, float* stereo, uint32_t frames) {
    sharp_lori_apply_master(mixer, stereo, frames);
}

void RenderLori(Lori* mixer, LoriFillFn fill, void* user, float* master, uint32_t frames) {
    float* buses[LORI_BUSES];
    for (int bus_index = 0; bus_index < LORI_BUSES; ++bus_index) {
        buses[bus_index] = mixer->scratch[bus_index];
    }
    uint32_t rendered_frames = 0;
    while (rendered_frames < frames) {
        uint32_t chunk_frames = frames - rendered_frames;
        if (chunk_frames > LORI_BLOCK) {
            chunk_frames = LORI_BLOCK;
        }
        if (fill) {
            fill(user, buses, LORI_BUSES, chunk_frames);
        }
        float* output = master + (size_t)rendered_frames * 2;
        memset(output, 0, (size_t)chunk_frames * 2 * sizeof(float));
        for (int bus_index = 0; bus_index < LORI_BUSES; ++bus_index) {
            if (!mixer->bus[bus_index].enabled) {
                continue;
            }
            float volume = mixer->bus[bus_index].volume;
            if (volume <= 0.0f) {
                continue;
            }
            float left_gain, right_gain;
            sharp_lori_balance_gains(mixer->bus[bus_index].balance, &left_gain, &right_gain);
            left_gain *= volume;
            right_gain *= volume;
            const float* source = mixer->scratch[bus_index];
            for (uint32_t frame_index = 0; frame_index < chunk_frames; ++frame_index) {
                output[frame_index * 2] += source[frame_index * 2] * left_gain;
                output[frame_index * 2 + 1] += source[frame_index * 2 + 1] * right_gain;
            }
        }
        sharp_lori_apply_master(mixer, output, chunk_frames);
        rendered_frames += chunk_frames;
    }
}
