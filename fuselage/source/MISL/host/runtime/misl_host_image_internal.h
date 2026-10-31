/* Allocation-free load planning. SEE: (MISL_HOST.md, Image preflight) */
#ifndef MISL_HOST_IMAGE_INTERNAL_H
#define MISL_HOST_IMAGE_INTERNAL_H
#include "../misl_host.h"
#include "../../../ROAP/roap_manifest.h"

typedef enum {
    MISL_HOST_IMAGE_OK = 0,
    MISL_HOST_IMAGE_ARGUMENT,
    MISL_HOST_IMAGE_FILE_SIZE,
    MISL_HOST_IMAGE_TRAILER,
    MISL_HOST_IMAGE_MANIFEST,
    MISL_HOST_IMAGE_ENTRY,
    MISL_HOST_IMAGE_ROM_VERSION,
    MISL_HOST_IMAGE_ADDRESS_SPACE,
    MISL_HOST_IMAGE_MEMORY_CAP,
    MISL_HOST_IMAGE_PROGRAM_SIZE
} MislHostImageResult;

typedef struct {
    uint32_t program_image_bytes;
    uint32_t entry_offset;
    uint32_t program_bytes;
    uint32_t allocation_bytes;
    uint32_t pool_count;
    uint32_t pool_base[ROAP_MANIFEST_MAX_POOLS];
    uint32_t pool_bytes[ROAP_MANIFEST_MAX_POOLS];
    const uint8_t* asset_bytes; /* Borrowed from the input; NULL when absent. */
    uint32_t asset_byte_count;
    uint32_t asset_base;
    char save_id[17];
    uint32_t save_version;
    uint32_t save_bytes;
    uint32_t save_base;
} MislHostImagePlan;

/* Zeroes output on refusal; output must not overlap input storage. */
MislHostImageResult fuselage_guest_plan_image(const MislGuestImage* image, uint32_t memory_cap_mib,
                                              MislHostImagePlan* output);
const char* fuselage_guest_get_image_result_message(MislHostImageResult result);
#endif
