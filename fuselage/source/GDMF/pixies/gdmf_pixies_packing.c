// Command argument encoding. SEE: (GDMF_PIXIES_COMMANDS.md, Coordinate packing)
#include "gdmf_pixies_commands.h"
#include <float.h>
#include <math.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Pixie arguments require binary32 floats");

#define GDMF_PIXIES_COORDINATE_MIN (-4096.0f)
#define GDMF_PIXIES_COORDINATE_MAX (4095.875f)
#define GDMF_PIXIES_COORDINATE_SCALE 8.0f

uint32_t PackPixieFloatArg(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int32_t gdmf_pixies_pack_coordinate(float coordinate) {
    if (isnan(coordinate)) {
        return 0;
    }
    // Clamp before conversion; a large finite float need not fit in an integer.
    if (coordinate <= GDMF_PIXIES_COORDINATE_MIN) {
        return -32768;
    }
    if (coordinate >= GDMF_PIXIES_COORDINATE_MAX) {
        return 32767;
    }
    return (int32_t)lrintf(coordinate * GDMF_PIXIES_COORDINATE_SCALE);
}

uint32_t PackPixieDrawXY(float coordinate_x, float coordinate_y) {
    int32_t fixed_x = gdmf_pixies_pack_coordinate(coordinate_x);
    int32_t fixed_y = gdmf_pixies_pack_coordinate(coordinate_y);
    return (((uint32_t)fixed_x & 0xFFFFu) << 16) | ((uint32_t)fixed_y & 0xFFFFu);
}

static bool gdmf_pixies_clip_parameter(float edge_direction, float edge_distance, float* start_fraction,
                                       float* end_fraction) {
    if (edge_direction == 0.0f) {
        return edge_distance >= 0.0f;
    }
    float fraction = edge_distance / edge_direction;
    if (edge_direction < 0.0f) {
        if (fraction > *end_fraction) {
            return false;
        }
        if (fraction > *start_fraction) {
            *start_fraction = fraction;
        }
    } else {
        if (fraction < *start_fraction) {
            return false;
        }
        if (fraction < *end_fraction) {
            *end_fraction = fraction;
        }
    }
    return true;
}

bool PackPixieDrawLine(float start_x, float start_y, float end_x, float end_y, uint32_t packed_endpoints[2]) {
    if (!packed_endpoints || !isfinite(start_x) || !isfinite(start_y) || !isfinite(end_x) ||
        !isfinite(end_y)) {
        return false;
    }

    if (start_x >= GDMF_PIXIES_COORDINATE_MIN && start_x <= GDMF_PIXIES_COORDINATE_MAX &&
        start_y >= GDMF_PIXIES_COORDINATE_MIN && start_y <= GDMF_PIXIES_COORDINATE_MAX &&
        end_x >= GDMF_PIXIES_COORDINATE_MIN && end_x <= GDMF_PIXIES_COORDINATE_MAX &&
        end_y >= GDMF_PIXIES_COORDINATE_MIN && end_y <= GDMF_PIXIES_COORDINATE_MAX) {
        packed_endpoints[0] = PackPixieDrawXY(start_x, start_y);
        packed_endpoints[1] = PackPixieDrawXY(end_x, end_y);
        return true;
    }

    float delta_x = end_x - start_x, delta_y = end_y - start_y;
    if (!isfinite(delta_x) || !isfinite(delta_y)) {
        return false;
    }
    float start_fraction = 0.0f, end_fraction = 1.0f;
    if (!gdmf_pixies_clip_parameter(-delta_x, start_x - GDMF_PIXIES_COORDINATE_MIN, &start_fraction,
                                    &end_fraction) ||
        !gdmf_pixies_clip_parameter(delta_x, GDMF_PIXIES_COORDINATE_MAX - start_x, &start_fraction,
                                    &end_fraction) ||
        !gdmf_pixies_clip_parameter(-delta_y, start_y - GDMF_PIXIES_COORDINATE_MIN, &start_fraction,
                                    &end_fraction) ||
        !gdmf_pixies_clip_parameter(delta_y, GDMF_PIXIES_COORDINATE_MAX - start_y, &start_fraction,
                                    &end_fraction)) {
        return false;
    }
    packed_endpoints[0] =
        PackPixieDrawXY(start_x + start_fraction * delta_x, start_y + start_fraction * delta_y);
    packed_endpoints[1] = PackPixieDrawXY(start_x + end_fraction * delta_x, start_y + end_fraction * delta_y);
    return true;
}
