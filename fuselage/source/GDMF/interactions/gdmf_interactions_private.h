#ifndef GDMF_INTERACTIONS_PRIVATE_H
#define GDMF_INTERACTIONS_PRIVATE_H
#include "../gdmf_interactions.h"
#include "../sprites/gdmf_sprites.h"
#include "../tiles/gdmf_tiles.h"

typedef bool (*gdmf_interactions_cell_callback)(const TilePointInfo* point, void* context);
bool gdmf_interactions_bounds(int sprite_index, float* minimum_x, float* minimum_y, float* maximum_x,
                              float* maximum_y);
void gdmf_interactions_visit_cells(int sprite_index, unsigned char layer,
                                   gdmf_interactions_cell_callback callback, void* context);
bool gdmf_interactions_refuses_rotation(unsigned char layer, const char* operation);
#endif
