#ifndef GDMF_VULKAN_GEOMETRY_H
#define GDMF_VULKAN_GEOMETRY_H

#include <vulkan/vulkan.h>
#include <stdbool.h>

// Center a fitted aspect-ratio rectangle in extent. Returns false for zero
// inputs or a fitted dimension that rounds to zero; output stays unchanged.
bool gdmf_vulkan_viewport_rect(VkExtent2D extent, uint32_t aspect_numerator,
    uint32_t aspect_denominator, VkRect2D* viewport);

// Map canvas normalized coordinates through viewport into swapchain normalized
// coordinates. Returns false on zero geometry or nonfinite coordinates/results;
// output stays unchanged. Input/output may alias. Does not clamp the rectangle.
bool gdmf_vulkan_mask_rect(VkExtent2D extent, VkRect2D viewport,
    const float canvas_rect[4], float swapchain_rect[4]);

#endif // GDMF_VULKAN_GEOMETRY_H
