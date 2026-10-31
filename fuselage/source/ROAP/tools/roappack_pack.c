/* Raw asset writer. SEE: (ROAPPACK.md, Packing assets) */
#include "roappack_internal.h"
#include <stdlib.h>

int roappack_pack(const char* path, const char* image, const RoapPackSection* sections, uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        for (uint32_t other = index + 1; other < count; other++) {
            if (sections[index].id == sections[other].id) {
                roappack_die(sections[index].path, "duplicate section ID");
            }
        }
    }
    uint32_t image_size = 0;
    unsigned char* image_bytes = image ? roappack_read_file(image, &image_size, roappack_max_bytes) : NULL;
    if (image_size >= ROAP_ASSET_TRAILER_BYTES &&
        roappack_read32(image_bytes + image_size - ROAP_ASSET_TRAILER_BYTES) == ROAP_ASSET_TRAILER_MAGIC) {
        roappack_die(image, "already carries an asset trailer; use the original program image");
    }
    unsigned char* data[ROAPPACK_MAX_SECTIONS];
    RoapAssetEntry directory[ROAPPACK_MAX_SECTIONS];
    uint64_t payload_size = 0;
    uint64_t metadata_size = 12u + (uint64_t)count * 20u + ROAP_ASSET_TRAILER_BYTES;
    /* Read and validate inputs before creating output, including same-path inputs. */
    for (uint32_t index = 0; index < count; index++) {
        payload_size = (payload_size + 3u) & ~(uint64_t)3u;
        uint64_t required_bytes = (uint64_t)image_size + payload_size + metadata_size;
        if (required_bytes > roappack_max_bytes) {
            roappack_die(path, "combined output exceeds the byte limit");
        }
        uint32_t size;
        data[index] =
            roappack_read_file(sections[index].path, &size, (uint32_t)(roappack_max_bytes - required_bytes));
        directory[index].id = sections[index].id;
        directory[index].offset = (uint32_t)payload_size;
        directory[index].size = size;
        directory[index].rawSize = size;
        directory[index].flags = ROAP_ASSET_FLAG_RAW;
        payload_size += size;
    }
    payload_size = (payload_size + 3u) & ~(uint64_t)3u;
    if ((uint64_t)image_size + payload_size + metadata_size > roappack_max_bytes) {
        roappack_die(path, "combined output exceeds the byte limit");
    }
    uint32_t directory_offset = (uint32_t)payload_size;
    FILE* output = roappack_open_output(path);
    roappack_write(output, image_bytes, image_size);
    free(image_bytes);
    uint32_t written_payload = 0;
    const unsigned char zeros[4] = {0};
    for (uint32_t index = 0; index < count; index++) {
        uint32_t padding = directory[index].offset - written_payload;
        roappack_write(output, zeros, padding);
        roappack_write(output, data[index], directory[index].size);
        free(data[index]);
        written_payload = directory[index].offset + directory[index].size;
    }
    roappack_write(output, zeros, directory_offset - written_payload);
    unsigned char encoded[20];
    roappack_encode32(encoded, ROAP_ASSET_DIRECTORY_MAGIC);
    roappack_encode32(encoded + 4, ROAP_ASSET_DIRECTORY_VERSION);
    roappack_encode32(encoded + 8, count);
    roappack_write(output, encoded, 12);
    for (uint32_t index = 0; index < count; index++) {
        roappack_encode32(encoded, directory[index].id);
        roappack_encode32(encoded + 4, directory[index].offset);
        roappack_encode32(encoded + 8, directory[index].size);
        roappack_encode32(encoded + 12, directory[index].rawSize);
        roappack_encode32(encoded + 16, directory[index].flags);
        roappack_write(output, encoded, 20);
    }
    roappack_encode32(encoded, ROAP_ASSET_TRAILER_MAGIC);
    roappack_encode32(encoded + 4, ROAP_ASSET_DIRECTORY_VERSION);
    roappack_encode32(encoded + 8, directory_offset);
    roappack_encode32(encoded + 12, (uint32_t)(payload_size + metadata_size));
    roappack_write(output, encoded, ROAP_ASSET_TRAILER_BYTES);
    roappack_commit_output(output, path);
    printf("%u sections, %u asset bytes -> %s%s\n", count, (uint32_t)(payload_size + metadata_size), path,
           image ? " (attached to program image)" : "");
    return 0;
}
