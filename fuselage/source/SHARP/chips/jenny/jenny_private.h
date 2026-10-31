#ifndef SHARP_JENNY_PRIVATE_H
#define SHARP_JENNY_PRIVATE_H
#include "jenny.h"
#include <stddef.h>

#define SHARP_JENNY_EG_SILENT 96.0

void sharp_jenny_commit_operator(JennyOp* operator_state);
void sharp_jenny_commit_voice(JennyVoice* voice);
void sharp_jenny_write_key(JennyVoice* voice, uint16_t value);
#endif
