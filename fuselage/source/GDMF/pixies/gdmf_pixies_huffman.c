// Canonical Huffman wrapper. SEE: (GDMF_PIXIES_FORMATS.md, Compression passes)
#include "gdmf_pixies_codec_private.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const unsigned char* data;
    size_t length;
    size_t byte_position;
    int bit_position;
} gdmf_pixies_bit_reader;

static int gdmf_pixies_bitreader_read_bit(gdmf_pixies_bit_reader* reader) {
    // Preserve the existing zero-padding behavior after encoded input ends.
    // SEE: (GDMF_PIXIES_FORMATS.md, Decoder behavior and limits)
    if (reader->byte_position >= reader->length) {
        return 0;
    }
    int bit = (reader->data[reader->byte_position] >> (7 - reader->bit_position)) & 1;
    reader->bit_position++;
    if (reader->bit_position == 8) {
        reader->bit_position = 0;
        reader->byte_position++;
    }

    return bit;
}

typedef struct {
    uint32_t first_code[256];
    int count[256];
    int offset[256];
    unsigned char symbols[256];
} gdmf_pixies_huffman_table;

static void gdmf_pixies_huffman_build_decode_table(const uint8_t code_lengths[256],
                                                   gdmf_pixies_huffman_table* table) {
    int length_counts[256] = {0};

    for (int symbol = 0; symbol < 256; symbol++) {
        if (code_lengths[symbol] > 0) {
            length_counts[code_lengths[symbol]]++;
        }
    }
    memcpy(table->count, length_counts, sizeof(length_counts));

    uint32_t code = 0;
    for (int length = 1; length < 256; length++) {
        code = (code + (uint32_t)length_counts[length - 1]) << 1;
        table->first_code[length] = code;
    }

    int offset = 0;
    for (int length = 1; length < 256; length++) {
        table->offset[length] = offset;
        offset += length_counts[length];
    }

    int cursor[256];
    memcpy(cursor, table->offset, sizeof(cursor));
    for (int symbol = 0; symbol < 256; symbol++) {
        if (code_lengths[symbol] > 0) {
            table->symbols[cursor[code_lengths[symbol]]++] = (unsigned char)symbol;
        }
    }

    return;
}

static bool gdmf_pixies_huffman_decode(gdmf_pixies_bit_reader* reader, const gdmf_pixies_huffman_table* table,
                                       size_t decoded_length, unsigned char* output) {
    for (size_t index = 0; index < decoded_length; index++) {
        uint32_t code = 0;
        int length = 0;
        bool found = false;

        while (length < 255) {
            code = (code << 1) | (uint32_t)gdmf_pixies_bitreader_read_bit(reader);
            length++;
            if (table->count[length] > 0) {
                uint32_t symbol_index = code - table->first_code[length];

                if (symbol_index < (uint32_t)table->count[length]) {
                    output[index] = table->symbols[table->offset[length] + symbol_index];
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

bool gdmf_pixies_undo_huffman_if_present(const gdmf_pixies_state* state, size_t* position, bool use_huffman,
                                         const unsigned char** output_data, size_t* output_length,
                                         bool* output_owned) {
    if (!use_huffman) {
        if (!gdmf_pixies_ram_range(*position, 4)) {
            return false;
        }
        uint32_t length = gdmf_pixies_read_u32(state->ram + *position);
        *position += 4;
        if (!gdmf_pixies_ram_range(*position, length)) {
            return false;
        }
        *output_data = state->ram + *position;
        *output_length = length;
        *output_owned = false;
        *position += length;
        return true;
    }

    if (!gdmf_pixies_ram_range(*position, 264)) {
        return false;
    }
    uint8_t code_lengths[256];
    memcpy(code_lengths, state->ram + *position, 256);
    *position += 256;
    uint32_t decoded_length = gdmf_pixies_read_u32(state->ram + *position);
    *position += 4;
    uint32_t encoded_length = gdmf_pixies_read_u32(state->ram + *position);
    *position += 4;
    if (!gdmf_pixies_ram_range(*position, encoded_length)) {
        return false;
    }

    if (decoded_length == 0 || decoded_length > GDMF_PIXIES_MAX_UNPACK_PIXELS * 4u) {
        return false;
    }

    gdmf_pixies_huffman_table table;
    gdmf_pixies_huffman_build_decode_table(code_lengths, &table);

    unsigned char* decoded = malloc(decoded_length);
    if (!decoded) {
        return false;
    }
    gdmf_pixies_bit_reader reader = {state->ram + *position, encoded_length, 0, 0};
    bool ok = gdmf_pixies_huffman_decode(&reader, &table, decoded_length, decoded);
    *position += encoded_length;
    if (!ok) {
        free(decoded);
        return false;
    }

    *output_data = decoded;
    *output_length = decoded_length;
    *output_owned = true;

    return true;
}
