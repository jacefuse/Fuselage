#ifndef GDMF_VULKAN_COMPOSITION_H
#define GDMF_VULKAN_COMPOSITION_H

#include "../masks/gdmf_vulkan_masks.h"
#include "../masks/gdmf_vulkan_mask_targets.h"

#define GDMF_VULKAN_PRIORITY_COUNT 256

typedef struct {
    uint8_t priority;
    VkImageView mask_view;
    gdmf_vulkan_mask_push_constants push;
    bool valid;
} gdmf_vulkan_composition_mask;

// CPU snapshot only; referenced mask views/resources remain borrowed.
typedef struct {
    VkExtent2D extent;
    VkRect2D viewport;
    uint32_t mask_count;
    uint32_t capacity_fallbacks;
    gdmf_vulkan_composition_mask masks[GDMF_VULKAN_PRIORITY_COUNT];
} gdmf_vulkan_composition_plan;

typedef struct {
    VkCommandBuffer command_buffer;
    uint32_t image_index;
    VkRenderPass render_pass;
    VkFramebuffer framebuffer;
    VkClearColorValue clear_color;
} gdmf_vulkan_composition_frame;

typedef struct {
    uint32_t masks_recorded;
    uint32_t mask_fallbacks;
} gdmf_vulkan_composition_result;

// Render-thread preparation under the application-state lock. Snapshot mask
// selection, views, bounds, and inversion. Capacity is at most 256 (zero means
// unmasked fallback). False leaves output unchanged for invalid geometry/capacity.
// Keep mask views and all layer frame resources alive through GPU use.
bool gdmf_vulkan_composition_prepare(VkExtent2D extent, VkRect2D viewport,
    uint32_t mask_capacity, gdmf_vulkan_composition_plan* plan);

// Render thread, after image completion and layer preparation. Command buffer
// must already be recording outside a pass. Supply valid frame/plan/result.
// Optional mask setup/targets may be NULL; missing/failed masks render unmasked.
// Returns true if recording was issued, not GPU success. False changes neither
// output nor commands for invalid frame/plan geometry. Does not end the command
// buffer or submit. SEE: (GDMF_VULKAN.md, Viewport and composition recording).
bool gdmf_vulkan_composition_record(const gdmf_vulkan_composition_frame* frame,
    const gdmf_vulkan_composition_plan* plan, gdmf_vulkan_masks* masks,
    gdmf_vulkan_mask_targets* targets, gdmf_vulkan_composition_result* result);

#endif // GDMF_VULKAN_COMPOSITION_H
