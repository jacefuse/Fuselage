#ifndef GDMF_SPRITES_BACKEND_H
#define GDMF_SPRITES_BACKEND_H

#include <stdbool.h>

// Renderer services. SEE: (GDMF_SPRITES.md, Backend lifecycle)
// Initialization attempts atlas/pipeline setup; false does not discard host state.
// Caller excludes render/worker users; repeated host init reuses existing GPU resources.
bool gdmf_sprites_backend_initialize(void);
// Requires verified GPU quiescence and excluded users; false retains ownership.
// Does not wait, reset host attributes or erase bitmap data.
bool gdmf_sprites_backend_shutdown(void);

#endif
