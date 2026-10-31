#include "gdmf_sprites_help.h"

bool OffsetSpritePosition(int sprite_index, float offset_x, float offset_y) {
    return SetSpritePosition(sprite_index, GetSpriteX(sprite_index) + offset_x,
                             GetSpriteY(sprite_index) + offset_y);
}

bool OffsetSpriteScale(int sprite_index, float scale_offset) {
    return SetSpriteScale(sprite_index, GetSpriteScale(sprite_index) + scale_offset);
}

bool OffsetSpriteRotation(int sprite_index, float rotation_offset) {
    return SetSpriteRotation(sprite_index, GetSpriteRotation(sprite_index) + rotation_offset);
}
