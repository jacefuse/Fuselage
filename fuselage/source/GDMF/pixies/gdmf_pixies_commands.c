// Command execution and list scope. SEE: (GDMF_PIXIES_COMMANDS.md, Lists and results)
#include "gdmf_pixies_drawing_private.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static PixieTransform gdmf_pixies_current_transform = {1, 0, 0, 1, 0, 0};
static PixieTransform gdmf_pixies_base_transform = {1, 0, 0, 1, 0, 0};
// Submission is serialized; current/base belong to interpreter scope, across IDs.
#define GDMF_PIXIES_CLEAR_USE_COLOR_BIT 0x0001u
#define GDMF_PIXIES_DRAW_MAX_WIDTH 256u
static uint32_t gdmf_pixies_run_list(const PixieCommand*, uint32_t, PixieListResult*, const PixieTransform*);
static void gdmf_pixies_apply_current_transform(float* coordinate_x, float* coordinate_y) {
    gdmf_pixies_transform_apply(&gdmf_pixies_current_transform, coordinate_x, coordinate_y);
}
static float gdmf_pixies_arg_to_float(uint32_t arg) {
    float value;
    memcpy(&value, &arg, sizeof value);
    return value;
}

static void gdmf_pixies_unpack_xy_draw(uint32_t packed, float* x, float* y) {
    *x = (float)(int16_t)((packed >> 16) & 0xFFFFu) / 8.0f;
    *y = (float)(int16_t)(packed & 0xFFFFu) / 8.0f;

    return;
}

static void gdmf_pixies_read_point(const unsigned char* points, uint32_t i, bool float_points, float* x,
                                   float* y) {
    if (float_points) {
        memcpy(x, points + (size_t)i * PIXIE_PTS_FLOAT_BYTES, sizeof *x);
        memcpy(y, points + (size_t)i * PIXIE_PTS_FLOAT_BYTES + 4, sizeof *y);
        return;
    }
    uint32_t packed;
    memcpy(&packed, points + (size_t)i * PIXIE_PTS_13_3_BYTES, sizeof packed);
    gdmf_pixies_unpack_xy_draw(packed, x, y);

    return;
}

static int gdmf_pixies_line_width(uint32_t arg) {
    // Clamp before converting the unsigned packet value to int.
    if (arg == 0) {
        return 1;
    }
    return arg > GDMF_PIXIES_DRAW_MAX_WIDTH ? GDMF_PIXIES_DRAW_MAX_WIDTH : (int)arg;
}
bool IssuePixieCommand(int id, PixieOpcode opcode, uint16_t flags, const uint32_t args[4]) {

    if (id < 0 || id >= MAX_PIXIES) {
        return gdmf_pixies_refuse(id, "pixie %d: id out of range (0..%d)", id, MAX_PIXIES - 1);
    }
    if (!gdmf_pixies_state_get(id)) {
        return gdmf_pixies_refuse(id, "pixie %d: not initialized; call InitPixie first", id);
    }
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);

    switch (opcode) {
    case PIXIE_OP_DRAW:
    case PIXIE_OP_DRAW_LINES:
    case PIXIE_OP_PLOT:
    case PIXIE_OP_CLEAR:
    case PIXIE_OP_DRAW_POLY:
        if (state->gpu_resident) {
            return gdmf_pixies_refuse(id,
                                      "pixie %d is GPU-resident (a kernel wrote its "
                                      "pixels) and the host may not draw on it; "
                                      "InitPixie makes it a canvas again",
                                      id);
        }
        break;
    default:
        break;
    }

    switch (opcode) {
    case PIXIE_OP_SET_ATTR: {

        if (!args) {
            return gdmf_pixies_refuse(id, "SET_ATTR: args is NULL");
        }
        float position_x = gdmf_pixies_arg_to_float(args[0]);
        float position_y = gdmf_pixies_arg_to_float(args[1]);
        float display_width = gdmf_pixies_arg_to_float(args[2]);
        float display_height = gdmf_pixies_arg_to_float(args[3]);
        if (!(display_width > 0.0f) || !(display_height > 0.0f)) {

            return gdmf_pixies_refuse(id, "SET_ATTR: display size %g x %g; both must be positive",
                                      (double)display_width, (double)display_height);
        }
        state->position_x = position_x;
        state->position_y = position_y;
        state->display_width = display_width;
        state->display_height = display_height;
        state->priority = (unsigned char)(flags & 0xFFu);
        state->enabled = (flags & GDMF_PIXIES_SET_ATTR_ENABLED_BIT) != 0;
        return true;
    }

    case PIXIE_OP_UNPACK:
        return gdmf_pixies_unpack_command(id, state, flags, args);

    case PIXIE_OP_DRAW: {
        if (!args) {
            return gdmf_pixies_refuse(id, "DRAW: args is NULL");
        }
        float start_x, start_y, end_x, end_y;
        gdmf_pixies_unpack_xy_draw(args[0], &start_x, &start_y);
        gdmf_pixies_unpack_xy_draw(args[1], &end_x, &end_y);

        gdmf_pixies_apply_current_transform(&start_x, &start_y);
        gdmf_pixies_apply_current_transform(&end_x, &end_y);
        gdmf_pixies_emit_line(state, start_x, start_y, end_x, end_y, args[2],
                              gdmf_pixies_line_width(args[3]));
        return true;
    }

    case PIXIE_OP_DRAW_LINES: {
        if (!args) {
            return gdmf_pixies_refuse(id, "DRAW_LINES: args is NULL");
        }

        const uint32_t line_mode = flags & PIXIE_LINES_MODE_MASK;
        const bool float_points = (flags & PIXIE_PTS_FLOAT) != 0;
        if (line_mode > PIXIE_LINES_SEGMENTS) {
            return gdmf_pixies_refuse(id,
                                      "DRAW_LINES: line mode %u unknown "
                                      "(0 = STRIP, 1 = SEGMENTS)",
                                      (unsigned)line_mode);
        }

        if ((flags & ~(uint16_t)(PIXIE_LINES_MODE_MASK | PIXIE_PTS_FLOAT)) != 0) {
            return gdmf_pixies_refuse(id,
                                      "DRAW_LINES: flags 0x%04X sets bits this build "
                                      "does not define (known: mode 0x%04X, "
                                      "PIXIE_PTS_FLOAT 0x%04X)",
                                      (unsigned)flags, (unsigned)PIXIE_LINES_MODE_MASK,
                                      (unsigned)PIXIE_PTS_FLOAT);
        }

        const uint32_t count = args[1];
        if (count < 2) {
            return gdmf_pixies_refuse(id, "DRAW_LINES: %u vertices; a segment needs 2", (unsigned)count);
        }

        const size_t point_stride = float_points ? PIXIE_PTS_FLOAT_BYTES : PIXIE_PTS_13_3_BYTES;
        const size_t offset = (size_t)args[0];
        if (offset > PIXIE_RAM_SIZE || count > (PIXIE_RAM_SIZE - offset) / point_stride) {
            return gdmf_pixies_refuse(id,
                                      "DRAW_LINES: %u vertices x %zu bytes at RAM offset "
                                      "%zu runs past the %u-byte RAM",
                                      (unsigned)count, point_stride, offset, (unsigned)PIXIE_RAM_SIZE);
        }
        if ((offset & 3u) != 0) {
            return gdmf_pixies_refuse(id, "DRAW_LINES: RAM offset %zu is not 4-byte aligned", offset);
        }

        const unsigned char* points = state->ram + offset;
        const int width = gdmf_pixies_line_width(args[3]);

        const uint32_t step = (line_mode == PIXIE_LINES_SEGMENTS) ? 2u : 1u;
        for (uint32_t i = 0; i + 1 < count; i += step) {
            float x0, y0, x1, y1;
            gdmf_pixies_read_point(points, i, float_points, &x0, &y0);
            gdmf_pixies_read_point(points, i + 1u, float_points, &x1, &y1);
            gdmf_pixies_apply_current_transform(&x0, &y0);
            gdmf_pixies_apply_current_transform(&x1, &y1);
            gdmf_pixies_emit_line(state, x0, y0, x1, y1, args[2], width);
        }
        return true;
    }

    case PIXIE_OP_PLOT: {
        if (!args) {
            return gdmf_pixies_refuse(id, "PLOT: args is NULL");
        }
        float coordinate_x = gdmf_pixies_arg_to_float(args[0]);
        float coordinate_y = gdmf_pixies_arg_to_float(args[1]);
        gdmf_pixies_apply_current_transform(&coordinate_x, &coordinate_y);
        Color color = gdmf_pixies_unpack_color(args[2]);

        if (state->mode == PIXIE_MODE_MASK) {
            int x = (int)lrintf(coordinate_x), y = (int)lrintf(coordinate_y);
            if (x < 0 || x >= state->output_width || y < 0 || y >= state->output_height) {

                return gdmf_pixies_refuse(id, "PLOT: (%d, %d) outside the %dx%d buffer", x, y,
                                          state->output_width, state->output_height);
            }

            state->mask[(size_t)y * state->output_width + x] = (uint8_t)(args[2] & 0xFFu);
            state->dirty = true;
        } else if (state->mode == PIXIE_MODE_TEXTURE) {
            int x = (int)lrintf(coordinate_x), y = (int)lrintf(coordinate_y);
            if (x < 0 || x >= state->output_width || y < 0 || y >= state->output_height) {
                return gdmf_pixies_refuse(id, "PLOT: (%d, %d) outside the %dx%d buffer", x, y,
                                          state->output_width, state->output_height);
            }
            state->output[y * state->output_width + x] = color;
            state->dirty = true;
        } else {
            gdmf_pixies_live_push_quad(state, coordinate_x, coordinate_y, 0.5f, 0.5f, color);
        }
        return true;
    }

    case PIXIE_OP_CLEAR: {
        bool use_color = (flags & GDMF_PIXIES_CLEAR_USE_COLOR_BIT) != 0;
        if (use_color && !args) {
            return gdmf_pixies_refuse(id, "CLEAR: flags asked for a fill color but args "
                                          "is NULL");
        }
        if (state->mode == PIXIE_MODE_MASK) {

            uint8_t value = use_color ? (uint8_t)(args[0] & 0xFFu) : 0u;
            memset(state->mask, value, (size_t)state->output_width * (size_t)state->output_height);
            state->dirty = true;
            return true;
        }
        Color fill = use_color ? gdmf_pixies_unpack_color(args[0]) : (Color){0, 0, 0, 0};

        if (state->mode == PIXIE_MODE_TEXTURE) {
            int pixel_count = state->output_width * state->output_height;

            for (int i = 0; i < pixel_count; i++) {
                state->output[i] = fill;
            }
            state->dirty = true;
        } else {
            state->live_vertex_count = 0;
            if (use_color) {
                gdmf_pixies_live_push_quad(
                    state, (float)state->output_width / 2.0f, (float)state->output_height / 2.0f,
                    (float)state->output_width / 2.0f, (float)state->output_height / 2.0f, fill);
            }
        }
        return true;
    }

    case PIXIE_OP_SHOW:
        state->shown = true;
        return true;

    case PIXIE_OP_HIDE:
        state->shown = false;
        return true;

    case PIXIE_OP_EXECUTE: {
        if (!args) {
            return gdmf_pixies_refuse(id, "EXECUTE: args is NULL");
        }

        uint32_t count = args[1];
        if (count == 0 || count > PIXIE_LIST_MAX_COMMANDS) {
            return gdmf_pixies_refuse(id, "EXECUTE: %u records (must be 1..%u)", (unsigned)count,
                                      (unsigned)PIXIE_LIST_MAX_COMMANDS);
        }

        size_t offset = (size_t)args[0];
        size_t bytes = (size_t)count * sizeof(PixieCommand);
        if (offset > PIXIE_RAM_SIZE || bytes > PIXIE_RAM_SIZE - offset) {
            return gdmf_pixies_refuse(id,
                                      "EXECUTE: %u records (%zu bytes) at RAM offset %zu "
                                      "runs past the %u-byte RAM",
                                      (unsigned)count, bytes, offset, (unsigned)PIXIE_RAM_SIZE);
        }

        if ((offset & 3u) != 0) {
            return gdmf_pixies_refuse(id, "EXECUTE: RAM offset %zu is not 4-byte aligned", offset);
        }

        uint32_t slot = args[2];
        if (slot >= PIXIE_TRANSFORM_SLOTS) {
            return gdmf_pixies_refuse(id, "EXECUTE: transform slot %u (must be 0..%d)", (unsigned)slot,
                                      PIXIE_TRANSFORM_SLOTS - 1);
        }
        PixieTransform under = (slot == 0) ? gdmf_pixies_current_transform
                                           : gdmf_pixies_transform_compose(&gdmf_pixies_current_transform,
                                                                           &state->transforms[slot]);

        gdmf_pixies_run_list((const PixieCommand*)(state->ram + offset), count, NULL, &under);
        return true;
    }

    case PIXIE_OP_SET_TRANSFORM: {
        if (!args) {
            return gdmf_pixies_refuse(id, "SET_TRANSFORM: args is NULL");
        }
        uint32_t slot = args[0];
        if (slot >= PIXIE_TRANSFORM_SLOTS) {
            return gdmf_pixies_refuse(id, "SET_TRANSFORM: slot %u (must be 0..%d)", (unsigned)slot,
                                      PIXIE_TRANSFORM_SLOTS - 1);
        }
        gdmf_pixies_current_transform =
            (slot == 0)
                ? gdmf_pixies_base_transform
                : gdmf_pixies_transform_compose(&gdmf_pixies_base_transform, &state->transforms[slot]);
        return true;
    }

    case PIXIE_OP_SET_DRAW_PATTERN:
        if (!args) {
            return gdmf_pixies_refuse(id, "SET_DRAW_PATTERN: args is NULL");
        }
        state->draw_pattern = (uint16_t)args[0];
        return true;

    case PIXIE_OP_DRAW_POLY: {
        if (!args) {
            return gdmf_pixies_refuse(id, "DRAW_POLY: args is NULL");
        }

        uint32_t count = args[1];
        if (count < 3 || count > PIXIE_POLY_MAX_POINTS) {

            return gdmf_pixies_refuse(id, "DRAW_POLY: %u points (must be 3..%d)", (unsigned)count,
                                      PIXIE_POLY_MAX_POINTS);
        }

        const bool float_points = (flags & PIXIE_PTS_FLOAT) != 0;
        PixiePolyMode polygon_mode = (PixiePolyMode)(flags & PIXIE_POLY_MODE_MASK);
        if (polygon_mode > PIXIE_POLY_PATTERN) {
            return gdmf_pixies_refuse(id,
                                      "DRAW_POLY: render mode %u unknown "
                                      "(0 = FILLED, 1 = LINES, 2 = PATTERN)",
                                      (unsigned)polygon_mode);
        }

        if ((flags & ~(uint16_t)(PIXIE_POLY_MODE_MASK | PIXIE_PTS_FLOAT)) != 0) {
            return gdmf_pixies_refuse(id,
                                      "DRAW_POLY: flags 0x%04X sets bits this build does "
                                      "not define (known: mode 0x%04X, PIXIE_PTS_FLOAT "
                                      "0x%04X)",
                                      (unsigned)flags, (unsigned)PIXIE_POLY_MODE_MASK,
                                      (unsigned)PIXIE_PTS_FLOAT);
        }

        const size_t point_stride = float_points ? PIXIE_PTS_FLOAT_BYTES : PIXIE_PTS_13_3_BYTES;
        size_t offset = (size_t)args[0];
        size_t bytes = (size_t)count * point_stride;
        if (offset > PIXIE_RAM_SIZE || bytes > PIXIE_RAM_SIZE - offset) {
            return gdmf_pixies_refuse(id,
                                      "DRAW_POLY: %u points x %zu bytes at RAM offset %zu "
                                      "runs past the %u-byte RAM",
                                      (unsigned)count, point_stride, offset, (unsigned)PIXIE_RAM_SIZE);
        }
        if ((offset & 3u) != 0) {
            return gdmf_pixies_refuse(id, "DRAW_POLY: RAM offset %zu is not 4-byte aligned", offset);
        }

        float point_x[PIXIE_POLY_MAX_POINTS];
        float point_y[PIXIE_POLY_MAX_POINTS];
        const unsigned char* points = state->ram + offset;
        for (uint32_t i = 0; i < count; i++) {
            gdmf_pixies_read_point(points, i, float_points, &point_x[i], &point_y[i]);
            gdmf_pixies_apply_current_transform(&point_x[i], &point_y[i]);
        }

        if (polygon_mode == PIXIE_POLY_LINES) {
            const int width = gdmf_pixies_line_width(args[3]);

            for (uint32_t i = 0, j = count - 1; i < count; j = i++) {
                gdmf_pixies_emit_line(state, point_x[j], point_y[j], point_x[i], point_y[i], args[2], width);
            }

            return true;
        }

        if (state->mode == PIXIE_MODE_MASK) {

            gdmf_pixies_fill_mask_polygon(state, point_x, point_y, (int)count, (uint8_t)(args[2] & 0xFFu),
                                          polygon_mode == PIXIE_POLY_PATTERN);
            state->dirty = true;
            return true;
        }
        if (state->mode == PIXIE_MODE_TEXTURE) {
            gdmf_pixies_fill_polygon(state, point_x, point_y, (int)count, gdmf_pixies_unpack_color(args[2]),
                                     polygon_mode == PIXIE_POLY_PATTERN);
            state->dirty = true;
            return true;
        }

        return gdmf_pixies_refuse(id, "DRAW_POLY: filled polygons require TEXTURE or MASK mode");
    }

    default:
        return gdmf_pixies_refuse(id, "opcode %d unknown", (int)opcode);
    }
}

static unsigned gdmf_pixies_list_depth = 0;

static uint32_t gdmf_pixies_run_list(const PixieCommand* commands, uint32_t count, PixieListResult* result,
                                     const PixieTransform* under) {
    if (result) {
        result->executed = 0;
        result->rejected = 0;
        result->firstRejectIndex = PIXIE_LIST_NO_REJECT;
        result->firstRejectOpcode = 0;
        result->firstRejectPixie = 0;
    }
    if (!commands || count == 0) {
        return 0;
    }

    if (gdmf_pixies_list_depth >= PIXIE_LIST_MAX_DEPTH) {
        if (result) {
            result->rejected = count;
            result->firstRejectIndex = 0;
        }
        return 0;
    }
    gdmf_pixies_list_depth++;

    const PixieTransform saved_current = gdmf_pixies_current_transform;
    const PixieTransform saved_base = gdmf_pixies_base_transform;
    gdmf_pixies_current_transform = *under;
    gdmf_pixies_base_transform = *under;

    char first_reason[PIXIE_OUTPUT_STRING_LEN];
    int first_reason_pixie = -1;

    uint32_t accepted = 0;
    for (uint32_t i = 0; i < count; i++) {
        // RAM lists contain bytes; copy before reading a record's typed fields.
        PixieCommand record;
        memcpy(&record, (const unsigned char*)commands + (size_t)i * sizeof record, sizeof record);
        const PixieCommand* command = &record;
        if (IssuePixieCommand((int)command->pixie, (PixieOpcode)command->opcode, command->flags,
                              command->args)) {
            accepted++;
            continue;
        }

        if (first_reason_pixie < 0) {
            first_reason_pixie = (int)command->pixie;
            memcpy(first_reason, gdmf_pixies_diagnostic_buffer(first_reason_pixie), sizeof first_reason);
        }
        if (result) {
            if (result->firstRejectIndex == PIXIE_LIST_NO_REJECT) {
                result->firstRejectIndex = i;
                result->firstRejectOpcode = command->opcode;
                result->firstRejectPixie = command->pixie;
            }
            result->rejected++;
        }
    }

    // Keep the diagnostic consistent with firstRejectIndex after later failures.
    if (first_reason_pixie >= 0) {
        memcpy(gdmf_pixies_diagnostic_buffer(first_reason_pixie), first_reason, sizeof first_reason);
    }

    gdmf_pixies_current_transform = saved_current;
    gdmf_pixies_base_transform = saved_base;
    gdmf_pixies_list_depth--;

    if (result) {
        result->executed = count;
    }
    return accepted;
}

uint32_t RunPixieCommandList(const PixieCommand* commands, uint32_t count, PixieListResult* result) {
    const PixieTransform under = gdmf_pixies_current_transform;

    return gdmf_pixies_run_list(commands, count, result, &under);
}
