#ifndef GDMF_TILES_PROBE_H
#define GDMF_TILES_PROBE_H

#include "gdmf_tiles_bitmaps.h"

typedef struct {
    const TileMap* map;
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
bool gdmf_tile_probe_collidable(const gdmf_tile_probe* probe, const gdmf_tile_axis* x,
                                const gdmf_tile_axis* y);
// Coordinates are in unrotated layer space; output clears on invalid input.
bool gdmf_tile_probe_cell(const gdmf_tile_probe* probe, const gdmf_tile_axis* x, const gdmf_tile_axis* y,
                          float local_x, float local_y, TilePointInfo* output);

#endif
