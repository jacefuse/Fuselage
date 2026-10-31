#ifndef GDMF_TILES_ATLAS_H
#define GDMF_TILES_ATLAS_H

#include "../../tiles/gdmf_tiles_bitmaps.h"
#include "../renderer/resources/gdmf_vulkan_memory.h"
#include "../renderer/resources/gdmf_vulkan_staging.h"

typedef struct {
    VkDevice device;
    VkPhysicalDevice physical_device;
    const VkPhysicalDeviceMemoryProperties* memory_properties;
    uint8_t layer;
    uint16_t width, height, tile_count;
} gdmf_tiles_atlas_config;

// Caller-owned resources, borrowed device. Flags describe recorded commands,
// not completion. SEE: (GDMF_TILES.md, Vulkan atlas and uploads)
typedef struct {
    VkDevice device;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    uint8_t layer;
    uint16_t width, height, tile_count;
    bool layout_initialized;
    bool bitmap_ready[MAX_TILES];
} gdmf_tiles_atlas;

// Zeroed output; publish only on success. No submission, wait or renderer globals.
bool gdmf_tiles_atlas_create(const gdmf_tiles_atlas_config* config, gdmf_tiles_atlas* atlas);
// Outside the render pass, after this image's prior completion/staging reset.
// Record on the graphics queue ordered with prior atlas use; no queue transfer.
// Host state remains stable. Returns recorded bitmap-copy count, not completion.
// Failed staging stays pending; a fresh atlas refills even previously clean slots.
uint32_t gdmf_tiles_atlas_upload(gdmf_tiles_atlas* atlas, VkCommandBuffer command_buffer,
                                 gdmf_vulkan_staging* staging, uint32_t image_index);
bool gdmf_tiles_atlas_bitmap_ready(const gdmf_tiles_atlas* atlas, int tile_id);
// Completed GPU use, discarded recorded references, released descriptor users,
// excluded producers. Does not wait/retire or erase host bitmap state.
void gdmf_tiles_atlas_destroy(gdmf_tiles_atlas* atlas);

#endif
