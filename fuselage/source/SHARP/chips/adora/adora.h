// Adora sample playback. SEE: (SHARP.md, Adora sample playback)
#ifndef ADORA_H
#define ADORA_H
#include <stdint.h>
#include <limits.h>
#include "../../backend/sharp_chip.h"

#define ADORA_VOICES 16

#define ADORA_REG_OFFSET_HI 0
#define ADORA_REG_OFFSET_LO 1
#define ADORA_REG_LENGTH_HI 2
#define ADORA_REG_LENGTH_LO 3
#define ADORA_REG_PITCH 4
#define ADORA_REG_VOLUME 5
#define ADORA_REG_PAN 6
#define ADORA_REG_TRIGGER 7
#define ADORA_REG_LOOP_START_HI 8
#define ADORA_REG_LOOP_START_LO 9
#define ADORA_REG_LOOP_LEN_HI 10
#define ADORA_REG_LOOP_LEN_LO 11
#define ADORA_REG_LOOP_MODE 12
#define ADORA_REG_FORMAT 13
#define ADORA_REG_ENV_AD 14
#define ADORA_REG_ENV_SR 15
#define ADORA_REG_BASE_RATE 16

#define ADORA_REG_STRIDE 32
#define ADORA_REG(voice, field) ((uint16_t)((voice) * ADORA_REG_STRIDE + (field)))

#define ADORA_LOOP_OFF 0
#define ADORA_LOOP_FORWARD 1
#define ADORA_LOOP_PINGPONG 2

#define ADORA_FMT_U8 0
#define ADORA_FMT_S8 1
#define ADORA_FMT_S16LE 2
#define ADORA_FMT_S16BE 3

#define ADORA_ENV_IDLE 0
#define ADORA_ENV_ATTACK 1
#define ADORA_ENV_DECAY 2
#define ADORA_ENV_SUSTAIN 3
#define ADORA_ENV_RELEASE 4

#define ADORA_PITCH_ONE 4096

#ifndef ADORA_DECLICK
#define ADORA_DECLICK 48
#endif
#ifndef ADORA_GAIN_RAMP_MS

#define ADORA_GAIN_RAMP_MS 0.67
#endif

#if ADORA_DECLICK < 1 || ADORA_DECLICK > INT_MAX
#error ADORA_DECLICK must fit a positive int
#endif

typedef struct {
    // Register state; lengths and offsets are bytes.
    uint32_t r_offset;
    uint32_t r_length;
    uint16_t r_pitch;
    uint16_t r_base_rate;
    uint16_t r_volume;
    uint16_t r_pan;
    uint32_t r_loop_start;
    uint32_t r_loop_len;
    uint16_t r_loop_mode;
    uint16_t r_format;
    uint16_t r_env_ad;
    uint16_t r_env_sr;

    // Envelope and transition state.
    int env_stage;
    float env_level;
    float env_a_inc, env_d_inc, env_r_inc, env_sustain;

    float last_out;
    int dc_n;

    float last_l, last_r;
    float dc_l, dc_r;
    int fading;
    float fade_level;

    // Playback positions and lengths are samples.
    int active;
    double pos;
    double step;
    float lg, rg;
    float cur_lg, cur_rg;
    const uint8_t* sample;
    int fmt;
    int bps;
    uint32_t nsamp;
    uint32_t loop_start;
    uint32_t loop_len;
    int loop_mode;
    int in_loop;
    int reverse;
} AdoraVoice;

typedef struct {
    AdoraVoice voices[ADORA_VOICES];
    const uint8_t* pool;
    uint32_t pool_size;
    double device_rate;
    double sample_rate;
    float gain_k;
} Adora;

// The pool is borrowed. SEE: (SHARP.md, Adora sample playback)
void InitAdora(Adora* chip, double device_rate, double sample_rate, const uint8_t* pool, uint32_t pool_size);

SharpChip GetAdoraSource(Adora* chip);

void WriteAdoraRegister(Adora* chip, uint16_t reg, uint16_t value);

// Add interleaved stereo samples; the caller clears the destination.
void RenderAdora(Adora* chip, float* output, uint32_t frames);

int GetAdoraActiveVoiceCount(const Adora* chip);

// Selected voice index or -1. SEE: (SHARP.md, Adora note helper)
int PlayAdoraNote(Adora* chip, uint32_t offset, uint32_t length, double pitch_ratio, float gain, float pan);

#endif
