#ifndef GDMF_PIXIES_DECODE_REQUESTS_PRIVATE_H
#define GDMF_PIXIES_DECODE_REQUESTS_PRIVATE_H

#include "gdmf_pixies_state_private.h"

// Inputs are borrowed for the call only; queue ownership stays private.
// True means a request copied its inputs and made the pixie GPU-resident.
// False must preserve host fallback eligibility unless already GPU-resident.
bool gdmf_pixies_decode_on_gpu(gdmf_pixies_state* state, uint32_t format, const unsigned char* data,
                               size_t length, int width, int height, int destination_x, int destination_y,
                               const Color* palette, uint32_t palette_count);
bool gdmf_pixies_decode_rle_on_gpu(gdmf_pixies_state* state, const unsigned char* row_table,
                                   const unsigned char* data, size_t length, int width, int height,
                                   int destination_x, int destination_y, const Color* palette,
                                   uint32_t palette_count);

#endif
