#pragma once
#include "misl_guest_call_internal.h"

// Color and palette ecalls.
// SEE: (MISL_GUEST.md, Published colors)

static inline uint32_t PackColorsRGBA8(Color color) {
    return (uint32_t)MISL_GUEST_CALL(MISL_PAL_PACK_RGBA8, misl_guest_pack_color(color), 0, 0, 0, 0, 0, 0);
}

static inline bool SetColorsPalette(unsigned char palette_index, Color* palette) {
    return MISL_GUEST_CALL(MISL_PAL_LOAD_FROM_SPRITE, palette_index, misl_guest_address(palette), 0, 0, 0, 0,
                           0) != 0;
}

static inline Color GetColorsColorFromChar(unsigned char color_index) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_PAL_GET_FROM_CHAR, color_index, 0, 0, 0, 0, 0, 0));
}

static inline Color GetColorsCommodoreColor(unsigned char color_index) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_PAL_GET_COMMODORE, color_index, 0, 0, 0, 0, 0, 0));
}

static inline Color GetColorsTandyColor(unsigned char color_index) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_PAL_GET_TANDY, color_index, 0, 0, 0, 0, 0, 0));
}

static inline Color GetColorsANSIColor(unsigned char color_index) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_PAL_GET_ANSI, color_index, 0, 0, 0, 0, 0, 0));
}

static inline bool SetColorsPaletteColor(unsigned char palette_index, unsigned char color_index,
                                         Color color) {
    return MISL_GUEST_CALL(MISL_PAL_SET, palette_index, color_index, misl_guest_pack_color(color), 0, 0, 0,
                           0) != 0;
}

static inline bool SetColorsDeepPalette(unsigned char palette_index, const Color* palette) {
    return MISL_GUEST_CALL(MISL_PAL_DEEP_SET_ALL, palette_index, (int32_t)(intptr_t)palette, 0, 0, 0, 0, 0) !=
           0;
}

static inline bool SetColorsDeepPaletteColor(unsigned char palette_index, unsigned char color_index,
                                             Color color) {
    return MISL_GUEST_CALL(MISL_PAL_DEEP_SET, palette_index, color_index, misl_guest_pack_color(color), 0, 0,
                           0, 0) != 0;
}

static inline Color GetColorsDeepPaletteColor(unsigned char palette_index, unsigned char color_index) {
    return misl_guest_unpack_color(
        MISL_GUEST_CALL(MISL_PAL_DEEP_GET, palette_index, color_index, 0, 0, 0, 0, 0));
}

static inline uint32_t GetColorsDeepGeneration(void) {
    return (uint32_t)MISL_GUEST_CALL(MISL_PAL_DEEP_GENERATION, 0, 0, 0, 0, 0, 0, 0);
}

static inline Color GetColorsPaletteColor(unsigned char palette_index, unsigned char color_index) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_PAL_GET, palette_index, color_index, 0, 0, 0, 0, 0));
}

static inline uint32_t GetColorsGeneration(void) {
    return (uint32_t)MISL_GUEST_CALL(MISL_PAL_GET_GENERATION, 0, 0, 0, 0, 0, 0, 0);
}
