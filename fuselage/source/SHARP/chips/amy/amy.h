#ifndef AMY_H
#define AMY_H
#include <stdint.h>

/* Six-voice PSG with shared noise and envelope generators.
 * SEE: (SHARP.md, Amy registers and synthesis)
 * Public state fields are retained for compatibility; use register writes for controls. */
#include "../../backend/sharp_chip.h"

#define AMY_VOICES 6

#define AMY_NOISE_GENS 2
#define AMY_NOISE_GENS_MAX 8
#define AMY_ENV_GENS 2
#define AMY_ENV_GENS_MAX 8

#define AMY_ROUTE_TO(gen) ((uint16_t)((gen) + 1))
#define AMY_ROUTE_NONE 0

#define AMY_REG_TONE_PERIOD 0
#define AMY_REG_VOLUME 1
#define AMY_REG_PAN 2
#define AMY_REG_CONTROL 3
#define AMY_REG_NOISE_ROUTE 4
#define AMY_REG_ENV_ROUTE 5

#define AMY_REG_STRIDE 32
#define AMY_REG(voice, field) ((uint16_t)((voice) * AMY_REG_STRIDE + (field)))

#define AMY_REG_GLOBAL (AMY_VOICES * AMY_REG_STRIDE)

#define AMY_NOISE_STRIDE 8
#define AMY_NREG_PERIOD 0
#define AMY_NREG_MODE 1
#define AMY_NREG_LENGTH 2
#define AMY_NREG_SOURCE 3
#define AMY_REG_NOISE(g, field) ((uint16_t)(AMY_REG_GLOBAL + (g) * AMY_NOISE_STRIDE + (field)))

#define AMY_ENV_STRIDE 8
#define AMY_EREG_PERIOD_HI 0
#define AMY_EREG_PERIOD_LO 1
#define AMY_EREG_SHAPE 2
#define AMY_REG_ENV(e, field)                                                                                \
    ((uint16_t)(AMY_REG_GLOBAL + AMY_NOISE_GENS_MAX * AMY_NOISE_STRIDE + (e) * AMY_ENV_STRIDE + (field)))

#define AMY_REG_BANKS_END                                                                                    \
    ((uint16_t)(AMY_REG_GLOBAL + AMY_NOISE_GENS_MAX * AMY_NOISE_STRIDE + AMY_ENV_GENS_MAX * AMY_ENV_STRIDE))

#define AMY_REG_CLOCK_HI (AMY_REG_BANKS_END + 0)
#define AMY_REG_CLOCK_LO (AMY_REG_BANKS_END + 1)
#define AMY_REG_VOLUME_TABLE (AMY_REG_BANKS_END + 2)
#define AMY_REG_MASTER_VOLUME (AMY_REG_BANKS_END + 3)

#define AMY_CTL_TONE 0x0001u

#define AMY_CTL_FINE_VOLUME 0x0002u

#define AMY_NOISE_WHITE_AY 0
#define AMY_NOISE_WHITE_SN 1
#define AMY_NOISE_PERIODIC 2
#define AMY_NOISE_WHITE_SEGA 3

#define AMY_NOISE_LENGTH_SN 15

#define AMY_NOISE_SOURCE_OWN 0

#define AMY_ENV_HOLD 0x01u
#define AMY_ENV_ALTERNATE 0x02u
#define AMY_ENV_ATTACK 0x04u
#define AMY_ENV_CONTINUE 0x08u

#define AMY_VOL_AY8910 0
#define AMY_VOL_YM2149 1
#define AMY_VOL_LINEAR_2DB 2

#define AMY_VOL_AY8910_REAL 3

#define AMY_VOL_YM2149_REAL 4

#define AMY_VOL_LEVELS 32

#define AMY_VOICE_SCALE (1.0f / (float)AMY_VOICES)

#define AMY_OVERSAMPLE 8

#define AMY_CLOCK_DEFAULT 1773400u

typedef struct {

    uint16_t r_tone_period;
    uint16_t r_volume;
    uint16_t r_pan;
    uint16_t r_control;
    uint16_t r_noise_route;
    uint16_t r_env_route;

    double count;
    int high;
    int clocked;

    float lg, rg;
} AmyVoice;

typedef struct {
    uint16_t r_period;
    uint16_t r_mode;
    uint16_t r_length;
    uint16_t r_source;

    uint32_t lfsr;
    double count;
    int half;
    int out;
} AmyNoise;

typedef struct {
    uint16_t r_period_hi;
    uint16_t r_period_lo;
    uint16_t r_shape;

    double acc;
    double step_len;
    double pending_len;
    int level;
    int rising;
    int holding;
} AmyEnv;

typedef struct {
    AmyVoice voices[AMY_VOICES];
    AmyNoise noise[AMY_NOISE_GENS];
    AmyEnv env[AMY_ENV_GENS];

    uint16_t r_clock_hi, r_clock_lo;
    uint16_t r_volume_table;
    uint16_t r_master_volume;

    double clock;
    double device_rate;

    float dc_x[2], dc_y[2];
} Amy;

/* Initialization is silent. Use a finite positive device rate. */
void InitAmy(Amy* chip, double device_rate);

/* Source callbacks retain the caller-owned chip. */
SharpChip GetAmySource(Amy* chip);

/* Reserved fields are ignored; writing shape always restarts its envelope. */
void WriteAmyRegister(Amy* chip, uint16_t reg, uint16_t value);

/* Add interleaved stereo frames to caller-owned output. */
void RenderAmy(Amy* chip, float* output, uint32_t frames);

/* Legacy control-state estimate, not a reliable count of audible voices.
 * SEE: (SHARP.md, Amy activity reporting) */
int GetAmyActiveVoiceCount(const Amy* chip);

#endif
