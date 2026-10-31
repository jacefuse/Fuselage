// Shared viewport fitting and mask coordinate conversion.
// SEE: (GDMF_VULKAN.md, Viewport and composition recording).
#include "gdmf_vulkan_geometry.h"
#include <math.h>

bool gdmf_vulkan_viewport_rect(VkExtent2D extent, uint32_t aspect_numerator,
    uint32_t aspect_denominator, VkRect2D* viewport) {
    if (!extent.width || !extent.height || !aspect_numerator || !aspect_denominator) { return false; }
    uint64_t width = extent.width;
    uint64_t height = width * aspect_denominator / aspect_numerator;
    if (height > extent.height) {
        height = extent.height;
        width = height * aspect_numerator / aspect_denominator;
    }
    if (!width || !height) { return false; }
    *viewport = (VkRect2D){
        .offset = {(int32_t)((extent.width - width) / 2), (int32_t)((extent.height - height) / 2)},
        .extent = {(uint32_t)width, (uint32_t)height}
    };
    return true;
}

bool gdmf_vulkan_mask_rect(VkExtent2D extent, VkRect2D viewport,
    const float canvas_rect[4], float swapchain_rect[4]) {
    if (!extent.width || !extent.height || !viewport.extent.width || !viewport.extent.height) { return false; }
    float converted[4];
    for (uint32_t coordinate = 0; coordinate < 4; coordinate += 2) {
        if (!isfinite(canvas_rect[coordinate]) || !isfinite(canvas_rect[coordinate + 1])) { return false; }
        converted[coordinate] = (((float)viewport.offset.x +
            (canvas_rect[coordinate] + 1.0f) * 0.5f * (float)viewport.extent.width) / (float)extent.width) * 2.0f - 1.0f;
        converted[coordinate + 1] = (((float)viewport.offset.y +
            (canvas_rect[coordinate + 1] + 1.0f) * 0.5f * (float)viewport.extent.height) / (float)extent.height) * 2.0f - 1.0f;
        if (!isfinite(converted[coordinate]) || !isfinite(converted[coordinate + 1])) { return false; }
    }
    for (uint32_t coordinate = 0; coordinate < 4; coordinate++) { swapchain_rect[coordinate] = converted[coordinate]; }
    return true;
}
