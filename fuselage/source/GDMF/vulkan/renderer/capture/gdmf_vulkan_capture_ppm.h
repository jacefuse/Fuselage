#ifndef GDMF_VULKAN_CAPTURE_PPM_H
#define GDMF_VULKAN_CAPTURE_PPM_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Borrowed, CPU-readable, tightly packed four-byte pixels in image row order.
// Vulkan readback and synchronization must already be complete.
typedef struct {
    const uint8_t* pixels;
    size_t byte_count;
    VkFormat format;
    VkExtent2D image_extent;
    VkRect2D viewport;
    VkExtent2D canvas_extent;
} gdmf_vulkan_capture_image;

// Write binary PPM RGB, sampling pixel centers through viewport and clamping
// to image bounds. Either zero canvas dimension selects image dimensions.
// Supports RGBA8/BGRA8 UNORM/SRGB only; alpha is discarded. Returns true only
// if validation, allocation, open, header/rows, and close all succeed.
// A write failure can leave a partial file; opening replaces an existing file.
// Supply valid pointers. This function does not access a Vulkan device.
// SEE: (GDMF_VULKAN.md, Capture image output).
bool gdmf_vulkan_capture_write_ppm(const char* path, const gdmf_vulkan_capture_image* image);

#endif // GDMF_VULKAN_CAPTURE_PPM_H
