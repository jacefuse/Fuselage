// Candidate coverage. SEE: (GDMF_TILES.md, Prepared frames)
#include "gdmf_tiles_frame.h"
#include <limits.h>
#include <math.h>
#include <string.h>

bool gdmf_tiles_geometry_plan_create(const gdmf_tiles_geometry_context* context,
                                     gdmf_tiles_geometry_plan* plan) {
    if (!plan) {
        return false;
    }
    memset(plan, 0, sizeof(*plan));
    if (!context || !context->map || !isfinite(context->canvas_width) || context->canvas_width <= 0 ||
        !isfinite(context->canvas_height) || context->canvas_height <= 0) {
        return false;
    }
    const TileMap* map = context->map;
    if (!map->initialized || !map->width || map->width > MAX_MAP_WIDTH || !map->height ||
        map->height > MAX_MAP_HEIGHT || !map->tileWidth || map->tileWidth > MAX_TILE_WIDTH ||
        !map->tileHeight || map->tileHeight > MAX_TILE_HEIGHT || !isfinite(map->scale) || map->scale <= 0 ||
        !isfinite(map->viewportX) || !isfinite(map->viewportY) || !isfinite(map->viewportWidth) ||
        map->viewportWidth <= 0 || !isfinite(map->viewportHeight) || map->viewportHeight <= 0 ||
        !isfinite(map->mapOffsetX) || !isfinite(map->mapOffsetY) || !isfinite(map->rotation) ||
        !isfinite(map->pivotX) || !isfinite(map->pivotY)) {
        return false;
    }
    gdmf_tiles_geometry_plan pending = {.scale = map->scale,
                                        .tile_width = (float)map->tileWidth * map->scale,
                                        .tile_height = (float)map->tileHeight * map->scale,
                                        .offset_x = map->mapOffsetX * map->scale,
                                        .offset_y = map->mapOffsetY * map->scale,
                                        .turned = map->rotation != 0};
    if (!isfinite(pending.tile_width) || !isfinite(pending.tile_height) || pending.tile_width <= 0 ||
        pending.tile_height <= 0 || !isfinite(pending.offset_x) || !isfinite(pending.offset_y)) {
        return false;
    }
    pending.cosine = pending.turned ? cosf(map->rotation) : 1;
    pending.sine = pending.turned ? sinf(map->rotation) : 0;
    double start_x, start_y, columns, rows;
    if (!pending.turned) {
        start_x = trunc(pending.offset_x / pending.tile_width) - 1;
        start_y = trunc(pending.offset_y / pending.tile_height) - 1;
        columns = (double)ceilf(map->viewportWidth / pending.tile_width) + 3;
        rows = (double)ceilf(map->viewportHeight / pending.tile_height) + 3;
    } else {
        float pivot_x = map->viewportX + map->pivotX, pivot_y = map->viewportY + map->pivotY;
        float corners_x[4] = {map->viewportX, map->viewportX + map->viewportWidth, map->viewportX,
                              map->viewportX + map->viewportWidth};
        float corners_y[4] = {map->viewportY, map->viewportY, map->viewportY + map->viewportHeight,
                              map->viewportY + map->viewportHeight};
        double minimum_x = 0, maximum_x = 0, minimum_y = 0, maximum_y = 0;
        for (unsigned corner = 0; corner < 4; ++corner) {
            float offset_x = corners_x[corner] - pivot_x, offset_y = corners_y[corner] - pivot_y;
            float local_x = pivot_x + offset_x * pending.cosine + offset_y * pending.sine;
            float local_y = pivot_y - offset_x * pending.sine + offset_y * pending.cosine;
            double x = ((double)local_x - map->viewportX + pending.offset_x) / pending.tile_width;
            double y = ((double)local_y - map->viewportY + pending.offset_y) / pending.tile_height;
            if (!isfinite(x) || !isfinite(y)) {
                return false;
            }
            if (!corner) {
                minimum_x = maximum_x = x;
                minimum_y = maximum_y = y;
                continue;
            }
            if (x < minimum_x) {
                minimum_x = x;
            } else if (x > maximum_x) {
                maximum_x = x;
            }
            if (y < minimum_y) {
                minimum_y = y;
            } else if (y > maximum_y) {
                maximum_y = y;
            }
        }
        start_x = floor(minimum_x) - 1;
        start_y = floor(minimum_y) - 1;
        columns = ceil(maximum_x) - floor(minimum_x) + 3;
        rows = ceil(maximum_y) - floor(minimum_y) + 3;
    }
    if (!isfinite(start_x) || !isfinite(start_y) || !isfinite(columns) || !isfinite(rows) ||
        start_x < INT_MIN || start_y < INT_MIN || start_x > INT_MAX || start_y > INT_MAX || columns < 1 ||
        rows < 1 || columns > INT_MAX || rows > INT_MAX || start_x + columns - 1 > INT_MAX ||
        start_y + rows - 1 > INT_MAX) {
        return false;
    }
    uint64_t cells = (uint64_t)columns * (uint64_t)rows;
    if (cells > UINT32_MAX / 6 || cells > SIZE_MAX / (6 * sizeof(gdmf_tile_vertex))) {
        return false;
    }
    pending.start_x = (int)start_x;
    pending.start_y = (int)start_y;
    pending.columns = (int)columns;
    pending.rows = (int)rows;
    pending.required_vertices = (uint32_t)cells * 6;
    *plan = pending;
    return true;
}
