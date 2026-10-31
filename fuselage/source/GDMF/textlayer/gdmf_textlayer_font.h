#ifndef GDMF_TEXTLAYER_FONT_H
#define GDMF_TEXTLAYER_FONT_H
#include "gdmf_textlayer.h"
#include <stddef.h>

#define GDMF_TEXT_GLYPH_COUNT 256
#define GDMF_TEXT_GLYPH_BYTES 64
#define GDMF_TEXT_ATLAS_WIDTH 256
#define GDMF_TEXT_ATLAS_HEIGHT 256
#define GDMF_TEXT_ATLAS_BYTES (GDMF_TEXT_ATLAS_WIDTH * GDMF_TEXT_ATLAS_HEIGHT * 4u)
extern const unsigned char gdmf_textlayer_font_bitmap[GDMF_TEXT_GLYPH_COUNT][GDMF_TEXT_GLYPH_BYTES];
// Caller-owned RGBA8 storage; false leaves it untouched for NULL/short buffers.
bool gdmf_textlayer_font_decode(unsigned char* pixels, size_t capacity);
#endif
