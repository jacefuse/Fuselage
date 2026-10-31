/* Linked ELF32 to flat image. SEE: (ROAPPACK.md, Flattening ELF) */
#include "roappack_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t file_offset, address, size;
} RoapPackSegment;

static int roappack_compare_segments(const void* left, const void* right) {
    uint32_t first = ((const RoapPackSegment*)left)->address;
    uint32_t second = ((const RoapPackSegment*)right)->address;
    return (first > second) - (first < second);
}

int roappack_flatten(const char* input_path, const char* output_path) {
    uint32_t file_size;
    unsigned char* elf = roappack_read_file(input_path, &file_size, roappack_max_bytes);
    if (file_size < 52 || memcmp(elf,
                                 "\x7f"
                                 "ELF",
                                 4) != 0) {
        roappack_die(input_path, "not an ELF file");
    }
    if (elf[4] != 1 || elf[5] != 1 || elf[6] != 1 || roappack_read32(elf + 20) != 1 ||
        roappack_read16(elf + 16) != 2 || roappack_read16(elf + 18) != 243 ||
        roappack_read16(elf + 40) < 52 || roappack_read16(elf + 40) > file_size) {
        roappack_die(input_path, "expected a linked little-endian RISC-V ELF32 executable");
    }
    uint32_t headers_offset = roappack_read32(elf + 28);
    uint16_t header_size = roappack_read16(elf + 42), header_count = roappack_read16(elf + 44);
    if (header_size < 32 || (uint64_t)headers_offset + (uint64_t)header_count * header_size > file_size) {
        roappack_die(input_path, "program headers extend outside the file");
    }
    RoapPackSegment* segments = header_count ? malloc((size_t)header_count * sizeof *segments) : NULL;
    if (header_count && !segments) {
        roappack_die(input_path, "out of memory");
    }
    uint32_t count = 0;
    for (uint32_t index = 0; index < header_count; index++) {
        const unsigned char* header = elf + headers_offset + (size_t)index * header_size;
        if (roappack_read32(header) != 1) {
            continue; /* PT_LOAD only. */
        }
        uint32_t offset = roappack_read32(header + 4), address = roappack_read32(header + 12);
        uint32_t size = roappack_read32(header + 16), memory_size = roappack_read32(header + 20);
        if (size > memory_size || (uint64_t)address + memory_size > (uint64_t)UINT32_MAX + 1u ||
            (size && ((uint64_t)offset + size > file_size || (uint64_t)address + size > UINT32_MAX))) {
            roappack_die(input_path, "invalid loadable segment extent");
        }
        if (size) {
            segments[count++] = (RoapPackSegment){offset, address, size};
        }
    }
    if (!count) {
        roappack_die(input_path, "no loadable segment with file bytes");
    }
    qsort(segments, count, sizeof *segments, roappack_compare_segments);
    if (segments[0].address != 0) {
        roappack_die(input_path, "lowest load address must be zero; ROAP loading does not relocate code");
    }
    uint64_t image_size = 0;
    for (uint32_t index = 0; index < count; index++) {
        if (segments[index].address < image_size) {
            roappack_die(input_path, "overlapping file-backed load segments");
        }
        image_size = (uint64_t)segments[index].address + segments[index].size;
    }
    if (image_size > roappack_max_bytes || image_size > SIZE_MAX) {
        roappack_die(input_path, "flat image exceeds the byte limit");
    }
    unsigned char* image = calloc((size_t)image_size, 1);
    if (!image) {
        roappack_die(input_path, "out of memory");
    }
    for (uint32_t index = 0; index < count; index++) {
        memcpy(image + segments[index].address, elf + segments[index].file_offset, segments[index].size);
    }
    FILE* output = roappack_open_output(output_path);
    roappack_write(output, image, (size_t)image_size);
    roappack_commit_output(output, output_path);
    printf("%s: %u bytes\n", output_path, (uint32_t)image_size);
    free(image);
    free(segments);
    free(elf);
    return 0;
}
