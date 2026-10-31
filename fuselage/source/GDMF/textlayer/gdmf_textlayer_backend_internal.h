#ifndef GDMF_TEXTLAYER_BACKEND_INTERNAL_H
#define GDMF_TEXTLAYER_BACKEND_INTERNAL_H
// Engine-owned renderer cleanup, excluded from the public tl interface.
#include <stdbool.h>
bool gdmf_textlayer_shutdown(void);
bool gdmf_textlayer_backend_healthy(void);
bool gdmf_textlayer_backend_has_resources(void);
// Coordinator only, after GPU completion and command-reference discard.
void gdmf_textlayer_backend_note_quiesced(void);
#endif
