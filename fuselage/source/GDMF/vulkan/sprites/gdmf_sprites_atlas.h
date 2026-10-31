#ifndef GDMF_SPRITES_ATLAS_H
#define GDMF_SPRITES_ATLAS_H

#include "../../sprites/gdmf_sprites.h"
#include "../renderer/resources/gdmf_vulkan_memory.h"
#include "../renderer/resources/gdmf_vulkan_staging.h"

// Caller-owned atlas; device is borrowed. Ready/layout flags describe recorded
// work, not GPU completion. SEE: (GDMF_SPRITES.md, Vulkan atlas)
typedef struct {
    VkDevice device;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    bool layout_initialized;
    bool layer_ready[MAX_SPRITE_BITMAPS];
} gdmf_sprites_atlas;

// Requires a zeroed output and matching physical-device memory properties.
// Checks format/array limits and binding. Output changes only on success.
// No commands, submissions, waits or global renderer access.
bool gdmf_sprites_atlas_create(VkDevice device, VkPhysicalDevice physical_device,
                               const VkPhysicalDeviceMemoryProperties* memory_properties,
                               gdmf_sprites_atlas* atlas);

// Record outside a render pass, after this frame index's staging reset/completion.
// Caller serializes host bitmap access. Returns the number of bitmap copies
// recorded; pending/uninitialized slots retry when staging becomes available.
// A newly created atlas refills from registered host slots even if not dirty.
uint32_t gdmf_sprites_atlas_upload(gdmf_sprites_atlas* atlas, VkCommandBuffer command_buffer,
                                   gdmf_vulkan_staging* staging, uint32_t image_index);
bool gdmf_sprites_atlas_layer_ready(const gdmf_sprites_atlas* atlas, SpriteBitmapID bitmap_id);

// Requires completed GPU work, discarded recorded references, no live descriptor
// users and excluded producers. Does not wait or retire, and preserves host data.
void gdmf_sprites_atlas_destroy(gdmf_sprites_atlas* atlas);

#endif
