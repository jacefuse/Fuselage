#ifndef GDMF_SPRITES_COLLISION_PRIVATE_H
#define GDMF_SPRITES_COLLISION_PRIVATE_H

#include "gdmf_sprites_geometry.h"
#include <math.h>

#define GDMF_SPRITES_PAIR_INLINE_PIXELS 32768.0

static inline bool gdmf_sprites_bounds_finite(const gdmf_sprite_bounds* bounds) {
    return isfinite(bounds->minimum_x) && isfinite(bounds->minimum_y) && isfinite(bounds->maximum_x) &&
           isfinite(bounds->maximum_y);
}

// Caller holds state stable; bounds must describe the two named instances.
unsigned char gdmf_sprites_collision_pair(int first_sprite, int second_sprite, unsigned char types,
                                          const gdmf_sprite_bounds* first_bounds,
                                          const gdmf_sprite_bounds* second_bounds);
// Automatic reporting pass, called during renderer preparation under the state lock.
void gdmf_sprites_collisions_run(void);
// Requires excluded callers and completed worker jobs. Frees only search scratch.
void gdmf_sprites_collision_search_shutdown(void);

#endif
