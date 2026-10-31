/* Appended asset ROM. SEE: (ROAP_FORMAT.md, Asset ROM) */
#ifndef ROAP_ASSET_ROM_H
#define ROAP_ASSET_ROM_H
#include <stdint.h>

#define ROAP_ASSET_TRAILER_MAGIC 0x544D4F52u
#define ROAP_ASSET_TRAILER_BYTES 16u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t dirOffset;  /* From the payload start, not the file start. */
    uint32_t totalBytes; /* Includes this trailer. */
} RoapAssetTrailer;
#define ROAP_ASSET_DIRECTORY_MAGIC 0x314D4F52u
#define ROAP_ASSET_DIRECTORY_VERSION 1u

#define ROAP_ASSET_FLAG_RAW 0u
/* Codec values 1 and 2 are already allocated; do not reuse them. */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count; /* Number of entries following this header. */
} RoapAssetDirectoryHeader;

typedef struct {
    uint32_t id;
    uint32_t offset;  /* From the payload start. */
    uint32_t size;    /* Stored bytes. */
    uint32_t rawSize; /* Decoded bytes; equal to size for raw data. */
    uint32_t flags;
} RoapAssetEntry;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RoapAssetDirectoryHeader) == 12u, "ROM directory header is a wire format");
_Static_assert(sizeof(RoapAssetEntry) == 20u, "ROM directory entry is a wire format");
_Static_assert(sizeof(RoapAssetTrailer) == ROAP_ASSET_TRAILER_BYTES,
               "ROM trailer is a wire format and is found by its size");
#endif

#endif
