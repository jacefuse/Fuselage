// Pixel-buffer drawing. SEE: (GDMF_PIXIES_COMMANDS.md, CPU drawing and live geometry)
#include "gdmf_pixies_drawing_private.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void gdmf_pixies_fill_polygon(gdmf_pixies_state* state, const float* point_x, const float* point_y, int count,
                              Color color, bool pattern) {
    if (count < 3) {
        return;
    }

    float minimum_y = point_y[0], maximum_y = point_y[0];
    for (int i = 1; i < count; i++) {
        if (point_y[i] < minimum_y) {
            minimum_y = point_y[i];
        }
        if (point_y[i] > maximum_y) {
            maximum_y = point_y[i];
        }
    }

    int first_row = (int)floorf(minimum_y);
    int last_row = (int)ceilf(maximum_y);
    if (first_row < 0) {
        first_row = 0;
    }
    if (last_row > state->output_height) {
        last_row = state->output_height;
    }

    float crossings[PIXIE_POLY_MAX_POINTS];

    for (int y = first_row; y < last_row; y++) {
        float row_center = (float)y + 0.5f;
        int crossing_count = 0;

        for (int i = 0, j = count - 1; i < count; j = i++) {
            float y0 = point_y[j], y1 = point_y[i];

            // Half-open edges count shared vertices once, at the row center.
            bool crosses = (y0 <= row_center && y1 > row_center) || (y1 <= row_center && y0 > row_center);
            if (!crosses) {
                continue;
            }

            float edge_fraction = (row_center - y0) / (y1 - y0);
            if (crossing_count < PIXIE_POLY_MAX_POINTS) {
                crossings[crossing_count++] = point_x[j] + edge_fraction * (point_x[i] - point_x[j]);
            }
        }
        if (crossing_count < 2) {
            continue;
        }

        for (int sorted_index = 1; sorted_index < crossing_count; sorted_index++) {
            float value = crossings[sorted_index];
            int insertion_index = sorted_index - 1;
            while (insertion_index >= 0 && crossings[insertion_index] > value) {
                crossings[insertion_index + 1] = crossings[insertion_index];
                insertion_index--;
            }
            crossings[insertion_index + 1] = value;
        }

        for (int span = 0; span + 1 < crossing_count; span += 2) {
            int first_column = (int)ceilf(crossings[span] - 0.5f);
            int last_column = (int)ceilf(crossings[span + 1] - 0.5f);
            if (first_column < 0) {
                first_column = 0;
            }
            if (last_column > state->output_width) {
                last_column = state->output_width;
            }

            if (!pattern) {
                for (int x = first_column; x < last_column; x++) {
                    state->output[y * state->output_width + x] = color;
                }
                continue;
            }

            if (state->pattern_width && state->pattern_height) {
                const unsigned char* pattern_pixels = state->ram + state->pattern_offset;
                int pattern_y = y % (int)state->pattern_height;
                for (int x = first_column; x < last_column; x++) {
                    int pattern_x = x % (int)state->pattern_width;
                    const unsigned char* texel =
                        pattern_pixels + ((size_t)pattern_y * state->pattern_width + pattern_x) * 4u;
                    if (texel[3] == 0) {
                        continue;
                    }
                    Color sample = {texel[0], texel[1], texel[2], texel[3]};
                    state->output[y * state->output_width + x] = sample;
                }
            } else {

                for (int x = first_column; x < last_column; x++) {
                    if (state->draw_pattern & (1u << (((unsigned)(x + y)) & 15u))) {
                        state->output[y * state->output_width + x] = color;
                    }
                }
            }
        }
    }

    return;
}

void gdmf_pixies_fill_mask_polygon(gdmf_pixies_state* state, const float* point_x, const float* point_y,
                                   int count, uint8_t value, bool pattern) {
    if (count < 3 || !state->mask) {
        return;
    }

    float minimum_y = point_y[0], maximum_y = point_y[0];
    for (int i = 1; i < count; i++) {
        if (point_y[i] < minimum_y) {
            minimum_y = point_y[i];
        }
        if (point_y[i] > maximum_y) {
            maximum_y = point_y[i];
        }
    }

    int first_row = (int)floorf(minimum_y);
    int last_row = (int)ceilf(maximum_y);
    if (first_row < 0) {
        first_row = 0;
    }
    if (last_row > state->output_height) {
        last_row = state->output_height;
    }

    float crossings[PIXIE_POLY_MAX_POINTS];

    for (int y = first_row; y < last_row; y++) {
        float row_center = (float)y + 0.5f;
        int crossing_count = 0;

        for (int i = 0, j = count - 1; i < count; j = i++) {
            float y0 = point_y[j], y1 = point_y[i];
            bool crosses = (y0 <= row_center && y1 > row_center) || (y1 <= row_center && y0 > row_center);
            if (!crosses) {
                continue;
            }

            float edge_fraction = (row_center - y0) / (y1 - y0);
            if (crossing_count < PIXIE_POLY_MAX_POINTS) {
                crossings[crossing_count++] = point_x[j] + edge_fraction * (point_x[i] - point_x[j]);
            }
        }
        if (crossing_count < 2) {
            continue;
        }

        for (int sorted_index = 1; sorted_index < crossing_count; sorted_index++) {
            float value = crossings[sorted_index];
            int insertion_index = sorted_index - 1;
            while (insertion_index >= 0 && crossings[insertion_index] > value) {
                crossings[insertion_index + 1] = crossings[insertion_index];
                insertion_index--;
            }
            crossings[insertion_index + 1] = value;
        }

        for (int span = 0; span + 1 < crossing_count; span += 2) {
            int first_column = (int)ceilf(crossings[span] - 0.5f);
            int last_column = (int)ceilf(crossings[span + 1] - 0.5f);
            if (first_column < 0) {
                first_column = 0;
            }
            if (last_column > state->output_width) {
                last_column = state->output_width;
            }
            for (int x = first_column; x < last_column; x++) {

                if (pattern && !(state->draw_pattern & (1u << (((unsigned)(x + y)) & 15u)))) {
                    continue;
                }
                state->mask[y * state->output_width + x] = value;
            }
        }
    }

    return;
}

static void gdmf_pixies_stamp_square(gdmf_pixies_state* state, int center_x, int center_y, int width,
                                     Color color) {
    int half = width / 2;

    for (int delta_y = 0; delta_y < width; delta_y++) {
        int y = center_y - half + delta_y;

        if (y < 0 || y >= state->output_height) {
            continue;
        }
        for (int delta_x = 0; delta_x < width; delta_x++) {
            int x = center_x - half + delta_x;

            if (x < 0 || x >= state->output_width) {
                continue;
            }
            state->output[y * state->output_width + x] = color;
        }
    }

    return;
}

static void gdmf_pixies_draw_line(gdmf_pixies_state* state, int x0, int y0, int x1, int y1, Color color,
                                  int width) {
    int delta_x = abs(x1 - x0), step_x = (x0 < x1) ? 1 : -1;
    int delta_y = -abs(y1 - y0), step_y = (y0 < y1) ? 1 : -1;
    int error = delta_x + delta_y;
    int step = 0;

    for (;;) {
        if ((state->draw_pattern & (uint16_t)(1u << (step & 15))) != 0) {
            if (width <= 1) {
                if (x0 >= 0 && x0 < state->output_width && y0 >= 0 && y0 < state->output_height) {
                    state->output[y0 * state->output_width + x0] = color;
                }
            } else {
                gdmf_pixies_stamp_square(state, x0, y0, width, color);
            }
        }
        step++;
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int doubled_error = 2 * error;
        if (doubled_error >= delta_y) {
            error += delta_y;
            x0 += step_x;
        }
        if (doubled_error <= delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }

    return;
}
static void gdmf_pixies_draw_mask_line(gdmf_pixies_state* state, int x0, int y0, int x1, int y1,
                                       uint8_t value, int width) {
    // Preserved mask behavior: no line pattern; even widths stamp width + 1.
    // SEE: (GDMF_PIXIES_COMMANDS.md, CPU drawing and live geometry)
    if (width < 1) {
        width = 1;
    }
    int half = width / 2;

    int delta_x = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int delta_y = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int step_x = (x0 < x1) ? 1 : -1;
    int step_y = (y0 < y1) ? 1 : -1;
    int error = delta_x - delta_y;

    for (;;) {
        for (int offset_y = -half; offset_y <= half; offset_y++) {
            for (int offset_x = -half; offset_x <= half; offset_x++) {
                int point_x = x0 + offset_x, point_y = y0 + offset_y;
                if (point_x >= 0 && point_x < state->output_width && point_y >= 0 &&
                    point_y < state->output_height) {
                    state->mask[(size_t)point_y * state->output_width + point_x] = value;
                }
            }
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int doubled_error = 2 * error;
        if (doubled_error > -delta_y) {
            error -= delta_y;
            x0 += step_x;
        }
        if (doubled_error < delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}

void gdmf_pixies_emit_line(gdmf_pixies_state* state, float x0, float y0, float x1, float y1,
                           uint32_t color_word, int width) {
    if (state->mode == PIXIE_MODE_MASK) {
        gdmf_pixies_draw_mask_line(state, (int)lrintf(x0), (int)lrintf(y0), (int)lrintf(x1), (int)lrintf(y1),
                                   (uint8_t)(color_word & 0xFFu), width);
        state->dirty = true;
    } else if (state->mode == PIXIE_MODE_TEXTURE) {
        gdmf_pixies_draw_line(state, (int)lrintf(x0), (int)lrintf(y0), (int)lrintf(x1), (int)lrintf(y1),
                              gdmf_pixies_unpack_color(color_word), width);
        state->dirty = true;
    } else {
        gdmf_pixies_live_push_line(state, x0, y0, x1, y1, (float)width, gdmf_pixies_unpack_color(color_word));
    }

    return;
}
