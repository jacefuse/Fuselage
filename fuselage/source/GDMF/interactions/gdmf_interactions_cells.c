// Flagged-cell enumeration. SEE: (GDMF_INTERACTIONS.md, Cell enumeration)
#include "gdmf_interactions_private.h"
#include <math.h>
#include <limits.h>

bool gdmf_interactions_bounds(int sprite_index, float* minimum_x, float* minimum_y, float* maximum_x,
                              float* maximum_y) {
    return GetSpriteBoundingBox(sprite_index, minimum_x, minimum_y, maximum_x, maximum_y) &&
           isfinite(*minimum_x) && isfinite(*minimum_y) && isfinite(*maximum_x) && isfinite(*maximum_y) &&
           *minimum_x < *maximum_x && *minimum_y < *maximum_y;
}
void gdmf_interactions_visit_cells(int sprite_index, unsigned char layer,
                                   gdmf_interactions_cell_callback callback, void* context) {
    float minimum_x, minimum_y, maximum_x, maximum_y;
    if (!callback ||
        !gdmf_interactions_bounds(sprite_index, &minimum_x, &minimum_y, &maximum_x, &maximum_y)) {
        return;
    }
    const float step_x = (float)GetTileLayerTileWidth(layer) * GetTileLayerScale(layer);
    const float step_y = (float)GetTileLayerTileHeight(layer) * GetTileLayerScale(layer);
    if (!isfinite(step_x) || !isfinite(step_y) || step_x <= 0 || step_y <= 0) {
        return;
    }
    // Preserve ENTERON's exclusive-edge inset and floating-point stepping.
    const float last_x = maximum_x - 0.001f, last_y = maximum_y - 0.001f;
    if ((minimum_x < last_x && (!(minimum_x + step_x > minimum_x) || !(last_x + step_x > last_x))) ||
        (minimum_y < last_y && (!(minimum_y + step_y > minimum_y) || !(last_y + step_y > last_y)))) {
        return;
    }
    bool last_row = false;
    for (float y = minimum_y; !last_row;) {
        if (y >= last_y) {
            y = last_y;
            last_row = true;
        }
        bool last_column = false;
        int previous_map_x = -1;
        for (float x = minimum_x; !last_column;) {
            if (x >= last_x) {
                x = last_x;
                last_column = true;
            }
            TilePointInfo point;
            if (GetTilePointInfo(layer, x, y, &point) && point.onMap && (int)point.mapX != previous_map_x) {
                previous_map_x = (int)point.mapX;
                if (!callback(&point, context)) {
                    return;
                }
            }
            if (!last_column) {
                float next_x = x + step_x;
                if (!isfinite(next_x) || next_x <= x) {
                    return;
                }
                x = next_x;
            }
        }
        if (!last_row) {
            float next_y = y + step_y;
            if (!isfinite(next_y) || next_y <= y) {
                return;
            }
            y = next_y;
        }
    }
}
typedef struct {
    TileCellRef* output;
    int capacity, count;
} gdmf_interactions_cell_sink;
static bool gdmf_interactions_collect_cell(const TilePointInfo* point, void* context) {
    gdmf_interactions_cell_sink* sink = context;
    if (!point->collidable) {
        return true;
    }
    if (sink->count == INT_MAX) {
        return false;
    }
    if (sink->output && sink->count < sink->capacity) {
        sink->output[sink->count] = (TileCellRef){point->mapX, point->mapY};
    }
    ++sink->count;
    return true;
}
int GetInteractionsSpriteTileCollisionCells(int sprite_index, unsigned char layer, TileCellRef* output_cells,
                                            int capacity) {
    if (gdmf_interactions_refuses_rotation(layer, "GetInteractionsSpriteTileCollisionCells")) {
        return 0;
    }
    gdmf_interactions_cell_sink sink = {output_cells, capacity, 0};
    gdmf_interactions_visit_cells(sprite_index, layer, gdmf_interactions_collect_cell, &sink);
    return sink.count;
}
