// Retained live geometry. SEE: (GDMF_PIXIES_COMMANDS.md, CPU drawing and live geometry)
#include "gdmf_pixies_drawing_private.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../window/gdmf_canvas.h"

static float gdmf_pixies_world_to_ndc_x(float world_x) {
    return (world_x / ((float)GDMF_GetCanvasWidth())) * 2.0f - 1.0f;
}
static float gdmf_pixies_world_to_ndc_y(float world_y) {
    return (world_y / ((float)GDMF_GetCanvasHeight())) * 2.0f - 1.0f;
}

static void gdmf_pixies_rotate_point(float* point_x, float* point_y, float pivot_x, float pivot_y, float sine,
                                     float cosine);
static void gdmf_pixies_pivot(const gdmf_pixies_state* state, float* pivot_x, float* pivot_y);

static void gdmf_pixies_local_to_ndc(const gdmf_pixies_state* state, float local_x, float local_y,
                                     float* normalized_x, float* normalized_y) {
    float world_x =
        (float)state->position_x + (local_x / (float)state->output_width) * (float)state->display_width;
    float world_y =
        (float)state->position_y + (local_y / (float)state->output_height) * (float)state->display_height;

    if (state->rotation_radians != 0.0f) {
        float pivot_x, pivot_y;
        gdmf_pixies_pivot(state, &pivot_x, &pivot_y);
        gdmf_pixies_rotate_point(&world_x, &world_y, pivot_x, pivot_y, sinf(state->rotation_radians),
                                 cosf(state->rotation_radians));
    }

    *normalized_x = gdmf_pixies_world_to_ndc_x(world_x);
    *normalized_y = gdmf_pixies_world_to_ndc_y(world_y);

    return;
}

void gdmf_pixies_live_push_quad(gdmf_pixies_state* state, float center_x, float center_y, float half_width,
                                float half_height, Color color) {
    if (state->live_vertex_count + 6 > state->live_vertex_capacity) {
        return;
    }

    float components[4] = {(float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f,
                           (float)color.a / 255.0f};

    float corners[4][2] = {{center_x - half_width, center_y - half_height},
                           {center_x + half_width, center_y - half_height},
                           {center_x - half_width, center_y + half_height},
                           {center_x + half_width, center_y + half_height}};
    float normalized_corners[4][2];
    for (int corner = 0; corner < 4; corner++) {
        gdmf_pixies_local_to_ndc(state, corners[corner][0], corners[corner][1],
                                 &normalized_corners[corner][0], &normalized_corners[corner][1]);
    }

    static const int order[6] = {0, 1, 2, 1, 3, 2};
    gdmf_pixies_live_vertex* vertices = &state->live_vertices[state->live_vertex_count];
    for (int i = 0; i < 6; i++) {
        int corner = order[i];

        vertices[i].position[0] = normalized_corners[corner][0];
        vertices[i].position[1] = normalized_corners[corner][1];
        memcpy(vertices[i].color, components, sizeof(components));
    }
    state->live_vertex_count += 6;

    return;
}

static void gdmf_pixies_live_push_line_segment(gdmf_pixies_state* state, float x0, float y0, float x1,
                                               float y1, float width, Color color) {
    if (state->live_vertex_count + 6 > state->live_vertex_capacity) {
        return;
    }

    float delta_x = x1 - x0, delta_y = y1 - y0;
    float length = sqrtf(delta_x * delta_x + delta_y * delta_y);
    float normal_x = -delta_y / length, normal_y = delta_x / length;
    float half_width = width / 2.0f;
    float offset_x = normal_x * half_width, offset_y = normal_y * half_width;

    float components[4] = {(float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f,
                           (float)color.a / 255.0f};

    float corners[4][2] = {{x0 - offset_x, y0 - offset_y},
                           {x0 + offset_x, y0 + offset_y},
                           {x1 - offset_x, y1 - offset_y},
                           {x1 + offset_x, y1 + offset_y}};
    float normalized_corners[4][2];
    for (int corner = 0; corner < 4; corner++) {
        gdmf_pixies_local_to_ndc(state, corners[corner][0], corners[corner][1],
                                 &normalized_corners[corner][0], &normalized_corners[corner][1]);
    }

    static const int order[6] = {0, 1, 2, 1, 3, 2};
    gdmf_pixies_live_vertex* vertices = &state->live_vertices[state->live_vertex_count];
    for (int i = 0; i < 6; i++) {
        int corner = order[i];

        vertices[i].position[0] = normalized_corners[corner][0];
        vertices[i].position[1] = normalized_corners[corner][1];
        memcpy(vertices[i].color, components, sizeof(components));
    }
    state->live_vertex_count += 6;

    return;
}

void gdmf_pixies_live_push_line(gdmf_pixies_state* state, float x0, float y0, float x1, float y1, float width,
                                Color color) {

    float delta_x = x1 - x0, delta_y = y1 - y0;
    float length = sqrtf(delta_x * delta_x + delta_y * delta_y);

    if (length < 0.0001f) {
        float half = width / 2.0f;

        gdmf_pixies_live_push_quad(state, x0, y0, half, half, color);
        return;
    }

    if (state->draw_pattern == 0xFFFF) {
        gdmf_pixies_live_push_line_segment(state, x0, y0, x1, y1, width, color);
        return;
    }

    {
        int total_steps = (int)(length + 0.5f);
        int i = 0;

        while (i < total_steps) {
            int run_start, run_end;
            float start_fraction, end_fraction;

            if ((state->draw_pattern & (uint16_t)(1u << (i & 15))) == 0) {
                i++;
                continue;
            }

            run_start = i;
            while (i < total_steps && (state->draw_pattern & (uint16_t)(1u << (i & 15))) != 0) {
                i++;
            }
            run_end = i;

            start_fraction = (float)run_start / length;
            end_fraction = (float)run_end / length;
            gdmf_pixies_live_push_line_segment(state, x0 + delta_x * start_fraction,
                                               y0 + delta_y * start_fraction, x0 + delta_x * end_fraction,
                                               y0 + delta_y * end_fraction, width, color);
        }
    }

    return;
}

static void gdmf_pixies_rotate_point(float* point_x, float* point_y, float pivot_x, float pivot_y, float sine,
                                     float cosine) {
    float delta_x = *point_x - pivot_x;
    float delta_y = *point_y - pivot_y;
    *point_x = pivot_x + delta_x * cosine - delta_y * sine;
    *point_y = pivot_y + delta_x * sine + delta_y * cosine;
}

static void gdmf_pixies_pivot(const gdmf_pixies_state* state, float* pivot_x, float* pivot_y) {
    if (state->rotate_around_hotspot && state->output_width > 0 && state->output_height > 0) {
        *pivot_x = state->position_x + (state->hotspot_x / (float)state->output_width) * state->display_width;
        *pivot_y =
            state->position_y + (state->hotspot_y / (float)state->output_height) * state->display_height;
    } else {
        *pivot_x = state->position_x + state->display_width * 0.5f;
        *pivot_y = state->position_y + state->display_height * 0.5f;
    }
}
