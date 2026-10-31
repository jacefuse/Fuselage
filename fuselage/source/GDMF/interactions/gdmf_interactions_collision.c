// Sprite-to-Tile tests. SEE: (GDMF_INTERACTIONS.md, Tile collisions)
#include "gdmf_interactions_private.h"
#include "../sprites/gdmf_sprites_probe.h"
#include "../tiles/gdmf_tiles_probe.h"
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
typedef struct {
    bool hit;
} gdmf_interactions_box_hit;
static bool gdmf_interactions_note_flagged_cell(const TilePointInfo* point, void* context) {
    if (point->collidable) {
        ((gdmf_interactions_box_hit*)context)->hit = true;
        return false;
    }
    return true;
}

bool gdmf_interactions_refuses_rotation(unsigned char layer, const char* operation) {
    static atomic_bool warned[MAX_TILE_LAYERS];

    if (GetTileMapRotation(layer) == 0.0f) {
        return false;
    }
    if (layer < MAX_TILE_LAYERS && !atomic_exchange(&warned[layer], true)) {
        printf("[Interactions] %s cannot query rotated Tile layer %u. "
               "Use Tile point queries. SEE: (GDMF_INTERACTIONS.md, Tile collisions)\n",
               operation, layer);
    }

    return true;
}

unsigned char CheckInteractionsSpriteTileCollision(int sprite_index, unsigned char layer,
                                                   unsigned char types_to_test) {
    float minimum_x, minimum_y, maximum_x, maximum_y;
    if (!gdmf_interactions_bounds(sprite_index, &minimum_x, &minimum_y, &maximum_x, &maximum_y)) {
        return 0;
    }
    if (GetTileLayerTileWidth(layer) == 0) {
        return 0;
    }
    if (gdmf_interactions_refuses_rotation(layer, "CheckInteractionsSpriteTileCollision")) {
        return 0;
    }

    unsigned char matched = 0;

    if (types_to_test & COLLISION_TYPE_BOUNDING_BOX) {
        gdmf_interactions_box_hit hit = {false};
        gdmf_interactions_visit_cells(sprite_index, layer, gdmf_interactions_note_flagged_cell, &hit);
        if (hit.hit) {
            matched |= COLLISION_TYPE_BOUNDING_BOX;
        }
    }

    unsigned char pixel_types = types_to_test & (COLLISION_TYPE_ANY_COLOR | COLLISION_TYPE_COLLIDABLE_COLORS);
    if (pixel_types == 0) {
        return matched;
    }

    const unsigned short sprite_mask = GetSpriteCollidableColors(sprite_index);
    const unsigned short layer_mask = GetTileLayerCollidableColors(layer);
    bool need_any = (pixel_types & COLLISION_TYPE_ANY_COLOR) != 0;
    bool need_mask = (pixel_types & COLLISION_TYPE_COLLIDABLE_COLORS) != 0;
    if (need_mask && (sprite_mask == 0 || layer_mask == 0)) {
        need_mask = false;
    }

    gdmf_sprite_probe sprite_probe;
    gdmf_tile_probe tile_probe;
    if (!gdmf_sprite_probe_init(sprite_index, &sprite_probe) || !gdmf_tile_probe_init(layer, &tile_probe)) {
        return matched;
    }

    enum { GDMF_INTERACTIONS_COLUMN_CHUNK = 256 };
    gdmf_tile_axis columns[GDMF_INTERACTIONS_COLUMN_CHUNK];
    double left = floor((double)minimum_x), right = ceil((double)maximum_x);
    double top = floor((double)minimum_y), bottom = ceil((double)maximum_y);
    if (left < INT_MIN || top < INT_MIN || right > INT_MAX || bottom > INT_MAX) {
        return matched;
    }
    const int first_x = (int)left, end_x = (int)right;
    const int first_y = (int)top, end_y = (int)bottom;
    TilePointInfo point;
    int chunk_end;
    for (int chunk_start = first_x; chunk_start < end_x && (need_any || need_mask); chunk_start = chunk_end) {
        chunk_end = ((int64_t)end_x - chunk_start < GDMF_INTERACTIONS_COLUMN_CHUNK)
                        ? end_x
                        : chunk_start + GDMF_INTERACTIONS_COLUMN_CHUNK;
        for (int world_x = chunk_start; world_x < chunk_end; world_x++) {
            gdmf_tile_probe_axis_x(&tile_probe, (float)world_x, &columns[world_x - chunk_start]);
        }

        for (int world_y = first_y; world_y < end_y && (need_any || need_mask); world_y++) {
            gdmf_tile_axis row;
            gdmf_tile_probe_axis_y(&tile_probe, (float)world_y, &row);
            if (!row.ok) {
                continue;
            }
            int row_start, row_end;
            if (!gdmf_sprite_probe_row_span(&sprite_probe, world_y, &row_start, &row_end)) {
                continue;
            }
            if (row_start < chunk_start) {
                row_start = chunk_start;
            }
            if (row_end > chunk_end) {
                row_end = chunk_end;
            }

            for (int world_x = row_start; world_x < row_end && (need_any || need_mask); world_x++) {
                const gdmf_tile_axis* column = &columns[world_x - chunk_start];
                if (!column->ok || !gdmf_tile_probe_collidable(&tile_probe, column, &row)) {
                    continue;
                }
                gdmf_tile_probe_cell(&tile_probe, column, &row, (float)world_x, (float)world_y, &point);
                if (!point.onTile) {
                    continue;
                }
                const unsigned char tile_color = point.paletteIndex;

                if (!(need_any && tile_color != 0) && !(need_mask && (layer_mask & (1u << tile_color)))) {
                    continue;
                }
                int sprite_color = gdmf_sprite_probe_pixel(&sprite_probe, (float)world_x, (float)world_y);
                if (sprite_color < 0) {
                    continue;
                }

                if (need_any && sprite_color != 0 && tile_color != 0) {
                    matched |= COLLISION_TYPE_ANY_COLOR;
                    need_any = false;
                }
                if (need_mask && (sprite_mask & (1u << sprite_color)) && (layer_mask & (1u << tile_color))) {
                    matched |= COLLISION_TYPE_COLLIDABLE_COLORS;
                    need_mask = false;
                }
            }
        }
    }

    return matched;
}
