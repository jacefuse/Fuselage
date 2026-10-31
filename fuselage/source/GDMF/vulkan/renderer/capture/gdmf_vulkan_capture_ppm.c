// Binary PPM output from completed four-channel frame readback.
// SEE: (GDMF_VULKAN.md, Capture image output).
#include "gdmf_vulkan_capture_ppm.h"
#include <stdio.h>
#include <stdlib.h>

bool gdmf_vulkan_capture_write_ppm(const char* path, const gdmf_vulkan_capture_image* image) {
    if (!path || !*path || !image || !image->pixels ||
        !image->image_extent.width || !image->image_extent.height ||
        !image->viewport.extent.width || !image->viewport.extent.height) { return false; }

    bool swap_red_blue;
    switch (image->format) {
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        swap_red_blue = true;
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        swap_red_blue = false;
        break;
    default:
        return false;
    }

    size_t image_bytes = image->image_extent.width;
    if (image->image_extent.height > SIZE_MAX / image_bytes) { return false; }
    image_bytes *= image->image_extent.height;
    if (image_bytes > SIZE_MAX / 4) { return false; }
    image_bytes *= 4;
    if (image->byte_count < image_bytes) { return false; }

    VkExtent2D canvas = image->canvas_extent;
    if (!canvas.width || !canvas.height) { canvas = image->image_extent; }
    size_t row_bytes = canvas.width;
    if (row_bytes > SIZE_MAX / 3) { return false; }
    row_bytes *= 3;
    uint8_t* row = malloc(row_bytes);
    if (!row) { return false; }
    FILE* file = fopen(path, "wb");
    if (!file) { free(row); return false; }

    bool success = fprintf(file,
        "P6\n# FUSELAGE canvas=%ux%u window=%ux%u rect=%d,%d,%u,%u\n%u %u\n255\n",
        canvas.width, canvas.height, image->image_extent.width, image->image_extent.height,
        image->viewport.offset.x, image->viewport.offset.y,
        image->viewport.extent.width, image->viewport.extent.height,
        canvas.width, canvas.height) >= 0;

    for (uint32_t canvas_y = 0; success && canvas_y < canvas.height; canvas_y++) {
        int64_t source_y = (int64_t)image->viewport.offset.y +
            (int64_t)(((double)canvas_y + 0.5) * image->viewport.extent.height / canvas.height);
        if (source_y < 0) { source_y = 0; }
        if (source_y >= image->image_extent.height) { source_y = (int64_t)image->image_extent.height - 1; }
        for (uint32_t canvas_x = 0; canvas_x < canvas.width; canvas_x++) {
            int64_t source_x = (int64_t)image->viewport.offset.x +
                (int64_t)(((double)canvas_x + 0.5) * image->viewport.extent.width / canvas.width);
            if (source_x < 0) { source_x = 0; }
            if (source_x >= image->image_extent.width) { source_x = (int64_t)image->image_extent.width - 1; }
            const uint8_t* pixel = image->pixels +
                ((size_t)source_y * image->image_extent.width + (size_t)source_x) * 4;
            size_t row_offset = (size_t)canvas_x * 3;
            row[row_offset] = swap_red_blue ? pixel[2] : pixel[0];
            row[row_offset + 1] = pixel[1];
            row[row_offset + 2] = swap_red_blue ? pixel[0] : pixel[2];
        }
        success = fwrite(row, 1, row_bytes, file) == row_bytes;
    }
    free(row);
    if (fclose(file) != 0) { success = false; }
    return success;
}
