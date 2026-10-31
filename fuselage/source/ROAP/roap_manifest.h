/* ROAP project manifest. SEE: (ROAP_FORMAT.md, Manifest) */
#ifndef ROAP_MANIFEST_H
#define ROAP_MANIFEST_H

#include <stdint.h>
#include <stddef.h>

#define ROAP_MANIFEST_MAGIC 0x4C53494Du
#define ROAP_MANIFEST_VERSION 1u

#define ROAP_MANIFEST_RATE_SCALE 1000u

#define ROAP_MANIFEST_HIDE_CURSOR 0x0001u
#define ROAP_MANIFEST_BANNER 0x0002u

/* Little-endian 128-byte record; field names and offsets are retained. */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t codeOffset;

    uint16_t canvasWidth, canvasHeight;
    uint16_t windowWidth, windowHeight;
    uint16_t aspectNum, aspectDen;
    uint16_t audioPoolMB;
    uint16_t flags;

    /* Rates are in thousandths of a hertz. */
    uint32_t simRateMilli;
    uint32_t targetFPSMilli;

    char title[32];
    uint32_t romMinVersion; /* Required BOOTSPUR version; zero accepts any. */
    char sramId[16];
    uint32_t sramVersion;
    uint32_t sramBytes;

    uint16_t programPoolMB;
    uint8_t poolCount;
    uint8_t poolReserved;
    uint16_t poolMB[8];

    uint32_t reserved[4];
} RoapManifest;

#define ROAP_MANIFEST_MAX_POOLS 8u

#define ROAP_MANIFEST_BYTES 128u

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RoapManifest) == ROAP_MANIFEST_BYTES, "ROAP manifest size changed");
#endif

/* Zero selects legacy entry zero or reports an unrecognized/invalid manifest. */
static inline uint32_t ROAP_GetManifestCodeOffset(const void* image, uint32_t image_size) {
    const unsigned char* bytes = (const unsigned char*)image;
    uint32_t magic, version, offset;

    if (!bytes || image_size < ROAP_MANIFEST_BYTES) {
        return 0u;
    }

    magic = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
            ((uint32_t)bytes[3] << 24);
    version = (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8);
    offset = (uint32_t)bytes[6] | ((uint32_t)bytes[7] << 8);

    if (magic != ROAP_MANIFEST_MAGIC) {
        return 0u;
    }
    if (version != ROAP_MANIFEST_VERSION) {
        return 0u;
    }

    if (offset < ROAP_MANIFEST_BYTES || offset >= image_size) {
        return 0u;
    }

    return offset;
}

/* Zero means no recognized minimum requirement. */
static inline uint32_t ROAP_GetManifestRomMinimumVersion(const void* image, uint32_t image_size) {
    const unsigned char* bytes = (const unsigned char*)image;
    if (!bytes || image_size < ROAP_MANIFEST_BYTES) {
        return 0u;
    }
    uint32_t magic = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                     ((uint32_t)bytes[3] << 24);
    if (magic != ROAP_MANIFEST_MAGIC) {
        return 0u;
    }
    uint32_t manifest_version = (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8);
    if (manifest_version != ROAP_MANIFEST_VERSION) {
        return 0u;
    }
    const uint32_t offset = (uint32_t)offsetof(RoapManifest, romMinVersion);
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1] << 8) |
           ((uint32_t)bytes[offset + 2] << 16) | ((uint32_t)bytes[offset + 3] << 24);
}

/* Return save bytes; id needs 17 writable bytes and version must be valid. */
static inline uint32_t ROAP_GetManifestSram(const void* image, uint32_t image_size, char id[17],
                                            uint32_t* version) {
    const unsigned char* bytes = (const unsigned char*)image;
    id[0] = '\0';
    *version = 0u;
    if (!bytes || image_size < ROAP_MANIFEST_BYTES) {
        return 0u;
    }
    uint32_t magic = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                     ((uint32_t)bytes[3] << 24);
    uint32_t manifest_version = (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8);
    if (magic != ROAP_MANIFEST_MAGIC || manifest_version != ROAP_MANIFEST_VERSION) {
        return 0u;
    }
    const uint32_t id_offset = (uint32_t)offsetof(RoapManifest, sramId);
    const uint32_t version_offset = (uint32_t)offsetof(RoapManifest, sramVersion);
    const uint32_t size_offset = (uint32_t)offsetof(RoapManifest, sramBytes);
    for (uint32_t index = 0; index < 16u; index++) {
        id[index] = (char)bytes[id_offset + index];
    }
    id[16] = '\0';
    *version = (uint32_t)bytes[version_offset] | ((uint32_t)bytes[version_offset + 1] << 8) |
               ((uint32_t)bytes[version_offset + 2] << 16) | ((uint32_t)bytes[version_offset + 3] << 24);
    return (uint32_t)bytes[size_offset] | ((uint32_t)bytes[size_offset + 1] << 8) |
           ((uint32_t)bytes[size_offset + 2] << 16) | ((uint32_t)bytes[size_offset + 3] << 24);
}

/* Return program MiB; ignore pool entries beyond the returned count. */
static inline uint32_t ROAP_GetManifestMemory(const void* image, uint32_t image_size, uint32_t* pool_count,
                                              uint16_t pool_mib[ROAP_MANIFEST_MAX_POOLS]) {
    const unsigned char* bytes = (const unsigned char*)image;
    uint32_t index;

    *pool_count = 0u;
    for (index = 0; index < ROAP_MANIFEST_MAX_POOLS; index++) {
        pool_mib[index] = 0u;
    }
    if (!bytes || image_size < ROAP_MANIFEST_BYTES) {
        return 0u;
    }

    uint32_t magic = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                     ((uint32_t)bytes[3] << 24);
    uint32_t manifest_version = (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8);
    if (magic != ROAP_MANIFEST_MAGIC || manifest_version != ROAP_MANIFEST_VERSION) {
        return 0u;
    }

    const uint32_t program_offset = (uint32_t)offsetof(RoapManifest, programPoolMB);
    const uint32_t count_offset = (uint32_t)offsetof(RoapManifest, poolCount);
    const uint32_t pools_offset = (uint32_t)offsetof(RoapManifest, poolMB);

    uint32_t program_mib = (uint32_t)bytes[program_offset] | ((uint32_t)bytes[program_offset + 1] << 8);
    uint32_t count = (uint32_t)bytes[count_offset];
    if (count > ROAP_MANIFEST_MAX_POOLS) {
        return program_mib;
    }

    for (index = 0; index < count; index++) {
        uint32_t pool_size = (uint32_t)bytes[pools_offset + index * 2u] |
                             ((uint32_t)bytes[pools_offset + index * 2u + 1u] << 8);
        if (pool_size == 0u) {
            return program_mib;
        }
        pool_mib[index] = (uint16_t)pool_size;
    }
    *pool_count = count;

    return program_mib;
}

#endif
