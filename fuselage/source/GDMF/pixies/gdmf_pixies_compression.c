// Byte compression passes. SEE: (GDMF_PIXIES_FORMATS.md, Compression passes)
#include "gdmf_pixies_codec_private.h"
#include <stdlib.h>
#include <string.h>

#define GDMF_PIXIES_LZ_MIN_MATCH 3
static size_t gdmf_pixies_rle_decode(const unsigned char* data, uint32_t length, unsigned char* output,
                                     size_t output_capacity) {
    size_t index = 0;
    size_t output_position = 0;

    while (length - index >= 2 && output_position < output_capacity) {
        unsigned char run = data[index++];
        unsigned char value = data[index++];
        size_t run_length = run;

        if (run_length > output_capacity - output_position) {
            run_length = output_capacity - output_position;
        }
        memset(output + output_position, value, run_length);
        output_position += run_length;
    }

    return output_position;
}

static size_t gdmf_pixies_lz_decode(const unsigned char* data, uint32_t length, unsigned char* output,
                                    size_t output_capacity) {
    size_t index = 0;
    size_t output_position = 0;

    while (index < length && output_position < output_capacity) {
        uint8_t tag = data[index++];

        if (tag & 0x80u) {
            if (length - index < 4) {
                break;
            }
            uint32_t match_distance = gdmf_pixies_read_u32(data + index);
            index += 4;
            if (match_distance == 0 || match_distance > output_position) {
                break;
            }
            size_t match_length = (size_t)(tag & 0x7Fu) + GDMF_PIXIES_LZ_MIN_MATCH;
            if (match_length > output_capacity - output_position) {
                match_length = output_capacity - output_position;
            }
            // Overlapping matches consume bytes produced earlier in this match.
            for (size_t copied = 0; copied < match_length; copied++) {
                output[output_position] = output[output_position - match_distance];
                output_position++;
            }
        } else {
            size_t literal_length = tag;

            if (literal_length > length - index) {
                literal_length = length - index;
            }
            if (literal_length > output_capacity - output_position) {
                literal_length = output_capacity - output_position;
            }
            memcpy(output + output_position, data + index, literal_length);
            index += (uint32_t)literal_length;
            output_position += literal_length;
        }
    }

    return output_position;
}
void gdmf_pixies_delta_decode(const unsigned char* data, size_t count, unsigned char* output) {
    unsigned char previous = 0;

    for (size_t index = 0; index < count; index++) {
        previous = (unsigned char)(previous + data[index]);
        output[index] = previous;
    }

    return;
}

size_t gdmf_pixies_decode_primary(bool use_lz, const unsigned char* data, uint32_t length,
                                  unsigned char* output, size_t output_capacity) {
    return use_lz ? gdmf_pixies_lz_decode(data, length, output, output_capacity)
                  : gdmf_pixies_rle_decode(data, length, output, output_capacity);
}
