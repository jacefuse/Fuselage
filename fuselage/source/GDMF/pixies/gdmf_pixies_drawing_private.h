#ifndef GDMF_PIXIES_DRAWING_PRIVATE_H
#define GDMF_PIXIES_DRAWING_PRIVATE_H

#include "gdmf_pixies_state_private.h"

// Coordinates are already transformed. Callers serialize state access.
void gdmf_pixies_emit_line(gdmf_pixies_state* state, float start_x, float start_y, float end_x, float end_y,
                           uint32_t color_word, int width);
void gdmf_pixies_fill_polygon(gdmf_pixies_state* state, const float* point_x, const float* point_y, int count,
                              Color color, bool pattern);
void gdmf_pixies_fill_mask_polygon(gdmf_pixies_state* state, const float* point_x, const float* point_y,
                                   int count, uint8_t value, bool pattern);
void gdmf_pixies_live_push_quad(gdmf_pixies_state* state, float center_x, float center_y, float half_width,
                                float half_height, Color color);
void gdmf_pixies_live_push_line(gdmf_pixies_state* state, float start_x, float start_y, float end_x,
                                float end_y, float width, Color color);
bool gdmf_pixies_unpack_command(int id, gdmf_pixies_state* state, uint16_t flags,
                                const uint32_t arguments[4]);

static inline Color gdmf_pixies_unpack_color(uint32_t packed) {
    return (Color){(unsigned char)packed, (unsigned char)(packed >> 8), (unsigned char)(packed >> 16),
                   (unsigned char)(packed >> 24)};
}

static inline void gdmf_pixies_unpack_xy(uint32_t packed, int* coordinate_x, int* coordinate_y) {
    *coordinate_x = (int16_t)(packed >> 16);
    *coordinate_y = (int16_t)packed;
}

#endif
