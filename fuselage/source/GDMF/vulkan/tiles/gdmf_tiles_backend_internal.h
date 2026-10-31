#ifndef GDMF_TILES_BACKEND_INTERNAL_H
#define GDMF_TILES_BACKEND_INTERNAL_H
#include <stdbool.h>

// Renderer completion includes discarding recorded command references.
bool gdmf_tiles_backend_healthy(void);
bool gdmf_tiles_backend_has_resources(void);
void gdmf_tiles_backend_note_quiesced(void);
bool gdmf_tiles_backend_shutdown(void);
#endif
