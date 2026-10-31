// Attribute access and bitmap assignment. SEE: (GDMF_SPRITES.md, Attributes)
#include "gdmf_sprites_state_private.h"
#include <stdio.h>

bool AssignSprite(int sprite_index, SpriteBitmapID bitmap_id) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        printf("AssignSprite: sprite %d is out of range.\n", sprite_index);
        return false;
    }
    if (bitmap_id != SPRITE_BITMAP_NONE && !gdmf_sprites_bitmap_id_valid(bitmap_id)) {
        printf("AssignSprite: bitmap ID %d is out of range.\n", bitmap_id);
        return false;
    }

    gdmf_sprites_state_get(sprite_index)->bitmapID = bitmap_id;

    return true;
}

bool AssignSpriteBitmapFromSprite(int source_sprite, int destination_sprite) {
    if (!gdmf_sprites_index_valid(source_sprite)) {
        printf("AssignSpriteBitmapFromSprite: source sprite %d is out of range.\n", source_sprite);
        return false;
    }
    if (!gdmf_sprites_index_valid(destination_sprite)) {
        printf("AssignSpriteBitmapFromSprite: destination sprite %d is out of range.\n", destination_sprite);
        return false;
    }

    gdmf_sprites_state_get(destination_sprite)->bitmapID = gdmf_sprites_state_get(source_sprite)->bitmapID;
    gdmf_sprites_state_get(destination_sprite)->showzero = gdmf_sprites_state_get(source_sprite)->showzero;

    return true;
}

SpriteBitmapID GetSpriteBitmapID(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return SPRITE_BITMAP_NONE;
    }

    return gdmf_sprites_state_get(sprite_index)->bitmapID;
}

bool ClearSprite(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    SetSpriteEnabled(sprite_index, false);
    AssignSprite(sprite_index, SPRITE_BITMAP_NONE);

    return true;
}

bool AssignSpriteTestPattern(int sprite_index) {
    return AssignSprite(sprite_index, SPRITE_TEST_PATTERN_BITMAP_ID);
}

bool GetSpriteEnabled(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    return gdmf_sprites_state_get(sprite_index)->enabled;
}

bool SetSpriteEnabled(int sprite_index, bool enabled) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->enabled = enabled;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool ToggleSpriteEnabled(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->enabled = !gdmf_sprites_state_get(sprite_index)->enabled;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool GetSpriteVisible(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    return gdmf_sprites_state_get(sprite_index)->visible;
}

bool SetSpriteVisible(int sprite_index, bool visible) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->visible = visible;

    return true;
}

bool ToggleSpriteVisible(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->visible = !gdmf_sprites_state_get(sprite_index)->visible;

    return true;
}

bool SetSpritePosition(int sprite_index, float x, float y) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->x = x;
    gdmf_sprites_state_get(sprite_index)->y = y;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

float GetSpriteX(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->x;
}

float GetSpriteY(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->y;
}

bool SetSpriteScale(int sprite_index, float scale) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    if (scale <= 0.0f) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->scale = scale;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

float GetSpriteScale(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->scale;
}

bool SetSpriteScaleX(int sprite_index, float scale_x) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    if (scale_x <= 0.0f) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->scaleX = scale_x;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool SetSpriteScaleY(int sprite_index, float scale_y) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    if (scale_y <= 0.0f) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->scaleY = scale_y;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

float GetSpriteScaleX(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 1.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->scaleX;
}

float GetSpriteScaleY(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 1.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->scaleY;
}

bool SetSpriteRotation(int sprite_index, float rotation_degrees) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->rotation = rotation_degrees;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

float GetSpriteRotation(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->rotation;
}

bool SetSpriteSkewX(int sprite_index, float skew_x) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->skewX = skew_x;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool SetSpriteSkewY(int sprite_index, float skew_y) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->skewY = skew_y;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

float GetSpriteSkewX(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->skewX;
}

float GetSpriteSkewY(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->skewY;
}

bool SetSpriteHotspot(int sprite_index, float hotspot_x, float hotspot_y) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    gdmf_sprites_state_get(sprite_index)->hotspotX = hotspot_x;
    gdmf_sprites_state_get(sprite_index)->hotspotY = hotspot_y;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool SetSpriteHotspotX(int sprite_index, float hotspot_x) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->hotspotX = hotspot_x;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool SetSpriteHotspotY(int sprite_index, float hotspot_y) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->hotspotY = hotspot_y;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

float GetSpriteHotspotX(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->hotspotX;
}

float GetSpriteHotspotY(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0.0f;
    }

    return gdmf_sprites_state_get(sprite_index)->hotspotY;
}

bool SetSpriteRotateAroundHotspot(int sprite_index, bool enabled) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->rotateAroundHotspot = enabled;
    gdmf_sprites_geometry_invalidate(sprite_index);

    return true;
}

bool GetSpriteRotateAroundHotspot(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    return gdmf_sprites_state_get(sprite_index)->rotateAroundHotspot;
}

bool SetSpriteFlipX(int sprite_index, bool mirrored) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    if (mirrored) {
        gdmf_sprites_state_get(sprite_index)->flip |= (unsigned char)SPRITE_FLIP_X;
    } else {
        gdmf_sprites_state_get(sprite_index)->flip &= (unsigned char)~SPRITE_FLIP_X;
    }

    return true;
}

bool SetSpriteFlipY(int sprite_index, bool mirrored) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    if (mirrored) {
        gdmf_sprites_state_get(sprite_index)->flip |= (unsigned char)SPRITE_FLIP_Y;
    } else {
        gdmf_sprites_state_get(sprite_index)->flip &= (unsigned char)~SPRITE_FLIP_Y;
    }

    return true;
}

bool GetSpriteFlipX(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    return (gdmf_sprites_state_get(sprite_index)->flip & SPRITE_FLIP_X) != 0;
}

bool GetSpriteFlipY(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    return (gdmf_sprites_state_get(sprite_index)->flip & SPRITE_FLIP_Y) != 0;
}

bool SetSpritePriority(int sprite_index, unsigned char priority) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->priority = priority;

    return true;
}

unsigned char GetSpritePriority(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0;
    }

    return gdmf_sprites_state_get(sprite_index)->priority;
}

bool SetSpriteColorPalette(int sprite_index, unsigned char palette) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->palette = palette;

    return true;
}

unsigned char GetSpriteColorPalette(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0;
    }

    return gdmf_sprites_state_get(sprite_index)->palette;
}

bool SetSpriteTransparency(int sprite_index, unsigned char opacity) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->transparency = opacity;

    return true;
}

unsigned char GetSpriteTransparency(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0;
    }

    return gdmf_sprites_state_get(sprite_index)->transparency;
}

bool SetSpriteShowZero(int sprite_index, bool show_zero) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->showzero = show_zero;

    return true;
}

bool GetSpriteShowZero(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }

    return gdmf_sprites_state_get(sprite_index)->showzero;
}

bool SetSpriteCollidableColors(int sprite_index, unsigned short color_mask) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->collidableColors = color_mask;

    return true;
}

unsigned short GetSpriteCollidableColors(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0;
    }

    return gdmf_sprites_state_get(sprite_index)->collidableColors;
}

bool SetSpriteCollisionTypes(int sprite_index, unsigned char type_mask) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return false;
    }
    gdmf_sprites_state_get(sprite_index)->collisionTypes = type_mask;

    return true;
}

unsigned char GetSpriteCollisionTypes(int sprite_index) {
    if (!gdmf_sprites_index_valid(sprite_index)) {
        return 0;
    }

    return gdmf_sprites_state_get(sprite_index)->collisionTypes;
}
