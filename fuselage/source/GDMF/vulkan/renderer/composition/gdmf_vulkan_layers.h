#ifndef GDMF_VULKAN_LAYERS_H
#define GDMF_VULKAN_LAYERS_H

// Internal Vulkan layer hooks. These types are backend-specific, not a common
// renderer contract for future backends.
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

// Prepare on the render thread under the drawing-state lock, outside a render
// pass. image_index selects buffers whose previous GPU work has completed.
void gdmf_textlayer_prepare(uint32_t image_index);
void gdmf_sprites_prepare(VkCommandBuffer command_buffer, uint32_t image_index);
void gdmf_tiles_prepare(VkCommandBuffer command_buffer, uint32_t image_index);
void gdmf_pixies_prepare(VkCommandBuffer command_buffer, uint32_t image_index);

// Record inside a render pass. The compositor walks priorities 255 down to 0,
// drawing pixies, tiles, then sprites at each priority. Text has its own hook.
void gdmf_textlayer_record(VkCommandBuffer command_buffer, uint32_t image_index);
void gdmf_sprites_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority);
void gdmf_tiles_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority);
void gdmf_pixies_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority);

// Lowest-numbered shown, ready mask pixie at this priority, or -1 if none.
int gdmf_pixies_priority_mask_id(uint8_t priority);

// Supplies a sampled view, normalized-device-coordinate rectangle
// {min_x, min_y, max_x, max_y}, and inversion flag. False leaves outputs alone
// when pixie_id does not identify a ready mask. Any output pointer may be NULL.
bool gdmf_pixies_get_mask_info(int pixie_id, VkImageView* image_view,
                              float normalized_rect[4], bool* inverted);

// Invalidate render-pass and image-slot resources at the coordinator boundary.
// Layer prepare hooks rebuild them as needed.
// Text, Sprite and Tile cleanup runs after quiescence, BEFORE replacing borrowed
// palette buffers or render pass. Atlases survive this boundary.
void gdmf_textlayer_on_swapchain_recreated(void);
void gdmf_sprites_on_swapchain_recreated(void);
void gdmf_tiles_on_swapchain_recreated(void);
void gdmf_pixies_on_swapchain_recreated(void);

#endif // GDMF_VULKAN_LAYERS_H
