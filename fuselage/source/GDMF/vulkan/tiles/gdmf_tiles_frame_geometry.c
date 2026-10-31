// Finite geometry preserves ENTERON operation order. SEE: (GDMF_TILES.md, Prepared frames)
#include "gdmf_tiles_frame.h"
#include <math.h>
#include <string.h>
static bool gdmf_tiles_geometry_finite(const gdmf_tile_vertex* vertices, uint32_t count) {
    for (uint32_t index = 0; index < count; ++index) {
        if (!isfinite(vertices[index].position[0]) || !isfinite(vertices[index].position[1])) {
            return false;
        }
    }
    return true;
}
static void gdmf_tiles_corner_uvs(bool flip_x, bool flip_y, bool quarter_turn, float uv[4][2]) {
    const float texture_left = flip_x ? 1.0f : 0.0f;
    const float texture_right = flip_x ? 0.0f : 1.0f;
    const float texture_top = flip_y ? 1.0f : 0.0f;
    const float texture_bottom = flip_y ? 0.0f : 1.0f;

    if (!quarter_turn) {
        uv[0][0] = texture_left;
        uv[0][1] = texture_top;
        uv[1][0] = texture_right;
        uv[1][1] = texture_top;
        uv[2][0] = texture_right;
        uv[2][1] = texture_bottom;
        uv[3][0] = texture_left;
        uv[3][1] = texture_bottom;
    } else {
        uv[0][0] = texture_left;
        uv[0][1] = texture_bottom;
        uv[1][0] = texture_left;
        uv[1][1] = texture_top;
        uv[2][0] = texture_right;
        uv[2][1] = texture_top;
        uv[3][0] = texture_right;
        uv[3][1] = texture_bottom;
    }

    return;
}

static bool gdmf_tiles_emit_turned(gdmf_tile_vertex* vertices, uint32_t* count, float origin_x,
                                   float origin_y, float edge_x_x, float edge_x_y, float edge_y_x,
                                   float edge_y_y, float bitmap_id, float palette, float opacity,
                                   float show_zero, bool flip_x, bool flip_y, bool quarter_turn) {
    float uv[4][2];
    gdmf_tiles_corner_uvs(flip_x, flip_y, quarter_turn, uv);

    const float corners_x[4] = {origin_x, origin_x + edge_x_x, origin_x + edge_x_x + edge_y_x,
                                origin_x + edge_y_x};
    const float corners_y[4] = {origin_y, origin_y + edge_x_y, origin_y + edge_x_y + edge_y_y,
                                origin_y + edge_y_y};

    const int order[6] = {0, 1, 3, 1, 2, 3};
    gdmf_tile_vertex quad[6];
    for (int vertex_index = 0; vertex_index < 6; vertex_index++) {
        const int corner = order[vertex_index];
        gdmf_tile_vertex* vertex = &quad[vertex_index];
        vertex->position[0] = corners_x[corner];
        vertex->position[1] = corners_y[corner];
        vertex->uv[0] = uv[corner][0];
        vertex->uv[1] = uv[corner][1];
        vertex->bitmap_id = bitmap_id;
        vertex->palette = palette;
        vertex->opacity = opacity;
        vertex->show_zero = show_zero;
    }
    bool finite = gdmf_tiles_geometry_finite(quad, 6);
    memcpy(&vertices[*count], quad, sizeof quad);
    *count += 6;

    return finite;
}

static bool gdmf_tiles_emit_axis(gdmf_tile_vertex* vertices, uint32_t* count, float canvas_width,
                                 float canvas_height, float screen_x, float screen_y, float screen_width,
                                 float screen_height, float bitmap_id, float palette, float opacity,
                                 float show_zero, bool flip_x, bool flip_y, bool quarter_turn) {

    if (quarter_turn) {
        screen_x += (screen_width - screen_height) * 0.5f;
        screen_y += (screen_height - screen_width) * 0.5f;
        float original_width = screen_width;
        screen_width = screen_height;
        screen_height = original_width;
    }

    float x0 = (screen_x / canvas_width) * 2.0f - 1.0f;
    float y0 = (screen_y / canvas_height) * 2.0f - 1.0f;
    float x1 = ((screen_x + screen_width) / canvas_width) * 2.0f - 1.0f;
    float y1 = ((screen_y + screen_height) / canvas_height) * 2.0f - 1.0f;

    float uv[4][2];
    gdmf_tiles_corner_uvs(flip_x, flip_y, quarter_turn, uv);
    const float top_left_u = uv[0][0], top_left_v = uv[0][1];
    const float top_right_u = uv[1][0], top_right_v = uv[1][1];
    const float bottom_right_u = uv[2][0], bottom_right_v = uv[2][1];
    const float bottom_left_u = uv[3][0], bottom_left_v = uv[3][1];

    gdmf_tile_vertex quad[6] = {
        {{x0, y0}, {top_left_u, top_left_v}, bitmap_id, palette, opacity, show_zero},
        {{x1, y0}, {top_right_u, top_right_v}, bitmap_id, palette, opacity, show_zero},
        {{x0, y1}, {bottom_left_u, bottom_left_v}, bitmap_id, palette, opacity, show_zero},
        {{x1, y0}, {top_right_u, top_right_v}, bitmap_id, palette, opacity, show_zero},
        {{x1, y1}, {bottom_right_u, bottom_right_v}, bitmap_id, palette, opacity, show_zero},
        {{x0, y1}, {bottom_left_u, bottom_left_v}, bitmap_id, palette, opacity, show_zero}};

    // Validate CPU-local values; reading GPU-mapped vertex memory can stall over PCIe.
    bool finite = gdmf_tiles_geometry_finite(quad, 6);
    memcpy(&vertices[*count], quad, sizeof(quad));
    *count += 6;

    return finite;
}
bool gdmf_tiles_geometry_build(const gdmf_tiles_geometry_context* context,
                               const gdmf_tiles_geometry_plan* plan, gdmf_tile_vertex* vertices,
                               uint32_t capacity, uint32_t* output_count) {
    if (!output_count) {
        return false;
    }
    *output_count = 0;
    if (!context || !context->map || !context->atlas || !plan || !vertices || !plan->required_vertices ||
        capacity < plan->required_vertices) {
        return false;
    }
    const TileMap* map = context->map;
    float tile_width = plan->tile_width, tile_height = plan->tile_height;
    double offset_x = plan->offset_x, offset_y = plan->offset_y;
    bool turned = plan->turned;
    float cosine = plan->cosine, sine = plan->sine;
    int start_x = plan->start_x, start_y = plan->start_y, columns = plan->columns, rows = plan->rows;
    uint32_t count = 0;
    bool finite = true;
    if (turned) {

        const float projection_x = 2.0f / (float)context->canvas_width;
        const float projection_y = 2.0f / (float)context->canvas_height;

        const float edge_x_x = (tile_width * cosine) * projection_x,
                    edge_x_y = (tile_width * sine) * projection_y;
        const float edge_y_x = (-tile_height * sine) * projection_x,
                    edge_y_y = (tile_height * cosine) * projection_y;

        const float swapped_x_x = (tile_height * cosine) * projection_x,
                    swapped_x_y = (tile_height * sine) * projection_y;
        const float swapped_y_x = (-tile_width * sine) * projection_x,
                    swapped_y_y = (tile_width * cosine) * projection_y;
        const float half_difference = (tile_width - tile_height) * 0.5f;
        const float shift_x = (half_difference * (cosine + sine)) * projection_x;
        const float shift_y = (half_difference * (sine - cosine)) * projection_y;

        const float pivot_x = map->viewportX + map->pivotX;
        const float pivot_y = map->viewportY + map->pivotY;
        const double base_offset_x = -offset_x - (double)map->pivotX;
        const double base_offset_y = -offset_y - (double)map->pivotY;
        const float base_x =
            (pivot_x + (float)(base_offset_x * cosine - base_offset_y * sine)) * projection_x - 1.0f;
        const float base_y =
            (pivot_y + (float)(base_offset_x * sine + base_offset_y * cosine)) * projection_y - 1.0f;

        float row_x = base_x + (float)start_x * edge_x_x + (float)start_y * edge_y_x;
        float row_y = base_y + (float)start_x * edge_x_y + (float)start_y * edge_y_y;

        for (int row = 0; row < rows; row++) {
            float origin_x = row_x, origin_y = row_y;

            for (int column = 0; column < columns; column++) {
                int map_x = start_x + column;
                int map_y = start_y + row;

                const float cell_origin_x = origin_x, cell_origin_y = origin_y;
                origin_x += edge_x_x;
                origin_y += edge_x_y;

                if (map->wrapX) {
                    map_x = ((map_x % (int)map->width) + (int)map->width) % (int)map->width;
                } else {
                    if (map_x < 0 || map_x >= (int)map->width) {
                        continue;
                    }
                }
                if (map->wrapY) {
                    map_y = ((map_y % (int)map->height) + (int)map->height) % (int)map->height;
                } else {
                    if (map_y < 0 || map_y >= (int)map->height) {
                        continue;
                    }
                }

                const TileLocation* cell = &map->location[map_x][map_y];
                uint16_t bitmap_id = cell->tileTypeID;
                if (!gdmf_tiles_atlas_bitmap_ready(context->atlas, bitmap_id)) {
                    continue;
                }

                float quad_x = cell_origin_x, quad_y = cell_origin_y;

                if (cell->offsetX != 0 || cell->offsetY != 0) {
                    const float nudge_x = (float)cell->offsetX;
                    const float nudge_y = (float)cell->offsetY;
                    quad_x += (nudge_x * cosine - nudge_y * sine) * projection_x;
                    quad_y += (nudge_x * sine + nudge_y * cosine) * projection_y;
                }

                const bool flip_x = (cell->flags & TILE_FLAG_HFLIP) != 0;
                const bool flip_y = (cell->flags & TILE_FLAG_VFLIP) != 0;
                const bool quarter_turn = (cell->flags & TILE_FLAG_ROT90) != 0;

                if (quarter_turn) {
                    quad_x += shift_x;
                    quad_y += shift_y;
                }

                finite &= gdmf_tiles_emit_turned(
                    vertices, &count, quad_x, quad_y, quarter_turn ? swapped_x_x : edge_x_x,
                    quarter_turn ? swapped_x_y : edge_x_y, quarter_turn ? swapped_y_x : edge_y_x,
                    quarter_turn ? swapped_y_y : edge_y_y, (float)bitmap_id, (float)cell->palette,
                    (float)cell->transparency, cell->showzero ? 1.0f : 0.0f, flip_x, flip_y, quarter_turn);
            }
            row_x += edge_y_x;
            row_y += edge_y_y;
        }

        *output_count = count;
        return finite;
    }

    float sub_offset_x = -(float)fmod(offset_x, tile_width);
    float sub_offset_y = -(float)fmod(offset_y, tile_height);

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            int map_x = start_x + column;
            int map_y = start_y + row;

            if (map->wrapX) {
                map_x = ((map_x % (int)map->width) + (int)map->width) % (int)map->width;
            } else {
                if (map_x < 0 || map_x >= (int)map->width) {
                    continue;
                }
            }
            if (map->wrapY) {
                map_y = ((map_y % (int)map->height) + (int)map->height) % (int)map->height;
            } else {
                if (map_y < 0 || map_y >= (int)map->height) {
                    continue;
                }
            }

            const TileLocation* cell = &map->location[map_x][map_y];
            uint16_t bitmap_id = cell->tileTypeID;
            if (!gdmf_tiles_atlas_bitmap_ready(context->atlas, bitmap_id)) {
                continue;
            }

            float screen_x = map->viewportX + (column - 1) * tile_width + sub_offset_x + (float)cell->offsetX;
            float screen_y = map->viewportY + (row - 1) * tile_height + sub_offset_y + (float)cell->offsetY;

            bool flip_x = (cell->flags & TILE_FLAG_HFLIP) != 0;
            bool flip_y = (cell->flags & TILE_FLAG_VFLIP) != 0;
            bool quarter_turn = (cell->flags & TILE_FLAG_ROT90) != 0;

            finite &= gdmf_tiles_emit_axis(vertices, &count, context->canvas_width, context->canvas_height, screen_x,
                                 screen_y, tile_width, tile_height, (float)bitmap_id, (float)cell->palette,
                                 (float)cell->transparency, cell->showzero ? 1.0f : 0.0f, flip_x, flip_y,
                                 quarter_turn);
        }
    }

    *output_count = count;
    return finite;
}
