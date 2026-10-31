#ifndef GDMF_VULKAN_MASKS_H
#define GDMF_VULKAN_MASKS_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stddef.h>

// Matches the fragment shader's vec4 rectangle followed by uint inversion flag.
typedef struct {
    float normalized_rect[4];
    uint32_t inverted;
} gdmf_vulkan_mask_push_constants;
_Static_assert(offsetof(gdmf_vulkan_mask_push_constants, inverted) == 16, "Mask inversion offset must be 16.");
_Static_assert(sizeof(gdmf_vulkan_mask_push_constants) == 20, "Mask push constants must occupy 20 bytes.");

typedef struct {
    const uint32_t* words;
    size_t byte_count;
} gdmf_vulkan_mask_shader;

// All handles and shader inputs are borrowed. Shaders must contain valid,
// suitably aligned SPIR-V with the existing mask binding/push-constant interface.
typedef struct {
    VkDevice device;
    VkRenderPass presentation_render_pass;
    VkPipelineCache pipeline_cache;
    VkFormat format;
    uint32_t image_count;
    uint32_t masks_per_image;
    gdmf_vulkan_mask_shader vertex_shader;
    gdmf_vulkan_mask_shader fragment_shader;
} gdmf_vulkan_mask_config;

// Caller owns all handles except device; sets are reclaimed with descriptor_pool.
// Offscreen image targets are a separate collection borrowing offscreen_render_pass.
typedef struct {
    VkDevice device;
    VkFormat format;
    uint32_t image_count;
    uint32_t masks_per_image;
    VkRenderPass offscreen_render_pass;
    VkSampler sampler;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet* descriptor_sets;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
} gdmf_vulkan_masks;

// Render-thread setup, normally on first mask use. Returns 0 on success or -1
// on invalid inputs/failure. Cleans partial resources; output changes only on
// success. Do not overwrite live state. Does not create target images or draw.
int gdmf_vulkan_masks_create(const gdmf_vulkan_mask_config* config, gdmf_vulkan_masks* masks);

// After the image's prior GPU/descriptor use completes and before recording.
// Supply valid shader-readable views; returns false for invalid slot/views.
// A true return means an update was issued, not GPU completion or layout proof.
bool gdmf_vulkan_masks_update_descriptor(gdmf_vulkan_masks* masks, uint32_t image_index,
    uint32_t mask_slot, VkImageView color_view, VkImageView mask_view);

// Stop users and complete GPU work first. Destroy the separate target collection
// before this borrowed render pass. Supply zeroed or successful state; clears it.
// Does not wait or release device/pipeline cache/presentation pass.
// SEE: (GDMF_VULKAN.md, Mask compositor setup).
void gdmf_vulkan_masks_destroy(gdmf_vulkan_masks* masks);

#endif // GDMF_VULKAN_MASKS_H
