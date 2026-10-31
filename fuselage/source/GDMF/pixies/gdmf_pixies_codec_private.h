#ifndef GDMF_PIXIES_CODEC_PRIVATE_H
#define GDMF_PIXIES_CODEC_PRIVATE_H

#include "gdmf_pixies_decode_private.h"

#define GDMF_PIXIES_MAX_UNPACK_PIXELS 16777216u
#define GDMF_PIXIES_MAX_UNPACK_DIM 4096

// Packed integers are little-endian and may be unaligned.
static inline uint16_t gdmf_pixies_read_u16(const unsigned char* bytes) {
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8));
}
static inline uint32_t gdmf_pixies_read_u32(const unsigned char* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}
static inline bool gdmf_pixies_ram_range(size_t offset, size_t length) {
    return offset <= PIXIE_RAM_SIZE && length <= PIXIE_RAM_SIZE - offset;
}

size_t gdmf_pixies_decode_primary(bool use_lz, const unsigned char* data, uint32_t length,
                                  unsigned char* output, size_t output_capacity);
void gdmf_pixies_delta_decode(const unsigned char* data, size_t count, unsigned char* output);
// On success, output_owned selects heap ownership versus a borrowed RAM view.
// The caller frees owned output after use. Failure may advance position.
bool gdmf_pixies_undo_huffman_if_present(const gdmf_pixies_state* state, size_t* position, bool use_huffman,
                                         const unsigned char** output_data, size_t* output_length,
                                         bool* output_owned);

#endif
