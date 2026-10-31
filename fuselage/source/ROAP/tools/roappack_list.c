/* Asset record validation and listing. SEE: (ROAPPACK.md, Listing assets) */
#include "roappack_internal.h"
#include <stdlib.h>

static int roappack_compare_ids(const void* left, const void* right) {
    uint32_t first = *(const uint32_t*)left, second = *(const uint32_t*)right;
    return (first > second) - (first < second);
}

int roappack_list(const char* path) {
    uint32_t file_size;
    unsigned char* file_bytes = roappack_read_file(path, &file_size, roappack_max_bytes);
    if (file_size < ROAP_ASSET_TRAILER_BYTES ||
        roappack_read32(file_bytes + file_size - ROAP_ASSET_TRAILER_BYTES) != ROAP_ASSET_TRAILER_MAGIC) {
        printf("%s: %u bytes, no asset ROM (no trailer)\n", path, file_size);
        free(file_bytes);
        return 0;
    }
    const unsigned char* trailer = file_bytes + file_size - ROAP_ASSET_TRAILER_BYTES;
    uint32_t version = roappack_read32(trailer + 4);
    uint32_t directory_offset = roappack_read32(trailer + 8);
    uint32_t payload_size = roappack_read32(trailer + 12);
    if (version != ROAP_ASSET_DIRECTORY_VERSION) {
        roappack_die(path, "unsupported asset trailer version");
    }
    if (payload_size < ROAP_ASSET_TRAILER_BYTES + 12u || payload_size > file_size) {
        roappack_die(path, "invalid asset payload size");
    }
    uint32_t payload_limit = payload_size - ROAP_ASSET_TRAILER_BYTES;
    if (directory_offset > payload_limit - 12u) {
        roappack_die(path, "directory header is outside the payload data");
    }
    uint32_t image_size = file_size - payload_size;
    const unsigned char* payload = file_bytes + image_size;
    const unsigned char* directory = payload + directory_offset;
    if (roappack_read32(directory) != ROAP_ASSET_DIRECTORY_MAGIC ||
        roappack_read32(directory + 4) != ROAP_ASSET_DIRECTORY_VERSION) {
        roappack_die(path, "unsupported asset directory magic or version");
    }
    uint32_t count = roappack_read32(directory + 8);
    uint64_t directory_end = (uint64_t)directory_offset + 12u + (uint64_t)count * 20u;
    if (directory_end > payload_limit || (uint64_t)count * sizeof(uint32_t) > SIZE_MAX) {
        roappack_die(path, "directory entries extend outside the payload data");
    }
    uint32_t* ids = count ? malloc((size_t)count * sizeof *ids) : NULL;
    if (count && !ids) {
        roappack_die(path, "out of memory");
    }
    for (uint32_t index = 0; index < count; index++) {
        const unsigned char* entry = directory + 12u + (size_t)index * 20u;
        uint32_t offset = roappack_read32(entry + 4), size = roappack_read32(entry + 8);
        uint32_t raw_size = roappack_read32(entry + 12), flags = roappack_read32(entry + 16);
        uint64_t end = (uint64_t)offset + size;
        if (end > payload_limit || (size && offset < directory_end && end > directory_offset)) {
            roappack_die(path, "section extent is outside data or overlaps the directory");
        }
        if (flags == ROAP_ASSET_FLAG_RAW && raw_size != size) {
            roappack_die(path, "raw section has inconsistent stored and decoded sizes");
        }
        ids[index] = roappack_read32(entry);
    }
    if (count) {
        qsort(ids, count, sizeof *ids, roappack_compare_ids);
        for (uint32_t index = 1; index < count; index++) {
            if (ids[index - 1] == ids[index]) {
                roappack_die(path, "duplicate section ID");
            }
        }
    }
    free(ids);
    printf("%s\n  file            %u bytes\n  program image   %u bytes\n  asset ROM       %u bytes\n"
           "  directory       +%u, version %u\n  sections        %u\n\n",
           path, file_size, image_size, payload_size, directory_offset, version, count);
    printf("    %-10s %-12s %-12s %-12s %s\n", "ID", "OFFSET", "STORED", "RAW", "CODEC");
    for (uint32_t index = 0; index < count; index++) {
        const unsigned char* entry = directory + 12u + (size_t)index * 20u;
        printf("    %-10u %-12u %-12u %-12u %s\n", roappack_read32(entry), roappack_read32(entry + 4),
               roappack_read32(entry + 8), roappack_read32(entry + 12),
               roappack_read32(entry + 16) == ROAP_ASSET_FLAG_RAW ? "raw" : "needs a guest decoder");
    }
    free(file_bytes);
    return 0;
}
