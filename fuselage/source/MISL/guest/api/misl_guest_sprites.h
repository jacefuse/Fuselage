#pragma once
#include "misl_guest_memory.h"

// Sprite ecalls and bitmap-slot addressing.
// SEE: (MISL_GUEST.md, Sprites and tiles)

static inline unsigned char* GetSpriteBitmapSlot(int bitmap_id) {
    return (unsigned char*)(MISL_SPRITE_ADDR + (unsigned)bitmap_id * MISL_SPRITE_STRIDE);
}

static inline bool UploadSpriteBitmap(SpriteBitmapID bitmap_id, const unsigned char* bitmap) {
    return MISL_GUEST_CALL(MISL_SPR_UPLOAD_BITMAP, bitmap_id, misl_guest_address(bitmap), 0, 0, 0, 0, 0) != 0;
}

static inline bool AssignSprite(int sprite_index, SpriteBitmapID bitmap_id) {
    return MISL_GUEST_CALL(MISL_SPR_ASSIGN, sprite_index, bitmap_id, 0, 0, 0, 0, 0) != 0;
}

static inline bool AssignSpriteBitmapFromSprite(int source_sprite, int destination_sprite) {
    return MISL_GUEST_CALL(MISL_SPR_ASSIGN_FROM, source_sprite, destination_sprite, 0, 0, 0, 0, 0) != 0;
}

static inline SpriteBitmapID GetSpriteBitmapID(int sprite_index) {
    return (SpriteBitmapID)MISL_GUEST_CALL(MISL_SPR_GET_BITMAP_ID, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline bool ClearSprite(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_CLEAR, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool AssignSpriteTestPattern(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_TEST_PATTERN, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetSpriteEnabled(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_ENABLED, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpriteEnabled(int sprite_index, bool enabled) {
    return MISL_GUEST_CALL(MISL_SPR_SET_ENABLED, sprite_index, enabled, 0, 0, 0, 0, 0) != 0;
}

static inline bool ToggleSpriteEnabled(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_TOGGLE_ENABLED, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetSpriteVisible(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_VISIBLE, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpriteVisible(int sprite_index, bool visible) {
    return MISL_GUEST_CALL(MISL_SPR_SET_VISIBLE, sprite_index, visible, 0, 0, 0, 0, 0) != 0;
}

static inline bool ToggleSpriteVisible(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_TOGGLE_VISIBLE, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpritePosition(int sprite_index, float x, float y) {
    return MISL_GUEST_CALL(MISL_SPR_SET_POSITION, sprite_index, misl_guest_float_bits(x),
                           misl_guest_float_bits(y), 0, 0, 0, 0) != 0;
}

static inline bool OffsetSpritePosition(int sprite_index, float offset_x, float offset_y) {
    return MISL_GUEST_CALL(MISL_SPR_OFFSET_POSITION, sprite_index, misl_guest_float_bits(offset_x),
                           misl_guest_float_bits(offset_y), 0, 0, 0, 0) != 0;
}

static inline float GetSpriteX(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_X, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline float GetSpriteY(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_Y, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline bool SetSpriteScale(int sprite_index, float scale) {
    return MISL_GUEST_CALL(MISL_SPR_SET_SCALE, sprite_index, misl_guest_float_bits(scale), 0, 0, 0, 0, 0) !=
           0;
}

static inline float GetSpriteScale(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_SCALE, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline bool OffsetSpriteScale(int sprite_index, float scale_offset) {
    return MISL_GUEST_CALL(MISL_SPR_OFFSET_SCALE, sprite_index, misl_guest_float_bits(scale_offset), 0, 0, 0,
                           0, 0) != 0;
}

static inline bool SetSpriteScaleX(int sprite_index, float scale_x) {
    return MISL_GUEST_CALL(MISL_SPR_SET_SCALE_X, sprite_index, misl_guest_float_bits(scale_x), 0, 0, 0, 0,
                           0) != 0;
}

static inline bool SetSpriteScaleY(int sprite_index, float scale_y) {
    return MISL_GUEST_CALL(MISL_SPR_SET_SCALE_Y, sprite_index, misl_guest_float_bits(scale_y), 0, 0, 0, 0,
                           0) != 0;
}

static inline float GetSpriteScaleX(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_SCALE_X, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline float GetSpriteScaleY(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_SCALE_Y, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline bool SetSpriteRotation(int sprite_index, float rotation_degrees) {
    return MISL_GUEST_CALL(MISL_SPR_SET_ROTATION, sprite_index, misl_guest_float_bits(rotation_degrees), 0, 0,
                           0, 0, 0) != 0;
}

static inline float GetSpriteRotation(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_ROTATION, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline bool OffsetSpriteRotation(int sprite_index, float rotation_offset) {
    return MISL_GUEST_CALL(MISL_SPR_OFFSET_ROTATION, sprite_index, misl_guest_float_bits(rotation_offset), 0,
                           0, 0, 0, 0) != 0;
}

static inline bool SetSpriteSkewX(int sprite_index, float skew_x) {
    return MISL_GUEST_CALL(MISL_SPR_SET_SKEW_X, sprite_index, misl_guest_float_bits(skew_x), 0, 0, 0, 0, 0) !=
           0;
}

static inline bool SetSpriteSkewY(int sprite_index, float skew_y) {
    return MISL_GUEST_CALL(MISL_SPR_SET_SKEW_Y, sprite_index, misl_guest_float_bits(skew_y), 0, 0, 0, 0, 0) !=
           0;
}

static inline float GetSpriteSkewX(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_SKEW_X, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline float GetSpriteSkewY(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_SKEW_Y, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline bool SetSpriteHotspot(int sprite_index, float hotspot_x, float hotspot_y) {
    return MISL_GUEST_CALL(MISL_SPR_SET_HOTSPOT, sprite_index, misl_guest_float_bits(hotspot_x),
                           misl_guest_float_bits(hotspot_y), 0, 0, 0, 0) != 0;
}

static inline bool SetSpriteHotspotX(int sprite_index, float hotspot_x) {
    return MISL_GUEST_CALL(MISL_SPR_SET_HOTSPOT_X, sprite_index, misl_guest_float_bits(hotspot_x), 0, 0, 0, 0,
                           0) != 0;
}

static inline bool SetSpriteHotspotY(int sprite_index, float hotspot_y) {
    return MISL_GUEST_CALL(MISL_SPR_SET_HOTSPOT_Y, sprite_index, misl_guest_float_bits(hotspot_y), 0, 0, 0, 0,
                           0) != 0;
}

static inline float GetSpriteHotspotX(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_HOTSPOT_X, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline float GetSpriteHotspotY(int sprite_index) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SPR_GET_HOTSPOT_Y, sprite_index, 0, 0, 0, 0, 0, 0));
}

static inline bool SetSpriteRotateAroundHotspot(int sprite_index, bool enabled) {
    return MISL_GUEST_CALL(MISL_SPR_SET_ROT_HOTSPOT, sprite_index, enabled, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetSpriteRotateAroundHotspot(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_ROT_HOTSPOT, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpriteFlipX(int sprite_index, bool mirrored) {
    return MISL_GUEST_CALL(MISL_SPR_SET_FLIP_X, sprite_index, mirrored, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpriteFlipY(int sprite_index, bool mirrored) {
    return MISL_GUEST_CALL(MISL_SPR_SET_FLIP_Y, sprite_index, mirrored, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetSpriteFlipX(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_FLIP_X, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetSpriteFlipY(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_FLIP_Y, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpritePriority(int sprite_index, unsigned char priority) {
    return MISL_GUEST_CALL(MISL_SPR_SET_PRIORITY, sprite_index, priority, 0, 0, 0, 0, 0) != 0;
}

static inline unsigned char GetSpritePriority(int sprite_index) {
    return (unsigned char)MISL_GUEST_CALL(MISL_SPR_GET_PRIORITY, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline bool SetSpriteColorPalette(int sprite_index, unsigned char palette) {
    return MISL_GUEST_CALL(MISL_SPR_SET_PALETTE, sprite_index, palette, 0, 0, 0, 0, 0) != 0;
}

static inline unsigned char GetSpriteColorPalette(int sprite_index) {
    return (unsigned char)MISL_GUEST_CALL(MISL_SPR_GET_PALETTE, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline bool SetSpriteTransparency(int sprite_index, unsigned char opacity) {
    return MISL_GUEST_CALL(MISL_SPR_SET_TRANSPARENCY, sprite_index, opacity, 0, 0, 0, 0, 0) != 0;
}

static inline unsigned char GetSpriteTransparency(int sprite_index) {
    return (unsigned char)MISL_GUEST_CALL(MISL_SPR_GET_TRANSPARENCY, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline bool SetSpriteShowZero(int sprite_index, bool show_zero) {
    return MISL_GUEST_CALL(MISL_SPR_SHOW_ZERO, sprite_index, show_zero, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetSpriteShowZero(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_SHOWZERO, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetSpriteCollidableColors(int sprite_index, unsigned short color_mask) {
    return MISL_GUEST_CALL(MISL_SPR_SET_COLLIDABLE, sprite_index, color_mask, 0, 0, 0, 0, 0) != 0;
}

static inline unsigned short GetSpriteCollidableColors(int sprite_index) {
    return (unsigned short)MISL_GUEST_CALL(MISL_SPR_GET_COLLIDABLE, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline bool SetSpriteCollisionTypes(int sprite_index, unsigned char type_mask) {
    return MISL_GUEST_CALL(MISL_SPR_SET_COLLTYPES, sprite_index, type_mask, 0, 0, 0, 0, 0) != 0;
}

static inline unsigned char GetSpriteCollisionTypes(int sprite_index) {
    return (unsigned char)MISL_GUEST_CALL(MISL_SPR_GET_COLLTYPES, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline bool HasSpriteCollision(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_HAS_COLLISION, sprite_index, 0, 0, 0, 0, 0, 0) != 0;
}

static inline int GetSpriteCollisionCount(int sprite_index) {
    return MISL_GUEST_CALL(MISL_SPR_GET_COLL_COUNT, sprite_index, 0, 0, 0, 0, 0, 0);
}

static inline int CopySpriteCollisions(int sprite_index, void* output, int capacity) {
    return MISL_GUEST_CALL(MISL_SPR_COPY_COLLISIONS, sprite_index, misl_guest_address(output), capacity, 0, 0,
                           0, 0);
}

static inline unsigned char CheckSpritePairCollision(int first_sprite, int second_sprite,
                                                     unsigned char types_to_test) {
    return (unsigned char)MISL_GUEST_CALL(MISL_SPR_CHECK_PAIR, first_sprite, second_sprite, types_to_test, 0,
                                          0, 0, 0);
}

static inline int CheckSpritePairCollisionBatch(const SpritePairRef* pairs, int count,
                                                unsigned char types_to_test, unsigned char* results) {
    return MISL_GUEST_CALL(MISL_SPR_CHECK_PAIR_BATCH, misl_guest_address(pairs), count, types_to_test,
                           misl_guest_address(results), 0, 0, 0);
}

static inline int FindSpriteCollisionPairs(const short* sprite_indices, int count,
                                           unsigned char types_to_test, SpritePairRef* output_pairs,
                                           int capacity, unsigned char* output_types) {
    return MISL_GUEST_CALL(MISL_SPR_FIND_PAIRS, misl_guest_address(sprite_indices), count, types_to_test,
                           misl_guest_address(output_pairs), capacity, misl_guest_address(output_types), 0);
}

static inline bool IsSpriteWorldPointInside(int sprite_index, float world_x, float world_y) {
    return MISL_GUEST_CALL(MISL_SPR_WORLD_POINT_ON, sprite_index, misl_guest_float_bits(world_x),
                           misl_guest_float_bits(world_y), 0, 0, 0, 0) != 0;
}

static inline void ToggleSpriteAtlasView(void) {
    MISL_GUEST_CALL(MISL_SPR_TOGGLE_ATLAS, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool GetSpriteAtlasViewActive(void) {
    return MISL_GUEST_CALL(MISL_SPR_GET_ATLAS_ACTIVE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline int GetSpriteRenderedCount(void) {
    return MISL_GUEST_CALL(MISL_SPR_GET_RENDERED_COUNT, 0, 0, 0, 0, 0, 0, 0);
}
