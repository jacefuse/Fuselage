// Color headers and payloads. SEE: (GDMF_PIXIES_FORMATS.md, Color headers and payloads)
#include "gdmf_pixies_codec_private.h"
#include <stdlib.h>
#include <string.h>

#include "gdmf_pixies_decode_requests_private.h"

bool gdmf_pixies_image_has_row_table(const gdmf_pixies_state* state, size_t offset) {
    if (!gdmf_pixies_ram_range(offset, 7)) {
        return false;
    }
    const unsigned char* base = state->ram + offset;
    return base[4] == 2 && base[5] == PIXIE_PASS_ROWTABLE && base[6] == PIXIE_PASS_RLE;
}

static bool gdmf_pixies_take_row_table(const gdmf_pixies_state* state, size_t* position, bool use_row_table,
                                       int height, const unsigned char** output_table) {
    *output_table = NULL;
    if (!use_row_table) {
        return true;
    }
    size_t bytes = (size_t)height * 4u;
    if (*position > PIXIE_RAM_SIZE || bytes > PIXIE_RAM_SIZE - *position) {
        return false;
    }
    *output_table = state->ram + *position;
    *position += bytes;
    return true;
}

static void gdmf_pixies_blit_pixel(gdmf_pixies_state* state, int x, int y, Color color) {
    if (x < 0 || x >= state->output_width || y < 0 || y >= state->output_height) {
        return;
    }
    state->output[y * state->output_width + x] = color;

    return;
}

bool gdmf_pixies_unpack_packed(gdmf_pixies_state* state, size_t offset, PixieUnpackFormat format,
                               int destination_x, int destination_y, unsigned char palette_index) {
    if (!gdmf_pixies_ram_range(offset, 5)) {
        return false;
    }
    const unsigned char* base = state->ram + offset;
    int width = gdmf_pixies_read_u16(base);
    int height = gdmf_pixies_read_u16(base + 2);
    uint8_t pass_count = base[4];
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (width > GDMF_PIXIES_MAX_UNPACK_DIM || height > GDMF_PIXIES_MAX_UNPACK_DIM ||
        (uint64_t)width * (uint64_t)height > GDMF_PIXIES_MAX_UNPACK_PIXELS) {
        return false;
    }
    if (!gdmf_pixies_ram_range(offset, 5u + pass_count)) {
        return false;
    }

    bool use_huffman = false;
    bool use_delta = false;
    bool use_lz = false;
    bool use_row_table = false;
    if (pass_count == 2 && base[5] == PIXIE_PASS_ROWTABLE) {

        if (base[6] != PIXIE_PASS_RLE) {
            return false;
        }
        if (format != PIXIE_FORMAT_RLE_PALETTE_SHARED && format != PIXIE_FORMAT_RLE_PALETTE_OWN) {
            return false;
        }
        use_row_table = true;
    } else if (pass_count == 0) {

    } else if (pass_count == 1) {
        if (base[5] == PIXIE_PASS_LZ) {
            use_lz = true;
        } else if (base[5] != PIXIE_PASS_RLE) {
            return false;
        }
    } else if (pass_count == 2) {
        if (base[5] == PIXIE_PASS_RLE && base[6] == PIXIE_PASS_HUFFMAN) {
            use_huffman = true;
        } else if (base[5] == PIXIE_PASS_LZ && base[6] == PIXIE_PASS_HUFFMAN) {
            use_lz = true;
            use_huffman = true;
        } else if (base[5] == PIXIE_PASS_DELTA && base[6] == PIXIE_PASS_RLE) {
            if (format != PIXIE_FORMAT_RLE_RGBA8) {
                return false;
            }
            use_delta = true;
        } else if (base[5] == PIXIE_PASS_DELTA && base[6] == PIXIE_PASS_LZ) {
            if (format != PIXIE_FORMAT_RLE_RGBA8) {
                return false;
            }
            use_delta = true;
            use_lz = true;
        } else {
            return false;
        }
    } else if (pass_count == 3) {
        if (base[5] != PIXIE_PASS_DELTA || base[7] != PIXIE_PASS_HUFFMAN) {
            return false;
        }
        if (base[6] == PIXIE_PASS_LZ) {
            use_lz = true;
        } else if (base[6] != PIXIE_PASS_RLE) {
            return false;
        }
        if (format != PIXIE_FORMAT_RLE_RGBA8) {
            return false;
        }
        use_delta = true;
        use_huffman = true;
    } else {
        return false;
    }

    size_t pixel_count = (size_t)width * (size_t)height;
    size_t position = offset + 5 + pass_count;

    switch (format) {
    case PIXIE_FORMAT_RLE_PALETTE_OWN: {
        if (!gdmf_pixies_ram_range(position, 256 * sizeof(Color))) {
            return false;
        }
        const Color* palette = (const Color*)(state->ram + position);
        position += 256 * sizeof(Color);

        const unsigned char* row_table;
        if (!gdmf_pixies_take_row_table(state, &position, use_row_table, height, &row_table)) {
            return false;
        }
        const unsigned char* compressed_data;
        size_t compressed_length;
        bool owns_compressed_data;
        if (!gdmf_pixies_undo_huffman_if_present(state, &position, use_huffman, &compressed_data,
                                                 &compressed_length, &owns_compressed_data)) {
            return false;
        }
        if (row_table &&
            gdmf_pixies_decode_rle_on_gpu(state, row_table, compressed_data, compressed_length, width, height,
                                          destination_x, destination_y, palette, 256)) {
            return true;
        }
        if (state->gpu_resident) {
            if (owns_compressed_data) {
                free((void*)compressed_data);
            }
            return false;
        }
        unsigned char* indices = malloc(pixel_count);
        if (!indices) {
            if (owns_compressed_data) {
                free((void*)compressed_data);
            }
            return false;
        }
        size_t decoded_bytes = gdmf_pixies_decode_primary(use_lz, compressed_data,
                                                          (uint32_t)compressed_length, indices, pixel_count);
        if (owns_compressed_data) {
            free((void*)compressed_data);
        }

        if (decoded_bytes != pixel_count) {
            free(indices);
            return false;
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                gdmf_pixies_blit_pixel(state, destination_x + x, destination_y + y,
                                       palette[indices[(size_t)y * width + x]]);
            }
        }
        free(indices);
        break;
    }

    case PIXIE_FORMAT_RLE_PALETTE_SHARED: {
        const unsigned char* row_table;
        if (!gdmf_pixies_take_row_table(state, &position, use_row_table, height, &row_table)) {
            return false;
        }
        const unsigned char* compressed_data;
        size_t compressed_length;
        bool owns_compressed_data;
        if (!gdmf_pixies_undo_huffman_if_present(state, &position, use_huffman, &compressed_data,
                                                 &compressed_length, &owns_compressed_data)) {
            return false;
        }
        if (row_table) {
            Color resolved_palette[16];
            for (int index = 0; index < 16; index++) {
                resolved_palette[index] = GetColorsPaletteColor(palette_index, (unsigned char)index);
            }
            if (gdmf_pixies_decode_rle_on_gpu(state, row_table, compressed_data, compressed_length, width,
                                              height, destination_x, destination_y, resolved_palette, 16)) {
                return true;
            }
            if (state->gpu_resident) {
                return false;
            }
        }
        if (state->gpu_resident) {
            if (owns_compressed_data) {
                free((void*)compressed_data);
            }
            return false;
        }
        unsigned char* indices = malloc(pixel_count);
        if (!indices) {
            if (owns_compressed_data) {
                free((void*)compressed_data);
            }
            return false;
        }
        size_t decoded_bytes = gdmf_pixies_decode_primary(use_lz, compressed_data,
                                                          (uint32_t)compressed_length, indices, pixel_count);
        if (owns_compressed_data) {
            free((void*)compressed_data);
        }

        if (decoded_bytes != pixel_count) {
            free(indices);
            return false;
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Color color = GetColorsPaletteColor(palette_index, indices[(size_t)y * width + x]);

                gdmf_pixies_blit_pixel(state, destination_x + x, destination_y + y, color);
            }
        }
        free(indices);
        break;
    }

    case PIXIE_FORMAT_PALETTE4BPP: {

        if (pass_count != 0) {
            return false;
        }
        size_t packed_length = (pixel_count + 1) / 2;
        if (!gdmf_pixies_ram_range(position, 4u + packed_length)) {
            return false;
        }
        position += 4;
        const unsigned char* packed = state->ram + position;
        {

            Color resolved_palette[16];
            for (int index = 0; index < 16; index++) {
                resolved_palette[index] = GetColorsPaletteColor(palette_index, (unsigned char)index);
            }
            if (gdmf_pixies_decode_on_gpu(state, PIXIE_FORMAT_PALETTE4BPP, packed, packed_length, width,
                                          height, destination_x, destination_y, resolved_palette, 16)) {
                return true;
            }
            if (state->gpu_resident) {
                return false;
            }
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                size_t index = (size_t)y * width + x;
                unsigned char byte = packed[index / 2];
                unsigned char palette_entry =
                    (index % 2 == 0) ? (unsigned char)(byte >> 4) : (unsigned char)(byte & 0x0F);

                gdmf_pixies_blit_pixel(state, destination_x + x, destination_y + y,
                                       GetColorsPaletteColor(palette_index, palette_entry));
            }
        }
        break;
    }

    case PIXIE_FORMAT_PALETTE8BPP: {

        if (pass_count != 0) {
            return false;
        }

        if (palette_index >= FUSELAGE_DEEP_PALETTE_COUNT) {
            return false;
        }
        if (!gdmf_pixies_ram_range(position, 4u + pixel_count)) {
            return false;
        }
        position += 4;
        const unsigned char* indices = state->ram + position;
        const Color* deep_palette = DeepColors[palette_index];
        if (gdmf_pixies_decode_on_gpu(state, PIXIE_FORMAT_PALETTE8BPP, indices, pixel_count, width, height,
                                      destination_x, destination_y, deep_palette,
                                      FUSELAGE_DEEP_PALETTE_SIZE)) {
            return true;
        }
        if (state->gpu_resident) {
            return false;
        }
        for (int y = 0; y < height; y++) {
            const unsigned char* row = indices + (size_t)y * width;
            for (int x = 0; x < width; x++) {
                gdmf_pixies_blit_pixel(state, destination_x + x, destination_y + y, deep_palette[row[x]]);
            }
        }
        break;
    }

    case PIXIE_FORMAT_RLE_RGBA8: {

        const unsigned char* compressed_data;
        size_t compressed_length;
        bool owns_compressed_data;
        if (!gdmf_pixies_undo_huffman_if_present(state, &position, use_huffman, &compressed_data,
                                                 &compressed_length, &owns_compressed_data)) {
            return false;
        }
        if (compressed_length < 16) {
            if (owns_compressed_data) {
                free((void*)compressed_data);
            }
            return false;
        }
        uint32_t plane_lengths[4];
        for (int channel = 0; channel < 4; channel++) {
            plane_lengths[channel] = gdmf_pixies_read_u32(compressed_data + (size_t)channel * 4);
        }
        size_t remaining = compressed_length - 16;
        bool lengths_valid = true;
        for (int channel = 0; channel < 4; channel++) {
            if (plane_lengths[channel] > remaining) {
                lengths_valid = false;
                break;
            }
            remaining -= plane_lengths[channel];
        }
        if (!lengths_valid) {
            if (owns_compressed_data) {
                free((void*)compressed_data);
            }
            return false;
        }

        unsigned char* planes[4] = {NULL, NULL, NULL, NULL};
        bool plane_failed = false;
        size_t plane_offset = 16;
        for (int channel = 0; channel < 4 && !plane_failed; channel++) {
            planes[channel] = malloc(pixel_count);
            if (!planes[channel]) {
                plane_failed = true;
                break;
            }
            size_t decoded_bytes = gdmf_pixies_decode_primary(
                use_lz, compressed_data + plane_offset, plane_lengths[channel], planes[channel], pixel_count);

            if (decoded_bytes != pixel_count) {
                plane_failed = true;
                break;
            }
            if (use_delta) {

                unsigned char* absolute_values = malloc(pixel_count);

                if (!absolute_values) {
                    plane_failed = true;
                    break;
                }
                gdmf_pixies_delta_decode(planes[channel], pixel_count, absolute_values);
                free(planes[channel]);
                planes[channel] = absolute_values;
            }
            plane_offset += plane_lengths[channel];
        }
        if (owns_compressed_data) {
            free((void*)compressed_data);
        }
        if (plane_failed) {
            for (int channel = 0; channel < 4; channel++) {
                free(planes[channel]);
            }
            return false;
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                size_t index = (size_t)y * width + x;
                Color color = {planes[0][index], planes[1][index], planes[2][index], planes[3][index]};

                gdmf_pixies_blit_pixel(state, destination_x + x, destination_y + y, color);
            }
        }
        for (int channel = 0; channel < 4; channel++) {
            free(planes[channel]);
        }
        break;
    }

    case PIXIE_FORMAT_RGBA8: {

        if (pass_count != 0) {
            return false;
        }
        size_t raw_length = pixel_count * 4;
        if (!gdmf_pixies_ram_range(position, 4u + raw_length)) {
            return false;
        }
        position += 4;
        const unsigned char* raw = state->ram + position;
        if (gdmf_pixies_decode_on_gpu(state, PIXIE_FORMAT_RGBA8, raw, raw_length, width, height,
                                      destination_x, destination_y, NULL, 0)) {
            return true;
        }
        if (state->gpu_resident) {
            return false;
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                size_t index = (size_t)y * width + x;
                Color color = {raw[index * 4 + 0], raw[index * 4 + 1], raw[index * 4 + 2],
                               raw[index * 4 + 3]};

                gdmf_pixies_blit_pixel(state, destination_x + x, destination_y + y, color);
            }
        }
        break;
    }

    default:
        return false;
    }

    state->dirty = true;

    return true;
}
