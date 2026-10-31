// Inverse tile lookup. SEE: (GDMF_TILES.md, Prepared probes)
#include "gdmf_tiles_probe.h"
#include <limits.h>
#include <math.h>
#include <string.h>

bool gdmf_tile_probe_init(uint8_t layer, gdmf_tile_probe* probe) {
    if (!probe) {
        return false;
    }
    memset(probe, 0, sizeof(*probe));
    const TileMap* map = gdmf_tiles_state_get(layer);
    if (!map || !map->initialized || !map->width || map->width > MAX_MAP_WIDTH || !map->height ||
        map->height > MAX_MAP_HEIGHT || !map->tileWidth || map->tileWidth > MAX_TILE_WIDTH ||
        !map->tileHeight || map->tileHeight > MAX_TILE_HEIGHT || !isfinite(map->scale) || map->scale <= 0 ||
        !isfinite(map->viewportX) || !isfinite(map->viewportY) || !isfinite(map->viewportWidth) ||
        !isfinite(map->viewportHeight) || map->viewportWidth <= 0 || map->viewportHeight <= 0 ||
        !isfinite(map->mapOffsetX) || !isfinite(map->mapOffsetY) || !isfinite(map->rotation) ||
        !isfinite(map->pivotX) || !isfinite(map->pivotY)) {
        return false;
    }
    gdmf_tile_probe pending = {.map = map,
                               .layer = layer,
                               .view_right = map->viewportX + map->viewportWidth,
                               .view_bottom = map->viewportY + map->viewportHeight,
                               .tile_width = (float)map->tileWidth,
                               .tile_height = (float)map->tileHeight,
                               .turned = map->rotation != 0.0f,
                               .pivot_x = map->viewportX + map->pivotX,
                               .pivot_y = map->viewportY + map->pivotY};
    pending.sine = pending.turned ? sinf(map->rotation) : 0;
    pending.cosine = pending.turned ? cosf(map->rotation) : 1;
    float scaled_width = pending.tile_width * map->scale, scaled_height = pending.tile_height * map->scale;
    if (!isfinite(pending.view_right) || !isfinite(pending.view_bottom) || !isfinite(pending.pivot_x) ||
        !isfinite(pending.pivot_y) || !isfinite(scaled_width) || !isfinite(scaled_height) ||
        scaled_width <= 0 || scaled_height <= 0) {
        return false;
    }
    *probe = pending;
    return true;
}
void gdmf_tile_probe_unrotate(const gdmf_tile_probe* probe, float* x, float* y) {
    if (!probe || !probe->map || !x || !y || !probe->turned) {
        return;
    }
    float offset_x = *x - probe->pivot_x, offset_y = *y - probe->pivot_y;
    *x = probe->pivot_x + offset_x * probe->cosine + offset_y * probe->sine;
    *y = probe->pivot_y - offset_x * probe->sine + offset_y * probe->cosine;
}
static void gdmf_tile_axis_local(const gdmf_tile_probe* probe, float coordinate, bool horizontal,
                                 gdmf_tile_axis* axis) {
    *axis = (gdmf_tile_axis){0};
    const TileMap* map = probe->map;
    float origin = horizontal ? map->viewportX : map->viewportY;
    double scroll = horizontal ? map->mapOffsetX : map->mapOffsetY;
    float tile_size = horizontal ? probe->tile_width : probe->tile_height;
    int map_size = horizontal ? map->width : map->height;
    double source = (double)(coordinate - origin) / map->scale + scroll;
    double cell = floor(source / tile_size);
    // Reject in double precision before the integer conversion.
    if (!isfinite(cell) || cell < INT_MIN || cell > INT_MAX) {
        return;
    }
    axis->unwrapped = (int)cell;
    axis->wrapped = axis->unwrapped;
    if (horizontal ? map->wrapX : map->wrapY) {
        axis->wrapped = ((axis->wrapped % map_size) + map_size) % map_size;
    } else if (axis->wrapped < 0 || axis->wrapped >= map_size) {
        return;
    }
    axis->ok = true;
}
void gdmf_tile_probe_axis_x(const gdmf_tile_probe* probe, float canvas_x, gdmf_tile_axis* axis) {
    if (!axis) {
        return;
    }
    *axis = (gdmf_tile_axis){0};
    if (!probe || !probe->map || probe->turned || !isfinite(canvas_x) || canvas_x < probe->map->viewportX ||
        canvas_x >= probe->view_right) {
        return;
    }
    gdmf_tile_axis_local(probe, canvas_x, true, axis);
}
void gdmf_tile_probe_axis_y(const gdmf_tile_probe* probe, float canvas_y, gdmf_tile_axis* axis) {
    if (!axis) {
        return;
    }
    *axis = (gdmf_tile_axis){0};
    if (!probe || !probe->map || probe->turned || !isfinite(canvas_y) || canvas_y < probe->map->viewportY ||
        canvas_y >= probe->view_bottom) {
        return;
    }
    gdmf_tile_axis_local(probe, canvas_y, false, axis);
}
static bool gdmf_tile_axes_valid(const gdmf_tile_probe* probe, const gdmf_tile_axis* x,
                                 const gdmf_tile_axis* y) {
    return probe && probe->map && x && y && x->ok && y->ok && x->wrapped >= 0 && y->wrapped >= 0 &&
           x->wrapped < probe->map->width && y->wrapped < probe->map->height;
}
bool gdmf_tile_probe_collidable(const gdmf_tile_probe* probe, const gdmf_tile_axis* x,
                                const gdmf_tile_axis* y) {
    return gdmf_tile_axes_valid(probe, x, y) &&
           (probe->map->location[x->wrapped][y->wrapped].flags & TILE_FLAG_COLLISION) != 0;
}
bool gdmf_tile_probe_cell(const gdmf_tile_probe* probe, const gdmf_tile_axis* x, const gdmf_tile_axis* y,
                          float local_x, float local_y, TilePointInfo* output) {
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
    const unsigned char* pixels = gdmf_tiles_bitmap_data(probe->layer, cell->tileTypeID);
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
bool gdmf_tile_probe_point(const gdmf_tile_probe* probe, float canvas_x, float canvas_y,
                           TilePointInfo* output) {
    if (!output) {
        return false;
    }
    memset(output, 0, sizeof(*output));
    if (!probe || !probe->map || !isfinite(canvas_x) || !isfinite(canvas_y)) {
        return false;
    }
    gdmf_tile_axis x, y;
    if (probe->turned) {
        if (canvas_x < probe->map->viewportX || canvas_x >= probe->view_right ||
            canvas_y < probe->map->viewportY || canvas_y >= probe->view_bottom) {
            return false;
        }
        gdmf_tile_probe_unrotate(probe, &canvas_x, &canvas_y);
        gdmf_tile_axis_local(probe, canvas_x, true, &x);
        gdmf_tile_axis_local(probe, canvas_y, false, &y);
    } else {
        gdmf_tile_probe_axis_x(probe, canvas_x, &x);
        gdmf_tile_probe_axis_y(probe, canvas_y, &y);
    }
    if (!x.ok || !y.ok) {
        return false;
    }
    return gdmf_tile_probe_cell(probe, &x, &y, canvas_x, canvas_y, output);
}
