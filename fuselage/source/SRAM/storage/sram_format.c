/* Byte-wise FSAV format 1. SEE: (SRAM.md, File format) */
#include "../core/sram_internal.h"
#include <string.h>
#include <time.h>

uint32_t sram_read_word(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}
uint64_t sram_read_wide_word(const uint8_t* bytes) {
    return sram_read_word(bytes) | (uint64_t)sram_read_word(bytes + 4) << 32;
}
static void sram_write_word(uint8_t* output, uint32_t value) {
    for (unsigned index = 0; index < 4; index++) {
        output[index] = (uint8_t)(value >> (index * 8));
    }
}
static void sram_write_wide_word(uint8_t* output, uint64_t value) {
    sram_write_word(output, (uint32_t)value);
    sram_write_word(output + 4, (uint32_t)(value >> 32));
}
uint32_t sram_checksum(const uint8_t* bytes, uint32_t byte_count) {
    static uint32_t table[256];
    static bool ready;
    if (!ready) {
        for (uint32_t index = 0; index < 256; index++) {
            uint32_t remainder = index;
            for (unsigned bit = 0; bit < 8; bit++) {
                remainder = (remainder & 1u) ? 0xEDB88320u ^ (remainder >> 1) : remainder >> 1;
            }
            table[index] = remainder;
        }
        ready = true;
    }
    uint32_t checksum = UINT32_MAX;
    for (uint32_t index = 0; index < byte_count; index++) {
        checksum = table[(checksum ^ bytes[index]) & 255u] ^ (checksum >> 8);
    }
    return checksum ^ UINT32_MAX;
}
uint64_t sram_get_utc_seconds(void) {
    time_t now = time(NULL);
    return now == (time_t)-1 || now < 0 ? 0 : (uint64_t)now;
}
void sram_encode_header(uint8_t header[SRAM_HEADER_BYTES]) {
    memset(header, 0, SRAM_HEADER_BYTES);
    memcpy(header, "FSAV", 4);
    sram_write_word(header + 4, SRAM_FILE_FORMAT);
    memcpy(header + 8, sram_state.id, SRAM_ID_BYTES);
    sram_write_word(header + 24, sram_state.version);
    sram_write_word(header + 28, sram_state.byte_count);
    sram_write_word(header + 32, sram_checksum(sram_state.data, sram_state.byte_count));
    sram_write_wide_word(header + 40, sram_state.created);
    sram_write_wide_word(header + 48, sram_get_utc_seconds());
}
