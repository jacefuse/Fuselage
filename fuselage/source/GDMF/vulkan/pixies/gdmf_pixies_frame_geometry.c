// Display transforms and conservative scissors. SEE: (GDMF_PIXIES.md, Frame preparation)
#include "gdmf_pixies_frame.h"
#include <math.h>

static void gdmf_pixies_frame_corners(const gdmf_pixies_state* state, float x[4], float y[4]) {
    x[0] = x[2] = state->position_x;
    x[1] = x[3] = state->position_x + state->display_width;
    y[0] = y[1] = state->position_y;
    y[2] = y[3] = state->position_y + state->display_height;
    float pivot_x = state->position_x + state->display_width * 0.5f;
    float pivot_y = state->position_y + state->display_height * 0.5f;
    if (state->rotate_around_hotspot) {
        pivot_x = state->position_x + (state->hotspot_x / (float)state->output_width) * state->display_width;
        pivot_y =
            state->position_y + (state->hotspot_y / (float)state->output_height) * state->display_height;
    }
    if (state->rotation_radians != 0) {
        float sine = sinf(state->rotation_radians), cosine = cosf(state->rotation_radians);
        for (int corner = 0; corner < 4; corner++) {
            float delta_x = x[corner] - pivot_x, delta_y = y[corner] - pivot_y;
            x[corner] = pivot_x + delta_x * cosine - delta_y * sine;
            y[corner] = pivot_y + delta_x * sine + delta_y * cosine;
        }
    }
}

void gdmf_pixies_frame_quad(const gdmf_pixies_state* state, float canvas_width, float canvas_height,
                            gdmf_pixies_texture_vertex vertices[6]) {
    float x[4], y[4];
    gdmf_pixies_frame_corners(state, x, y);
    static const unsigned order[6] = {0, 1, 2, 1, 3, 2};
    static const float uv[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (unsigned vertex = 0; vertex < 6; vertex++) {
        unsigned corner = order[vertex];
        vertices[vertex] = (gdmf_pixies_texture_vertex){
            {(x[corner] / canvas_width) * 2.0f - 1.0f, (y[corner] / canvas_height) * 2.0f - 1.0f},
            {uv[corner][0], uv[corner][1]}};
    }
}

bool gdmf_pixies_frame_scissor(const gdmf_pixies_state* state, float canvas_width, float canvas_height,
                               VkRect2D viewport, VkRect2D* scissor) {
    if (!state || !scissor || !isfinite(canvas_width) || !isfinite(canvas_height) || canvas_width <= 0 ||
        canvas_height <= 0 || viewport.offset.x < 0 || viewport.offset.y < 0 ||
        viewport.extent.width > (uint32_t)(INT32_MAX - viewport.offset.x) ||
        viewport.extent.height > (uint32_t)(INT32_MAX - viewport.offset.y))
        return false;
    float x[4], y[4];
    gdmf_pixies_frame_corners(state, x, y);
    double minimum_x = INFINITY, minimum_y = INFINITY, maximum_x = -INFINITY, maximum_y = -INFINITY;
    for (unsigned corner = 0; corner < 4; corner++) {
        if (!isfinite(x[corner]) || !isfinite(y[corner]))
            return false;
        double pixel_x = (double)x[corner] / canvas_width * viewport.extent.width;
        double pixel_y = (double)y[corner] / canvas_height * viewport.extent.height;
        minimum_x = fmin(minimum_x, pixel_x);
        maximum_x = fmax(maximum_x, pixel_x);
        minimum_y = fmin(minimum_y, pixel_y);
        maximum_y = fmax(maximum_y, pixel_y);
    }
    // Clamp in floating point before conversion; round outward to retain edges.
    minimum_x = fmax(0.0, fmin(viewport.extent.width, floor(minimum_x)));
    minimum_y = fmax(0.0, fmin(viewport.extent.height, floor(minimum_y)));
    maximum_x = fmax(0.0, fmin(viewport.extent.width, ceil(maximum_x)));
    maximum_y = fmax(0.0, fmin(viewport.extent.height, ceil(maximum_y)));
    if (maximum_x <= minimum_x || maximum_y <= minimum_y)
        return false;
    *scissor = (VkRect2D){{viewport.offset.x + (int32_t)minimum_x, viewport.offset.y + (int32_t)minimum_y},
                          {(uint32_t)(maximum_x - minimum_x), (uint32_t)(maximum_y - minimum_y)}};
    return true;
}
