#ifndef SRAM_INTERNAL_H
#define SRAM_INTERNAL_H
#include "../sram_host.h"
#include <stdio.h>
#define SRAM_PATH_CAPACITY 1024u
#define SRAM_FILE_PATH_CAPACITY (SRAM_PATH_CAPACITY + 64u)
#define SRAM_HEADER_BYTES 64u
#define SRAM_FILE_FORMAT 1u

typedef struct {
    char id[SRAM_ID_BYTES];
    uint32_t version;
    uint32_t byte_count;
    char directory[SRAM_PATH_CAPACITY];
    char path[SRAM_FILE_PATH_CAPACITY];
    char previous_path[SRAM_FILE_PATH_CAPACITY];
    uint8_t* data;
    bool configured;
    bool open;
    bool abandoned;
    bool dirty;
    uint64_t created;
    char reason[160];
} SramState;
extern SramState sram_state;
void sram_set_failure(const char* reason);
bool sram_build_path(char* output, size_t capacity, const char* program_path, const char* extension);
uint32_t sram_read_word(const uint8_t* bytes);
uint64_t sram_read_wide_word(const uint8_t* bytes);
uint32_t sram_checksum(const uint8_t* bytes, uint32_t byte_count);
void sram_encode_header(uint8_t header[SRAM_HEADER_BYTES]);
uint64_t sram_get_utc_seconds(void);
bool sram_write_file(bool first_run);
bool sram_publish_new(const char* staging_path, const char* destination_path);
#endif
