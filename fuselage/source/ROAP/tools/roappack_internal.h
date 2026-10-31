/* Private host-tool interface. SEE: (ROAPPACK.md, Source organization) */
#ifndef ROAPPACK_INTERNAL_H
#define ROAPPACK_INTERNAL_H
#include "../roap_asset_rom.h"
#include <stdio.h>

#define ROAPPACK_MAX_SECTIONS 256u
typedef struct {
    uint32_t id;
    const char* path;
} RoapPackSection;

extern uint32_t roappack_max_bytes;
_Noreturn void roappack_die(const char* context, const char* reason);
unsigned char* roappack_read_file(const char* path, uint32_t* size, uint32_t limit);
void roappack_cleanup_output(void);
FILE* roappack_open_output(const char* path);
void roappack_write(FILE* output, const void* bytes, size_t size);
void roappack_commit_output(FILE* output, const char* path);
int roappack_pack(const char* path, const char* image, const RoapPackSection* sections, uint32_t count);
int roappack_list(const char* path);
int roappack_flatten(const char* input_path, const char* output_path);

static inline uint16_t roappack_read16(const unsigned char* bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}
static inline uint32_t roappack_read32(const unsigned char* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}
static inline void roappack_encode32(unsigned char* bytes, uint32_t value) {
    for (unsigned index = 0; index < 4; index++) {
        bytes[index] = (unsigned char)(value >> (index * 8));
    }
}
#endif
