#ifndef GDMF_SPRITES_GEOMETRY_H
#define GDMF_SPRITES_GEOMETRY_H

#include "gdmf_sprites_state_private.h"

typedef struct {
    float minimum_x, minimum_y, maximum_x, maximum_y;
} gdmf_sprite_bounds;

// Corner order: top-left, top-right, bottom-left, bottom-right, before projection.
// SEE: (GDMF_SPRITES.md, Transforms)
void gdmf_sprites_world_quad(const Sprite* sprite, float output_x[4], float output_y[4]);
// Borrowed cached bounds, or NULL on invalid index. State must remain serialized.
const gdmf_sprite_bounds* gdmf_sprites_bounds_get(int sprite_index);

#endif
