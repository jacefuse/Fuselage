#pragma once
#include "misl_guest_call_internal.h"

// Sprite geometry and collision ecalls.
// SEE: (MISL_GUEST.md, Collision queries)

static inline float GetInteractionsSpriteDistance(int first_sprite, int second_sprite) {
    return misl_guest_bits_float(
        MISL_GUEST_CALL(MISL_IACT_DIST_SPRITES, first_sprite, second_sprite, 0, 0, 0, 0, 0));
}

static inline float GetInteractionsSpriteDirection(int first_sprite, int second_sprite) {
    return misl_guest_bits_float(
        MISL_GUEST_CALL(MISL_IACT_DIR_SPRITES, first_sprite, second_sprite, 0, 0, 0, 0, 0));
}

static inline bool GetInteractionsSpritePixelWorldPosition(int sprite_index, int local_x, int local_y,
                                                           float* world_x, float* world_y) {
    return MISL_GUEST_CALL(MISL_IACT_LOCALPIX_TO_WORLD, sprite_index, local_x, local_y,
                           misl_guest_address(world_x), misl_guest_address(world_y), 0, 0) != 0;
}

static inline unsigned char CheckInteractionsSpriteTileCollision(int sprite_index, uint8_t layer,
                                                                 unsigned char types_to_test) {
    return (unsigned char)MISL_GUEST_CALL(MISL_IACT_SPRITE_TILE_COLLISION, sprite_index, layer, types_to_test,
                                          0, 0, 0, 0);
}

static inline int GetInteractionsSpriteTileCollisionCells(int sprite_index, uint8_t layer,
                                                          TileCellRef* output_cells, int capacity) {
    return (int)MISL_GUEST_CALL(MISL_IACT_SPRITE_TILE_CELLS, sprite_index, layer,
                                misl_guest_address(output_cells), capacity, 0, 0, 0);
}

static inline int CheckInteractionsSpriteTileCollisionBatch(const short* sprites, int count, uint8_t layer,
                                                            unsigned char types_to_test,
                                                            unsigned char* results) {
    return (int)MISL_GUEST_CALL(MISL_IACT_SPRITE_TILE_BATCH, misl_guest_address(sprites), count, layer,
                                types_to_test, misl_guest_address(results), 0, 0);
}
