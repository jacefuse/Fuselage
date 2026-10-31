// Tiled mask range decoding. SEE: (GDMF_PIXIES_FORMATS.md, Mask formats)
#include "gdmf_pixies_codec_private.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const unsigned char* position;
    const unsigned char* end;
    uint32_t range, code;
} gdmf_pixies_range_decoder;

static inline unsigned char gdmf_pixies_context_byte(gdmf_pixies_range_decoder* decoder) {
    return (decoder->position < decoder->end) ? *decoder->position++ : 0u;
}
static void gdmf_pixies_context_init(gdmf_pixies_range_decoder* decoder, const unsigned char* at,
                                     const unsigned char* end) {
    decoder->position = at;
    decoder->end = end;
    decoder->range = 0xFFFFFFFFu;
    decoder->code = 0;
    (void)gdmf_pixies_context_byte(decoder);
    for (int index = 0; index < 4; index++) {
        decoder->code = (decoder->code << 8) | gdmf_pixies_context_byte(decoder);
    }
}
static int gdmf_pixies_context_bit(gdmf_pixies_range_decoder* decoder, uint32_t zero_probability) {
    uint32_t bound = (decoder->range >> 12) * zero_probability;
    int bit;
    if (decoder->code < bound) {
        decoder->range = bound;
        bit = 0;
    } else {
        decoder->code -= bound;
        decoder->range -= bound;
        bit = 1;
    }
    while (decoder->range < (1u << 24)) {
        decoder->range <<= 8;
        decoder->code = (decoder->code << 8) | gdmf_pixies_context_byte(decoder);
    }
    return bit;
}

static inline int gdmf_pixies_context_tap(const unsigned char* frame, int image_width, int image_height,
                                          int x, int y) {
    if (x < 0) {
        x = 0;
    }
    if (x >= image_width) {
        x = image_width - 1;
    }
    if (y < 0) {
        y = 0;
    }
    if (y >= image_height) {
        y = image_height - 1;
    }
    return frame[(size_t)y * (size_t)image_width + (size_t)x] & 3;
}

bool gdmf_pixies_unpack_mask_ctxtile(gdmf_pixies_state* state, size_t offset) {
    const int image_width = state->output_width, image_height = state->output_height;
    const size_t pixel_count = (size_t)image_width * (size_t)image_height;

    if (offset > PIXIE_RAM_SIZE || PIXIE_RAM_SIZE - offset < 18u) {
        return false;
    }
    const unsigned char* header = state->ram + offset;
    if (gdmf_pixies_read_u16(header) != image_width || gdmf_pixies_read_u16(header + 2) != image_height) {
        return false;
    }
    if (header[4] != 1u || header[5] != PIXIE_PASS_CTXTILE) {
        return false;
    }

    const unsigned int context_bits = header[6];
    const unsigned int tile_size = header[7];
    const unsigned int tile_count = (unsigned int)gdmf_pixies_read_u16(header + 8);
    uint32_t probability_offset, payload_bytes;
    probability_offset = gdmf_pixies_read_u32(header + 10);
    payload_bytes = gdmf_pixies_read_u32(header + 14);

    if (context_bits == 0 || context_bits > PIXIE_CTXTILE_MAX_CTXBITS) {
        return false;
    }
    if (tile_size == 0 || tile_size > PIXIE_CTXTILE_MAX_TILE) {
        return false;
    }

    const uint32_t context_mask = (context_bits >= 32) ? 0xFFFFFFFFu : ((1u << context_bits) - 1u);
    const size_t probability_bytes = ((size_t)1u << context_bits) * 2u * sizeof(uint32_t);
    if (probability_offset > PIXIE_RAM_SIZE || probability_bytes > PIXIE_RAM_SIZE - probability_offset) {
        return false;
    }
    if ((probability_offset & 3u) != 0u) {
        return false;
    }
    const unsigned char* probability_table = state->ram + probability_offset;

    const size_t table_bytes = (size_t)tile_count * 4u;
    const size_t table_offset = offset + 18u;
    if (table_bytes > PIXIE_RAM_SIZE - table_offset) {
        return false;
    }
    const size_t payload_offset = table_offset + table_bytes;
    if (payload_bytes > PIXIE_RAM_SIZE - payload_offset) {
        return false;
    }
    const unsigned char* tile_entries = state->ram + table_offset;
    const unsigned char* payload = state->ram + payload_offset;
    const unsigned char* payload_end = payload + payload_bytes;

    const int tiles_x = (image_width + (int)tile_size - 1) / (int)tile_size;
    const int tiles_y = (image_height + (int)tile_size - 1) / (int)tile_size;
    if (tile_count > (unsigned int)(tiles_x * tiles_y)) {
        return false;
    }

    if (tile_count == 0) {
        return true;
    }

    // Temporal taps must see the previous frame, regardless of tile order.
    unsigned char* previous_levels = malloc(pixel_count * 2u);
    if (!previous_levels) {
        return false;
    }
    unsigned char* current_levels = previous_levels + pixel_count;
    for (size_t index = 0; index < pixel_count; index++) {
        previous_levels[index] = (unsigned char)(state->mask[index] / 85u);
    }
    memcpy(current_levels, previous_levels, pixel_count);

    for (unsigned int entry = 0, stream_offset = 0; entry < tile_count; entry++) {
        unsigned int tile_index = (unsigned int)gdmf_pixies_read_u16(tile_entries + entry * 4u);
        unsigned int length = (unsigned int)gdmf_pixies_read_u16(tile_entries + entry * 4u + 2u);
        if (tile_index >= (unsigned int)(tiles_x * tiles_y) || stream_offset > payload_bytes) {
            break;
        }

        const int origin_x = (int)(tile_index % (unsigned int)tiles_x) * (int)tile_size;
        const int origin_y = (int)(tile_index / (unsigned int)tiles_x) * (int)tile_size;

        gdmf_pixies_range_decoder decoder;
        gdmf_pixies_context_init(&decoder, payload + stream_offset, payload_end);
        stream_offset += length;

        for (int y = origin_y; y < origin_y + (int)tile_size && y < image_height; y++) {
            for (int x = origin_x; x < origin_x + (int)tile_size && x < image_width; x++) {

                const int has_left = (x > origin_x), has_above = (y > origin_y);
                const int has_above_left = (has_left && has_above),
                          has_above_right = (has_above && x + 1 < origin_x + (int)tile_size);
                uint32_t context =
                    (uint32_t)gdmf_pixies_context_tap(previous_levels, image_width, image_height, x, y) |
                    (uint32_t)gdmf_pixies_context_tap(previous_levels, image_width, image_height, x - 1, y)
                        << 2 |
                    (uint32_t)gdmf_pixies_context_tap(previous_levels, image_width, image_height, x + 1, y)
                        << 4 |
                    (uint32_t)gdmf_pixies_context_tap(previous_levels, image_width, image_height, x, y - 1)
                        << 6 |
                    (uint32_t)gdmf_pixies_context_tap(previous_levels, image_width, image_height, x, y + 1)
                        << 8 |
                    (uint32_t)(has_left ? gdmf_pixies_context_tap(current_levels, image_width, image_height,
                                                                  x - 1, y)
                                        : gdmf_pixies_context_tap(previous_levels, image_width, image_height,
                                                                  x - 1, y))
                        << 10 |
                    (uint32_t)(has_above ? gdmf_pixies_context_tap(current_levels, image_width, image_height,
                                                                   x, y - 1)
                                         : gdmf_pixies_context_tap(previous_levels, image_width, image_height,
                                                                   x, y - 1))
                        << 12 |
                    (uint32_t)(has_above_left ? gdmf_pixies_context_tap(current_levels, image_width,
                                                                        image_height, x - 1, y - 1)
                                              : gdmf_pixies_context_tap(previous_levels, image_width,
                                                                        image_height, x - 1, y - 1))
                        << 14 |
                    (uint32_t)(has_above_right ? gdmf_pixies_context_tap(current_levels, image_width,
                                                                         image_height, x + 1, y - 1)
                                               : gdmf_pixies_context_tap(previous_levels, image_width,
                                                                         image_height, x + 1, y - 1))
                        << 16;
                context &= context_mask;

                const uint32_t first_probability_word =
                                   gdmf_pixies_read_u32(probability_table + context * 8u),
                               second_probability_word =
                                   gdmf_pixies_read_u32(probability_table + context * 8u + 4u);
                const uint32_t upper_probability = first_probability_word & 0xFFFFu,
                               lower_zero_probability = (first_probability_word >> 16) & 0xFFFFu,
                               lower_one_probability = second_probability_word & 0xFFFFu;

                // Out-of-range probabilities can reduce the range to zero and stall.
                if (upper_probability >= 4096u || lower_zero_probability >= 4096u ||
                    lower_one_probability >= 4096u) {
                    free(previous_levels);
                    return false;
                }
                const int upper_bit =
                    gdmf_pixies_context_bit(&decoder, upper_probability ? upper_probability : 2048u);
                const int lower_bit = gdmf_pixies_context_bit(
                    &decoder, upper_bit ? (lower_one_probability ? lower_one_probability : 2048u)
                                        : (lower_zero_probability ? lower_zero_probability : 2048u));
                current_levels[(size_t)y * (size_t)image_width + (size_t)x] =
                    (unsigned char)((upper_bit << 1) | lower_bit);
            }
        }
    }

    for (size_t index = 0; index < pixel_count; index++) {
        state->mask[index] = (unsigned char)((current_levels[index] & 3u) * 85u);
    }
    free(previous_levels);
    state->dirty = true;
    return true;
}
