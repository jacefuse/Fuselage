#ifndef SHARP_AMY_PRIVATE_H
#define SHARP_AMY_PRIVATE_H
#include "amy.h"
#include <stddef.h>

/* Control helpers shared by Amy's implementation files. */
void sharp_amy_ensure_volume_tables(void);
const float* sharp_amy_get_volume_table(unsigned index);
void sharp_amy_set_voice_gains(AmyVoice* voice);
void sharp_amy_refresh_envelope_step(Amy* chip, AmyEnv* envelope);
void sharp_amy_refresh_envelope_steps(Amy* chip);
void sharp_amy_restart_envelope(AmyEnv* envelope);
#endif
