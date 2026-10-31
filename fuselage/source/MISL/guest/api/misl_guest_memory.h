#pragma once
#include "misl_guest_call_internal.h"
#include "../runtime/misl_guest_cache_internal.h"
#include "../../bootspur/misl_bootspur_services.h"
#include "../../misl_mmio.h"
#include "../../bootspur/misl_rom.h"
#include "../../../ROAP/roap_manifest.h"

// Region discovery, mapped assets, memory pools and save reads.
// SEE: (MISL_GUEST.md, Discovery and mapped data)

static inline const volatile MislDescriptor* MISL_GetDescriptor(void) {
    return misl_guest_descriptor;
}

static inline uint32_t MISL_GetRegionBase(uint32_t id) {
    return (id < MISL_DESC_MAX_REGIONS) ? misl_guest_region_bases[id] : 0u;
}

static inline uint32_t MISL_GetRegionSize(uint32_t id) {
    const volatile MislDescriptor* descriptor = misl_guest_descriptor;
    if (!descriptor) {
        return 0u;
    }
    for (uint32_t region_index = 0; region_index < descriptor->regionCount; region_index++) {
        if (descriptor->region[region_index].id == id) {
            return descriptor->region[region_index].size;
        }
    }
    return 0u;
}

// Startup discovery checks magic and caches known IDs; it is not a full descriptor validator.
static inline bool MISL_DiscoverRegions(void) {
    uint32_t descriptor_address = (uint32_t)MISL_GUEST_CALL(MISL_SYS_DESCRIPTOR, 0, 0, 0, 0, 0, 0, 0);
    if (!descriptor_address) {
        return false;
    }

    const volatile MislDescriptor* descriptor = (const volatile MislDescriptor*)(uintptr_t)descriptor_address;
    if (descriptor->magic != MISL_DESC_MAGIC) {
        return false;
    }

    for (uint32_t region_index = 0; region_index < MISL_DESC_MAX_REGIONS; region_index++) {
        misl_guest_region_bases[region_index] = 0u;
    }
    for (uint32_t region_index = 0;
         region_index < descriptor->regionCount && region_index < MISL_DESC_MAX_REGIONS; region_index++) {
        uint32_t region_id = descriptor->region[region_index].id;
        if (region_id < MISL_DESC_MAX_REGIONS) {
            misl_guest_region_bases[region_id] = descriptor->region[region_index].base;
        }
    }
    misl_guest_descriptor = descriptor;
    return true;
}

static inline uint32_t misl_guest_read_asset_word(uint32_t base, uint32_t offset) {
    const volatile unsigned char* bytes = (const volatile unsigned char*)(uintptr_t)(base + offset);
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static inline uint32_t MISL_GetAssetBase(void) {
    return misl_guest_region_bases[MISL_REGION_ASSET_ROM];
}

static inline uint32_t MISL_GetAssetSize(void) {
    return MISL_GetRegionSize(MISL_REGION_ASSET_ROM);
}

static inline bool MISL_HasAssets(void) {
    return MISL_GetAssetBase() != 0u;
}

static inline uint32_t MISL_GetPoolCount(void) {
    uint32_t pool_count = 0u;
    while (pool_count < ROAP_MANIFEST_MAX_POOLS &&
           misl_guest_region_bases[MISL_REGION_POOL + pool_count] != 0u) {
        pool_count++;
    }
    return pool_count;
}

static inline uint32_t MISL_GetPoolBase(uint32_t pool_index) {
    return (pool_index < ROAP_MANIFEST_MAX_POOLS) ? misl_guest_region_bases[MISL_REGION_POOL + pool_index]
                                                  : 0u;
}

static inline uint32_t MISL_GetPoolSize(uint32_t pool_index) {
    return (pool_index < ROAP_MANIFEST_MAX_POOLS) ? MISL_GetRegionSize(MISL_REGION_POOL + pool_index) : 0u;
}

static inline uint32_t MISL_GetProgramSize(void) {
    return MISL_GetRegionSize(MISL_REGION_PROGRAM);
}

// False means absent or invalid; entry may already contain partially read fields.
// This reader does not provide the packer's full file validation.
static inline bool MISL_FindAsset(uint32_t asset_id, RoapAssetEntry* entry) {
    uint32_t payload_base = MISL_GetAssetBase();
    uint32_t payload_bytes = MISL_GetAssetSize();
    if (!payload_base || !entry) {
        return false;
    }

    if (payload_bytes < ROAP_ASSET_TRAILER_BYTES + (uint32_t)sizeof(RoapAssetDirectoryHeader)) {
        return false;
    }
    uint32_t data_limit = payload_bytes - ROAP_ASSET_TRAILER_BYTES;

    uint32_t directory_offset =
        misl_guest_read_asset_word(payload_base, payload_bytes - ROAP_ASSET_TRAILER_BYTES + 8u);
    if (directory_offset > data_limit - (uint32_t)sizeof(RoapAssetDirectoryHeader)) {
        return false;
    }
    if (misl_guest_read_asset_word(payload_base, directory_offset) != ROAP_ASSET_DIRECTORY_MAGIC) {
        return false;
    }

    uint32_t entry_count = misl_guest_read_asset_word(payload_base, directory_offset + 8u);
    uint32_t entry_capacity = (data_limit - (uint32_t)sizeof(RoapAssetDirectoryHeader) - directory_offset) /
                              (uint32_t)sizeof(RoapAssetEntry);
    if (entry_count > entry_capacity) {
        entry_count = entry_capacity;
    }

    for (uint32_t entry_index = 0; entry_index < entry_count; entry_index++) {
        uint32_t entry_offset = directory_offset + (uint32_t)sizeof(RoapAssetDirectoryHeader) +
                                entry_index * (uint32_t)sizeof(RoapAssetEntry);
        if (misl_guest_read_asset_word(payload_base, entry_offset) != asset_id) {
            continue;
        }
        entry->id = asset_id;
        entry->offset = misl_guest_read_asset_word(payload_base, entry_offset + 4u);
        entry->size = misl_guest_read_asset_word(payload_base, entry_offset + 8u);
        entry->rawSize = misl_guest_read_asset_word(payload_base, entry_offset + 12u);
        entry->flags = misl_guest_read_asset_word(payload_base, entry_offset + 16u);

        if (entry->offset > payload_bytes || entry->size > payload_bytes - entry->offset) {
            return false;
        }
        return true;
    }
    return false;
}

static inline const volatile void* MISL_GetAssetData(uint32_t asset_id, uint32_t* size) {
    RoapAssetEntry entry;
    if (!MISL_FindAsset(asset_id, &entry)) {
        if (size) {
            *size = 0u;
        }
        return 0;
    }
    if (size) {
        *size = entry.size;
    }
    return (const volatile void*)(uintptr_t)(MISL_GetAssetBase() + entry.offset);
}

static inline const volatile MislInput* MISL_GetInput(void) {
    return (const volatile MislInput*)(uintptr_t)misl_guest_region_bases[MISL_REGION_INPUT];
}

static inline const volatile MislClock* MISL_GetClock(void) {
    return (const volatile MislClock*)(uintptr_t)misl_guest_region_bases[MISL_REGION_CLOCK];
}

static inline bool MISL_IsClockAvailable(void) {
    return misl_guest_region_bases[MISL_REGION_CLOCK] != 0u;
}

static inline uint32_t MISL_GetSystemRomBase(void) {
    return misl_guest_region_bases[MISL_REGION_BOOTSPUR];
}

static inline bool MISL_IsSystemRomAvailable(void) {
    return misl_guest_region_bases[MISL_REGION_BOOTSPUR] != 0u;
}

static inline const volatile MislRomHeader* MISL_GetSystemRomHeader(void) {
    return (const volatile MislRomHeader*)(uintptr_t)misl_guest_region_bases[MISL_REGION_BOOTSPUR];
}

static inline uint32_t MISL_GetSramSize(void) {
    return misl_guest_region_bases[MISL_REGION_SRAM] ? MISL_GetRegionSize(MISL_REGION_SRAM) : 0u;
}

static inline bool MISL_IsSramAvailable(void) {
    return misl_guest_region_bases[MISL_REGION_SRAM] != 0u;
}

static inline bool MISL_ReadSram(uint32_t offset, void* destination, uint32_t bytes) {
    const uint32_t size = MISL_GetSramSize();
    if (!destination || bytes == 0u || bytes > size || offset > size - bytes) {
        return false;
    }
    __builtin_memcpy(destination,
                     (const void*)(uintptr_t)(misl_guest_region_bases[MISL_REGION_SRAM] + offset), bytes);
    return true;
}
