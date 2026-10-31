/* Native pointer/string admission. SEE: (MISL_HOST.md, Guest memory access) */
#ifndef MISL_HOST_POINTERS_INTERNAL_H
#define MISL_HOST_POINTERS_INTERNAL_H
#include <stdint.h>

#define MISL_HOST_BATCH_MAX_ITEMS (1u << 20)
typedef enum {
    MISL_HOST_STRING_COMPLETE,
    MISL_HOST_STRING_TRUNCATED,
    MISL_HOST_STRING_UNMAPPED,
    MISL_HOST_STRING_ARGUMENT
} MislHostStringStatus;
typedef struct {
    uint32_t byte_count;
    MislHostStringStatus status;
} MislHostStringRead;

const void* fuselage_guest_resolve_input(uint32_t address, uint32_t byte_count);
void* fuselage_guest_resolve_output(uint32_t address, uint32_t capacity);
unsigned char* fuselage_guest_resolve_batch_output(uint32_t address, uint32_t byte_count);
int fuselage_guest_batch_ranges_valid(uint32_t input_address, uint32_t input_bytes, uint32_t output_address,
                                      uint32_t output_bytes);
MislHostStringRead fuselage_guest_copy_string(uint32_t address, char* output, uint32_t capacity);
#endif
