#ifndef GDMF_PIXIES_DECODE_PRIVATE_H
#define GDMF_PIXIES_DECODE_PRIVATE_H

#include "gdmf_pixies_state_private.h"

// Requires initialized matching state, serialized access and a whole-pixel
// destination decoded from the command's signed 16-bit pair.
bool gdmf_pixies_unpack_mask_ctxtile(gdmf_pixies_state* state, size_t offset);
bool gdmf_pixies_image_has_row_table(const gdmf_pixies_state* state, size_t offset);
bool gdmf_pixies_unpack_packed(gdmf_pixies_state* state, size_t offset, PixieUnpackFormat format,
                               int destination_x, int destination_y, unsigned char palette_index);

#endif
