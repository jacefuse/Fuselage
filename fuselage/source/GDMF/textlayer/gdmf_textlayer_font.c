// CPU atlas conversion, with ENTERON's original shade mapping and orientation.
#include "gdmf_textlayer_font.h"

_Static_assert(CHARACTER_WIDTH == 16 && CHARACTER_HEIGHT == 16, "Packed font has 16x16 glyphs");
_Static_assert(GDMF_TEXT_GLYPH_BYTES * 4 == CHARACTER_WIDTH * CHARACTER_HEIGHT, "Four pixels per byte");
bool gdmf_textlayer_font_decode(unsigned char* pixels, size_t capacity) {
    if (!pixels || capacity < GDMF_TEXT_ATLAS_BYTES) {
        return false;
    }
    const unsigned char shades[4] = {0, 255, 85, 170};
    for (unsigned glyph = 0; glyph < GDMF_TEXT_GLYPH_COUNT; ++glyph) {
        unsigned origin_x = (glyph % 16) * CHARACTER_WIDTH;
        unsigned origin_y = (glyph / 16) * CHARACTER_HEIGHT;
        for (unsigned byte_index = 0; byte_index < GDMF_TEXT_GLYPH_BYTES; ++byte_index) {
            unsigned packed = gdmf_textlayer_font_bitmap[glyph][byte_index];
            for (unsigned pixel = 0; pixel < 4; ++pixel) {
                unsigned shade = (packed >> (pixel * 2)) & 3;
                unsigned local_index = byte_index * 4 + pixel;
                unsigned x = origin_x + local_index % CHARACTER_WIDTH;
                unsigned y = origin_y + local_index / CHARACTER_WIDTH;
                size_t output_index = ((size_t)y * GDMF_TEXT_ATLAS_WIDTH + x) * 4;
                pixels[output_index] = pixels[output_index + 1] = pixels[output_index + 2] = shades[shade];
                pixels[output_index + 3] = shade ? 255 : 0;
            }
        }
    }
    return true;
}
