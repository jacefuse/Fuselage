#ifndef GDMF_INTERACTIONS_H
#define GDMF_INTERACTIONS_H
#include <stdbool.h>

#define GDMF_INTERACTIONS_VERSION "0.5.2026102701 ENTERON"

// Geometric-center distance in canvas pixels; zero for an invalid index.
float GetInteractionsSpriteDistance(int first_sprite, int second_sprite);
// Signed atan2 direction in degrees; zero for an invalid index.
// Positive angles turn toward positive canvas y. SEE: (GDMF_INTERACTIONS.md, Geometry)
float GetInteractionsSpriteDirection(int first_sprite, int second_sprite);
// Forward transform of an integer bitmap coordinate; both outputs are required.
// False leaves outputs untouched for invalid coordinates/indices/NULL outputs.
bool GetInteractionsSpritePixelWorldPosition(int sprite_index, int local_x, int local_y, float* world_x,
                                             float* world_y);

// Requested collision bits that matched; zero for no match or unavailable query.
// Rotated Tile maps are unsupported. SEE: (GDMF_INTERACTIONS.md, Tile collisions)
unsigned char CheckInteractionsSpriteTileCollision(int sprite_index, unsigned char layer,
                                                   unsigned char types_to_test);

typedef struct {
    unsigned short x, y;
} TileCellRef;

// Flagged cells overlapped by the Sprite's box, in row order; wrapping may repeat cells.
// Returns the total count, possibly greater than capacity. NULL output counts only.
int GetInteractionsSpriteTileCollisionCells(int sprite_index, unsigned char layer, TileCellRef* output_cells,
                                            int capacity);

// One result byte per Sprite; returns how many matched any requested type.
// Nonpositive count/NULL arrays write nothing. Keep all input state stable until return.
int CheckInteractionsSpriteTileCollisionBatch(const short* sprites, int count, unsigned char layer,
                                              unsigned char types_to_test, unsigned char* results);
#endif
