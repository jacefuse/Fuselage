#ifndef ADORA_PRIVATE_H
#define ADORA_PRIVATE_H
#include "adora.h"

void sharp_adora_configure_envelope(Adora* chip, AdoraVoice* voice);
void sharp_adora_set_voice_gains(AdoraVoice* voice);
void sharp_adora_update_step(Adora* chip, AdoraVoice* voice);
void sharp_adora_trigger_voice(Adora* chip, AdoraVoice* voice);
#endif
