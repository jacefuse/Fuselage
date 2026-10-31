/* Split the file and place declared memory without allocating it. */
#include "misl_host_image_internal.h"
#include "../../../ROAP/roap_asset_rom.h"
#include "../../bootspur/misl_rom.h"
#include <string.h>

static uint32_t fuselage_guest_read_u32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static int fuselage_guest_split_image(const MislGuestImage* image, MislHostImagePlan* plan) {
    plan->program_image_bytes = image->byte_count;
    if (image->byte_count < ROAP_ASSET_TRAILER_BYTES) {
        return 1;
    }
    const uint8_t* trailer = image->bytes + image->byte_count - ROAP_ASSET_TRAILER_BYTES;
    if (fuselage_guest_read_u32(trailer) != ROAP_ASSET_TRAILER_MAGIC) {
        return 1;
    }
    uint32_t payload_bytes = fuselage_guest_read_u32(trailer + 12);
    if (fuselage_guest_read_u32(trailer + 4) != ROAP_ASSET_DIRECTORY_VERSION ||
        payload_bytes < ROAP_ASSET_TRAILER_BYTES + sizeof(RoapAssetDirectoryHeader) ||
        payload_bytes > image->byte_count) {
        return 0;
    }
    uint32_t payload_limit = payload_bytes - ROAP_ASSET_TRAILER_BYTES;
    uint32_t directory_offset = fuselage_guest_read_u32(trailer + 8);
    if (directory_offset > payload_limit - sizeof(RoapAssetDirectoryHeader)) {
        return 0;
    }
    plan->program_image_bytes = image->byte_count - payload_bytes;
    plan->asset_bytes = image->bytes + plan->program_image_bytes;
    plan->asset_byte_count = payload_bytes;
    const uint8_t* directory = plan->asset_bytes + directory_offset;
    if (fuselage_guest_read_u32(directory) != ROAP_ASSET_DIRECTORY_MAGIC ||
        fuselage_guest_read_u32(directory + 4) != ROAP_ASSET_DIRECTORY_VERSION) {
        return 0;
    }
    uint64_t directory_end = (uint64_t)directory_offset + sizeof(RoapAssetDirectoryHeader) +
                             (uint64_t)fuselage_guest_read_u32(directory + 8) * sizeof(RoapAssetEntry);
    return directory_end <= payload_limit;
}

MislHostImageResult fuselage_guest_plan_image(const MislGuestImage* image, uint32_t memory_cap_mib,
                                              MislHostImagePlan* output) {
    if (!output) {
        return MISL_HOST_IMAGE_ARGUMENT;
    }
    *output = (MislHostImagePlan){0};
    if (!image || !image->bytes || image->byte_count == 0) {
        return MISL_HOST_IMAGE_ARGUMENT;
    }
    if (image->byte_count > FUSELAGE_GUEST_MAX_FILE_BYTES) {
        return MISL_HOST_IMAGE_FILE_SIZE;
    }
    MislHostImagePlan plan = {0};
    if (!fuselage_guest_split_image(image, &plan)) {
        return MISL_HOST_IMAGE_TRAILER;
    }
    if (plan.program_image_bytes < 2) {
        return MISL_HOST_IMAGE_PROGRAM_SIZE;
    }
    uint32_t manifest_entry = 0;
    int has_manifest =
        plan.program_image_bytes >= 4 && fuselage_guest_read_u32(image->bytes) == ROAP_MANIFEST_MAGIC;
    if (has_manifest) {
        manifest_entry = ROAP_GetManifestCodeOffset(image->bytes, plan.program_image_bytes);
        if (!manifest_entry || image->bytes[offsetof(RoapManifest, poolCount)] > ROAP_MANIFEST_MAX_POOLS) {
            return MISL_HOST_IMAGE_MANIFEST;
        }
        uint32_t count = image->bytes[offsetof(RoapManifest, poolCount)];
        for (uint32_t index = 0; index < count; index++) {
            uint32_t offset = (uint32_t)offsetof(RoapManifest, poolMB) + index * 2;
            if (image->bytes[offset] == 0 && image->bytes[offset + 1] == 0) {
                return MISL_HOST_IMAGE_MANIFEST;
            }
        }
    }
    plan.entry_offset = image->entry_offset ? image->entry_offset : manifest_entry;
    if ((plan.entry_offset & 1u) || plan.entry_offset >= plan.program_image_bytes ||
        plan.program_image_bytes - plan.entry_offset < 2 ||
        (has_manifest && plan.entry_offset < ROAP_MANIFEST_BYTES)) {
        return MISL_HOST_IMAGE_ENTRY;
    }
    if (ROAP_GetManifestRomMinimumVersion(image->bytes, plan.program_image_bytes) > MISL_ROM_VERSION) {
        return MISL_HOST_IMAGE_ROM_VERSION;
    }
    plan.save_bytes =
        ROAP_GetManifestSram(image->bytes, plan.program_image_bytes, plan.save_id, &plan.save_version);
    uint64_t placement_top = MISL_SHARED_TOP;
    if (plan.asset_byte_count) {
        if (plan.asset_byte_count > placement_top) {
            return MISL_HOST_IMAGE_ADDRESS_SPACE;
        }
        placement_top = (placement_top - plan.asset_byte_count) & ~(uint64_t)(MISL_PLACE_ALIGN - 1u);
        plan.asset_base = (uint32_t)placement_top;
    }
    if (plan.save_bytes) {
        if (plan.save_bytes > placement_top) {
            return MISL_HOST_IMAGE_ADDRESS_SPACE;
        }
        placement_top = (placement_top - plan.save_bytes) & ~(uint64_t)(MISL_PLACE_ALIGN - 1u);
        plan.save_base = (uint32_t)placement_top;
    }
    uint16_t pool_mib[ROAP_MANIFEST_MAX_POOLS];
    uint32_t program_mib =
        ROAP_GetManifestMemory(image->bytes, plan.program_image_bytes, &plan.pool_count, pool_mib);
    uint64_t program_bytes = program_mib ? (uint64_t)program_mib << 20 : MISL_MEM_SIZE;
    uint64_t allocation_bytes = program_bytes;
    for (uint32_t index = 0; index < plan.pool_count; index++) {
        allocation_bytes += (uint64_t)pool_mib[index] << 20;
    }
    if (allocation_bytes > placement_top) {
        return MISL_HOST_IMAGE_ADDRESS_SPACE;
    }
    if (memory_cap_mib && allocation_bytes > ((uint64_t)memory_cap_mib << 20)) {
        return MISL_HOST_IMAGE_MEMORY_CAP;
    }
    if ((uint64_t)plan.program_image_bytes + MISL_STACK_RESERVE > program_bytes) {
        return MISL_HOST_IMAGE_PROGRAM_SIZE;
    }
    plan.program_bytes = (uint32_t)program_bytes;
    plan.allocation_bytes = (uint32_t)allocation_bytes;
    uint32_t pool_base = plan.program_bytes;
    for (uint32_t index = 0; index < plan.pool_count; index++) {
        plan.pool_base[index] = pool_base;
        plan.pool_bytes[index] = (uint32_t)pool_mib[index] << 20;
        pool_base += plan.pool_bytes[index];
    }
    *output = plan;
    return MISL_HOST_IMAGE_OK;
}

const char* fuselage_guest_get_image_result_message(MislHostImageResult result) {
    switch (result) {
    case MISL_HOST_IMAGE_OK:
        return "Image layout accepted";
    case MISL_HOST_IMAGE_ARGUMENT:
        return "Missing image bytes or output plan";
    case MISL_HOST_IMAGE_FILE_SIZE:
        return "ROAP file exceeds the host address-space limit";
    case MISL_HOST_IMAGE_TRAILER:
        return "Invalid or unsupported asset trailer/directory";
    case MISL_HOST_IMAGE_MANIFEST:
        return "Invalid or unsupported ROAP manifest";
    case MISL_HOST_IMAGE_ENTRY:
        return "Entry is outside the program image or is not two-byte aligned";
    case MISL_HOST_IMAGE_ROM_VERSION:
        return "ROAP requires a newer BOOTSPUR version";
    case MISL_HOST_IMAGE_ADDRESS_SPACE:
        return "Declared regions exceed the shared address space";
    case MISL_HOST_IMAGE_MEMORY_CAP:
        return "Declared RAM exceeds the host memory cap";
    case MISL_HOST_IMAGE_PROGRAM_SIZE:
        return "Program image does not fit with its stack reserve";
    default:
        return "Unknown image layout result";
    }
}
