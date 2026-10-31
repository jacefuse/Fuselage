#ifndef GDMF_PIXIES_BACKEND_INTERNAL_H
#define GDMF_PIXIES_BACKEND_INTERNAL_H

#include "../../pixies/gdmf_pixies_backend.h"

// Only the renderer calls this after successful GPU completion and command-pool
// reset, with producers excluded. It invalidates recorded snapshot publication.
void gdmf_pixies_backend_note_quiesced(void);
bool gdmf_pixies_backend_healthy(void);
bool gdmf_pixies_backend_has_resources(void);

#endif
