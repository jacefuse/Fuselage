// Sweeny subtractive synthesis. SEE: (SHARP.md, Sweeny synthesis)
#ifndef SWEENY_H
#define SWEENY_H
#include <stdint.h>
#include "../../backend/sharp_chip.h"

#define SWEENY_VOICES 8

#define SWEENY_FILTERS 4
#define SWEENY_FILTERS_MAX 8

#define SWEENY_REG_FREQ_HI 0
#define SWEENY_REG_FREQ_LO 1
#define SWEENY_REG_PULSE_WIDTH 2
#define SWEENY_REG_WAVEFORM 3
#define SWEENY_REG_GATE 4
#define SWEENY_REG_VOLUME 5
#define SWEENY_REG_PAN 6
#define SWEENY_REG_ENV_AD 7
#define SWEENY_REG_ENV_SR 8
#define SWEENY_REG_CONTROL 9
#define SWEENY_REG_FILTER_ROUTE 10

#define SWEENY_REG_STRIDE 32
#define SWEENY_REG(voice, field) ((uint16_t)((voice) * SWEENY_REG_STRIDE + (field)))

#define SWEENY_REG_GLOBAL (SWEENY_VOICES * SWEENY_REG_STRIDE)

#define SWEENY_FILTER_STRIDE 8
#define SWEENY_FREG_CUTOFF 0
#define SWEENY_FREG_RES 1
#define SWEENY_FREG_MODE 2

#define SWEENY_REG_FILTER(f, field) ((uint16_t)(SWEENY_REG_GLOBAL + (f) * SWEENY_FILTER_STRIDE + (field)))

#define SWEENY_REG_MASTER_VOLUME ((uint16_t)(SWEENY_REG_GLOBAL + SWEENY_FILTERS_MAX * SWEENY_FILTER_STRIDE))

#define SWEENY_WAVE_TRIANGLE 0
#define SWEENY_WAVE_SAW 1
#define SWEENY_WAVE_PULSE 2
#define SWEENY_WAVE_NOISE 3

#define SWEENY_CTL_SYNC 0x0001u
#define SWEENY_CTL_RING 0x0002u
#define SWEENY_CTL_BANDLIMIT 0x0004u
#define SWEENY_CTL_TEST 0x0008u

#define SWEENY_FILT_LP 0x0001u
#define SWEENY_FILT_BP 0x0002u
#define SWEENY_FILT_HP 0x0004u

#define SWEENY_FREQ_ONE 65536u

#define SWEENY_ENV_IDLE 0
#define SWEENY_ENV_ATTACK 1
#define SWEENY_ENV_DECAY 2
#define SWEENY_ENV_SUSTAIN 3
#define SWEENY_ENV_RELEASE 4

typedef struct {
    // Voice registers and oscillator/envelope state.
    uint32_t r_freq;
    uint16_t r_pulse_width;
    uint16_t r_waveform;
    uint16_t r_volume;
    uint16_t r_pan;
    uint16_t r_env_ad;
    uint16_t r_env_sr;
    uint16_t r_control;
    uint16_t r_filter_route;

    double phase;
    double step;
    double noise_acc;
    uint32_t noise_lfsr;
    float noise_out;
    float last_out;
    int wrapped;

    int env_stage;
    float env_level;
    float env_a_inc, env_d_coef, env_r_coef, env_sustain;

    int active;
    float lg, rg;
} SweenyVoice;

typedef struct {
    uint16_t r_cutoff;
    uint16_t r_resonance;
    uint16_t r_mode;
    float ic1[2], ic2[2];
} SweenyFilter;

typedef struct {
    SweenyVoice voices[SWEENY_VOICES];
    SweenyFilter filter[SWEENY_FILTERS];
    uint16_t r_master_volume;
    double device_rate;
} Sweeny;

// Use a finite positive device rate. SEE: (SHARP.md, Sweeny synthesis)
void InitSweeny(Sweeny* chip, double device_rate);

SharpChip GetSweenySource(Sweeny* chip);

void WriteSweenyRegister(Sweeny* chip, uint16_t reg, uint16_t value);

// Add interleaved stereo output; the caller clears the destination.
void RenderSweeny(Sweeny* chip, float* output, uint32_t frames);

int GetSweenyActiveVoiceCount(const Sweeny* chip);

#endif
