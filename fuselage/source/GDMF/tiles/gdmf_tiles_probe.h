#ifndef GDMF_TILES_PROBE_H
#define GDMF_TILES_PROBE_H

#include "gdmf_tiles_bitmaps.h"
#include <math.h>
#include <string.h>

typedef struct {
    const TileMap* map;
    gdmf_tiles_bitmap_view bitmaps;
    uint8_t layer;
    float view_right, view_bottom, tile_width, tile_height;
    bool turned;
    float sine, cosine, pivot_x, pivot_y;
} gdmf_tile_probe;

typedef struct {
    int unwrapped, wrapped;
    bool ok;
} gdmf_tile_axis;

// Cached transform with borrowed live state, not a detached snapshot. Keep layer
// and bitmap state unchanged through use. SEE: (GDMF_TILES.md, Prepared probes)
// Failed initialization clears the output. No GPU access.
bool gdmf_tile_probe_init(uint8_t layer, gdmf_tile_probe* probe);
void gdmf_tile_probe_unrotate(const gdmf_tile_probe* probe, float* x, float* y);
bool gdmf_tile_probe_point(const gdmf_tile_probe* probe, float canvas_x, float canvas_y,
                           TilePointInfo* output);

// Axis splitting is valid only for unturned layers; turned probes return !ok.
// Axes passed to cell/collidable must come from the same stable probe/query.
void gdmf_tile_probe_axis_x(const gdmf_tile_probe* probe, float canvas_x, gdmf_tile_axis* axis);
void gdmf_tile_probe_axis_y(const gdmf_tile_probe* probe, float canvas_y, gdmf_tile_axis* axis);

// The per-pixel half is inline: collision walks in other files call it for every
// covered pixel. SEE: (GDMF_INTERACTIONS.md, Tile collisions)
static inline bool gdmf_tile_axes_valid(const gdmf_tile_probe* probe, const gdmf_tile_axis* x,
                                        const gdmf_tile_axis* y) {
    return probe && probe->map && x && y && x->ok && y->ok && x->wrapped >= 0 && y->wrapped >= 0 &&
           x->wrapped < probe->map->width && y->wrapped < probe->map->height;
}
static inline bool gdmf_tile_probe_collidable(const gdmf_tile_probe* probe, const gdmf_tile_axis* x,
                                              const gdmf_tile_axis* y) {
    return gdmf_tile_axes_valid(probe, x, y) &&
           (probe->map->location[x->wrapped][y->wrapped].flags & TILE_FLAG_COLLISION) != 0;
}
// Coordinates are in unrotated layer space; output clears on invalid input.
static inline bool gdmf_tile_probe_cell(const gdmf_tile_probe* probe, const gdmf_tile_axis* x,
                                        const gdmf_tile_axis* y, float local_x, float local_y,
                                        TilePointInfo* output) {
    if (!output) {
        return false;
    }
    memset(output, 0, sizeof(*output));
    if (!gdmf_tile_axes_valid(probe, x, y) || !isfinite(local_x) || !isfinite(local_y)) {
        return false;
    }
    const TileMap* map = probe->map;
    const TileLocation* cell = &map->location[x->wrapped][y->wrapped];
    output->onMap = true;
    output->mapX = (uint16_t)x->wrapped;
    output->mapY = (uint16_t)y->wrapped;
    output->collidable = (cell->flags & TILE_FLAG_COLLISION) != 0;
    const unsigned char* pixels = gdmf_tiles_bitmap_view_data(&probe->bitmaps, cell->tileTypeID);
    if (!pixels) {
        return true;
    }
    float width = probe->tile_width * map->scale, height = probe->tile_height * map->scale;
    float origin_x = map->viewportX +
                     (float)(((double)x->unwrapped * probe->tile_width - map->mapOffsetX) * map->scale) +
                     (float)cell->offsetX;
    float origin_y = map->viewportY +
                     (float)(((double)y->unwrapped * probe->tile_height - map->mapOffsetY) * map->scale) +
                     (float)cell->offsetY;
    bool flip_x = (cell->flags & TILE_FLAG_HFLIP) != 0, flip_y = (cell->flags & TILE_FLAG_VFLIP) != 0;
    bool quarter_turn = (cell->flags & TILE_FLAG_ROT90) != 0;
    if (quarter_turn) {
        origin_x += (width - height) * 0.5f;
        origin_y += (height - width) * 0.5f;
        float original_width = width;
        width = height;
        height = original_width;
    }
    float fraction_x = (local_x - origin_x) / width, fraction_y = (local_y - origin_y) / height;
    if (!isfinite(fraction_x) || !isfinite(fraction_y) || fraction_x < 0 || fraction_x >= 1 ||
        fraction_y < 0 || fraction_y >= 1) {
        return true;
    }
    float source_left = flip_x ? 1 : 0, source_right = flip_x ? 0 : 1;
    float source_top = flip_y ? 1 : 0, source_bottom = flip_y ? 0 : 1;
    float texture_x, texture_y;
    if (!quarter_turn) {
        texture_x = source_left + (source_right - source_left) * fraction_x;
        texture_y = source_top + (source_bottom - source_top) * fraction_y;
    } else {
        texture_x = source_left + (source_right - source_left) * fraction_y;
        texture_y = source_bottom + (source_top - source_bottom) * fraction_x;
    }
    int pixel_x = (int)(texture_x * probe->tile_width), pixel_y = (int)(texture_y * probe->tile_height);
    if (pixel_x < 0) {
        pixel_x = 0;
    }
    if (pixel_y < 0) {
        pixel_y = 0;
    }
    if (pixel_x >= map->tileWidth) {
        pixel_x = map->tileWidth - 1;
    }
    if (pixel_y >= map->tileHeight) {
        pixel_y = map->tileHeight - 1;
    }
    output->onTile = true;
    output->paletteIndex = pixels[pixel_y * map->tileWidth + pixel_x];
    return true;
}

#endif
