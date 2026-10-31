#ifndef GDMF_PIXIES_COMMANDS_H
#define GDMF_PIXIES_COMMANDS_H

// Public command layouts. SEE: (GDMF_PIXIES_COMMANDS.md, Command packets)
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PIXIE_OP_SET_ATTR = 0,
    PIXIE_OP_UNPACK,
    PIXIE_OP_DRAW,
    PIXIE_OP_PLOT,
    PIXIE_OP_CLEAR,
    PIXIE_OP_SHOW,
    PIXIE_OP_HIDE,
    PIXIE_OP_EXECUTE,
    PIXIE_OP_SET_DRAW_PATTERN,
    PIXIE_OP_DRAW_POLY,
    PIXIE_OP_SET_TRANSFORM,
    PIXIE_OP_DRAW_LINES,
} PixieOpcode;

typedef enum {
    PIXIE_LINES_STRIP = 0,
    PIXIE_LINES_SEGMENTS = 1,
} PixieLinesMode;

typedef enum {
    PIXIE_POLY_FILLED = 0,
    PIXIE_POLY_LINES = 1,
    PIXIE_POLY_PATTERN = 2,
} PixiePolyMode;

typedef struct {
    uint8_t pixie; // Target pixie ID; each record may select a different target.
    uint8_t opcode;
    uint16_t flags;
    uint32_t args[4];
} PixieCommand;

typedef struct {
    uint32_t executed; // Records visited, including refusals; 0 on early rejection.
    uint32_t rejected;
    uint32_t firstRejectIndex; // PIXIE_LIST_NO_REJECT if none.
    uint32_t firstRejectOpcode;
    uint32_t firstRejectPixie;
} PixieListResult;

typedef struct {
    // x' = a*x + b*y + tx; y' = c*x + d*y + ty.
    float a, b, c, d;
    float tx, ty;
} PixieTransform;

#define PIXIE_PTS_FLOAT 0x0100u
#define PIXIE_PTS_13_3_BYTES 4u
#define PIXIE_PTS_FLOAT_BYTES 8u
#define PIXIE_LINES_MODE_MASK 0x000Fu
#define PIXIE_POLY_MAX_POINTS 256
#define PIXIE_POLY_MODE_MASK 0x000Fu
#define PIXIE_LIST_NO_REJECT 0xFFFFFFFFu
#define PIXIE_LIST_MAX_COMMANDS 65536u
#define PIXIE_LIST_MAX_DEPTH 8
#define PIXIE_TRANSFORM_SLOTS 16

// Preserve a float bit pattern in one argument word.
uint32_t PackPixieFloatArg(float value);

// Clamps to signed 13.3 using the current rounding mode; NaN becomes zero.
uint32_t PackPixieDrawXY(float coordinate_x, float coordinate_y);

// Clip the segment to the encoding range, then pack both endpoints.
// False for NULL output, nonfinite/overflowing arithmetic, or an outside segment.
// Output is unchanged on failure. SEE: (GDMF_PIXIES_COMMANDS.md, Coordinate packing)
bool PackPixieDrawLine(float start_x, float start_y, float end_x, float end_y, uint32_t packed_endpoints[2]);

#endif // GDMF_PIXIES_COMMANDS_H
