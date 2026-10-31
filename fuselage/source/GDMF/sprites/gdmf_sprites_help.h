#ifndef GDMF_SPRITES_HELP_H
#define GDMF_SPRITES_HELP_H

#include "gdmf_sprites.h"

// Add offsets through the absolute setters. False on invalid index.
// SEE: (GDMF_SPRITES.md, Relative helpers)
bool OffsetSpritePosition(int sprite_index, float offset_x, float offset_y);
// Adds to scale; does not multiply. False if the resulting scale is <= 0.
bool OffsetSpriteScale(int sprite_index, float scale_offset);
// Adds degrees to the current rotation.
bool OffsetSpriteRotation(int sprite_index, float rotation_offset);

#endif // GDMF_SPRITES_HELP_H
