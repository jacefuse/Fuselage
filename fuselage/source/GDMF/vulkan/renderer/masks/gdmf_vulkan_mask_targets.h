#ifndef GDMF_VULKAN_MASK_TARGETS_H
#define GDMF_VULKAN_MASK_TARGETS_H

#include <vulkan/vulkan.h>

typedef struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView image_view;
    VkFramebuffer framebuffer;
} gdmf_vulkan_mask_target;

// Device/render pass are borrowed; memory properties and geometry are copied.
typedef struct {
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkRenderPass render_pass;
    VkFormat format;
    VkExtent2D extent;
    uint32_t image_count;
    uint32_t masks_per_image;
} gdmf_vulkan_mask_target_config;

// Caller owns the target array and all created image/memory/view/framebuffers.
typedef struct {
    gdmf_vulkan_mask_target_config config;
    uint32_t target_count;
    gdmf_vulkan_mask_target* targets;
} gdmf_vulkan_mask_targets;

// Render-thread setup. Supply valid pointers and compatible format/render pass.
// Returns 0 on bookkeeping allocation success, -1 for invalid dimensions/counts
// or allocation failure. Output changes only on success; targets remain lazy.
// Do not overwrite a live collection. Does not choose the mask-capacity policy.
int gdmf_vulkan_mask_targets_create(const gdmf_vulkan_mask_target_config* config,
    gdmf_vulkan_mask_targets* targets);

// Render thread, after this image's prior work completes. Lazily creates the
// target; returns a borrowed pointer or NULL for invalid indices/failure.
// Returned objects stay valid until collection destruction; do not destroy them.
const gdmf_vulkan_mask_target* gdmf_vulkan_mask_targets_ensure(gdmf_vulkan_mask_targets* targets,
    uint32_t image_index, uint32_t mask_slot);

// Stop users and complete GPU work first. Retire descriptors referencing views.
// Supply a zeroed or successful collection; clears it. Does not wait or destroy
// the borrowed device/render pass. SEE: (GDMF_VULKAN.md, Mask offscreen targets).
void gdmf_vulkan_mask_targets_destroy(gdmf_vulkan_mask_targets* targets);

#endif // GDMF_VULKAN_MASK_TARGETS_H
