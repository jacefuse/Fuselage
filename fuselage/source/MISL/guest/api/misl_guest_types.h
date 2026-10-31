#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../../misl_abi.h"

// Guest-owned mirrors; preserve wire values and field layouts.
// SEE: (MISL_GUEST.md, Types and subsystem wrappers)
#ifndef FUSELAGE_COLOR_TYPE
#define FUSELAGE_COLOR_TYPE
typedef struct {
    unsigned char r, g, b, a;
} Color;
#endif

typedef int16_t SpriteBitmapID;

#define SPRITE_BITMAP_NONE -1
#define SPRITE_TEST_PATTERN_BITMAP_ID 0

typedef enum { PIXIE_MODE_TEXTURE = 0, PIXIE_MODE_LIVE = 1, PIXIE_MODE_MASK = 2 } PixieMode;
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
    PIXIE_OP_DRAW_LINES
} PixieOpcode;

#define PIXIE_PTS_FLOAT 0x0100u
#define PIXIE_PTS_13_3_BYTES 4u
#define PIXIE_PTS_FLOAT_BYTES 8u

typedef enum { PIXIE_LINES_STRIP = 0, PIXIE_LINES_SEGMENTS = 1 } PixieLinesMode;
#define PIXIE_LINES_MODE_MASK 0x000Fu

#define PIXIE_TRANSFORM_SLOTS 16
typedef struct {
    float a, b, c, d, tx, ty;
} PixieTransform;
#ifdef __cplusplus
static_assert(sizeof(PixieTransform) == 24, "PixieTransform must stay 24 bytes; the host reads this layout");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(PixieTransform) == 24, "PixieTransform must stay 24 bytes; the host reads this layout");
#endif

#define PIXIE_POLY_MAX_POINTS 256

#define PIXIE_POLY_MODE_MASK 0x000Fu

typedef enum {
    PIXIE_POLY_FILLED = 0,
    PIXIE_POLY_LINES = 1,
    PIXIE_POLY_PATTERN = 2,
} PixiePolyMode;

typedef enum {
    PIXIE_FORMAT_RGBA8 = 0,
    PIXIE_FORMAT_PALETTE4BPP,
    PIXIE_FORMAT_RLE_PALETTE_SHARED,
    PIXIE_FORMAT_RLE_PALETTE_OWN,
    PIXIE_FORMAT_RLE_RGBA8,

    PIXIE_FORMAT_PALETTE8BPP
} PixieUnpackFormat;
typedef struct {
    short a, b;
} SpritePairRef;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(SpritePairRef) == 4, "SpritePairRef must stay two int16; the ecall states this width");
#endif
typedef enum {
    COLLISION_TYPE_NONE = 0,
    COLLISION_TYPE_BOUNDING_BOX = MISL_COLLISION_BOUNDING_BOX,
    COLLISION_TYPE_ANY_COLOR = MISL_COLLISION_ANY_COLOR,
    COLLISION_TYPE_COLLIDABLE_COLORS = MISL_COLLISION_COLLIDABLE_COLORS,
} SpriteCollisionType;
typedef struct {
    uint16_t x, y;
} TileCellRef;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(TileCellRef) == 4, "TileCellRef must stay two uint16; the ecall states this width");
#endif
#define PIXIE_DRAW_COORD_MIN (-4096.0f)
#define PIXIE_DRAW_COORD_MAX (4095.875f)
#define FUSELAGE_DEEP_PALETTE_COUNT 16
#define FUSELAGE_DEEP_PALETTE_SIZE 256
#define SHARP_CHIP_ADORA 0u
#define SHARP_CHIP_SWEENY 1u
#define SHARP_CHIP_AMY 2u
#define SHARP_CHIP_JENNY 3u
