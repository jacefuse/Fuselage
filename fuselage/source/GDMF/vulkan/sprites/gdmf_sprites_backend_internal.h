#ifndef GDMF_SPRITES_BACKEND_INTERNAL_H
#define GDMF_SPRITES_BACKEND_INTERNAL_H

#include "../../sprites/gdmf_sprites_backend.h"

// Status polling is atomic; resource access still requires excluded lifecycle
// users or the drawing-state lock. SEE: (GDMF_SPRITES.md, Backend lifecycle)
bool gdmf_sprites_backend_healthy(void);
bool gdmf_sprites_backend_has_resources(void);
// Only after verified completion AND discarded recorded command references.
void gdmf_sprites_backend_note_quiesced(void);

#endif
