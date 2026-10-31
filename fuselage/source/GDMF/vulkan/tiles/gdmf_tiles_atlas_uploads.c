// Recorded uploads through the shared frame arena. SEE: (GDMF_TILES.md, Vulkan atlas and uploads)
#include "gdmf_tiles_atlas.h"
#include <string.h>

static void gdmf_tiles_atlas_initialize(gdmf_tiles_atlas* atlas, VkCommandBuffer command_buffer) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = atlas->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, atlas->tile_count},
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    atlas->layout_initialized = true;
}

static void gdmf_tiles_atlas_copy(gdmf_tiles_atlas* atlas, VkCommandBuffer command_buffer, VkBuffer buffer,
                                  VkDeviceSize offset, uint32_t bitmap_id) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = atlas->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, bitmap_id, 1},
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    VkBufferImageCopy region = {
        .bufferOffset = offset,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, bitmap_id, 1},
        .imageExtent = {atlas->width, atlas->height, 1},
    };
    vkCmdCopyBufferToImage(command_buffer, buffer, atlas->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}

uint32_t gdmf_tiles_atlas_upload(gdmf_tiles_atlas* atlas, VkCommandBuffer command_buffer,
                                 gdmf_vulkan_staging* staging, uint32_t image_index) {
    if (!atlas || !atlas->image || !atlas->device || !command_buffer || !staging ||
        staging->device != atlas->device || image_index >= staging->image_count) {
        return 0;
    }
    const TileMap* map = gdmf_tiles_state_get(atlas->layer);
    if (!map || !map->initialized || map->tileWidth != atlas->width || map->tileHeight != atlas->height ||
        gdmf_tiles_bitmap_count(atlas->layer) != atlas->tile_count) {
        return 0;
    }
    if (!atlas->layout_initialized) {
        gdmf_tiles_atlas_initialize(atlas, command_buffer);
    }
    uint32_t uploaded = 0;
    bool geometry_changed = false;
    for (int bitmap_id = 0; bitmap_id < atlas->tile_count; ++bitmap_id) {
        const unsigned char* pixels = gdmf_tiles_bitmap_data(atlas->layer, bitmap_id);
        if (!pixels || (atlas->bitmap_ready[bitmap_id] && !gdmf_tiles_bitmap_dirty(atlas->layer, bitmap_id))) {
            continue;
        }
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        unsigned char* destination =
            gdmf_frame_staging_alloc(staging, image_index, atlas->width * atlas->height, &buffer, &offset);
        // Continue requesting failed slots so arena growth sees the full demand.
        if (!destination || !buffer || offset % 4 != 0) {
            continue;
        }
        memcpy(destination, pixels, (size_t)atlas->width * atlas->height);
        bool newly_ready = !atlas->bitmap_ready[bitmap_id];
        gdmf_tiles_atlas_copy(atlas, command_buffer, buffer, offset, (uint32_t)bitmap_id);
        atlas->bitmap_ready[bitmap_id] = true;
        gdmf_tiles_bitmap_mark_uploaded(atlas->layer, bitmap_id);
        geometry_changed = geometry_changed || newly_ready;
        ++uploaded;
    }
    if (geometry_changed) {
        gdmf_tiles_backend_mark_dirty(atlas->layer);
    }
    return uploaded;
}

bool gdmf_tiles_atlas_bitmap_ready(const gdmf_tiles_atlas* atlas, int tile_id) {
    if (!atlas || !atlas->image || !atlas->layout_initialized || tile_id < 0 ||
        tile_id >= atlas->tile_count || !atlas->bitmap_ready[tile_id]) {
        return false;
    }
    const TileMap* map = gdmf_tiles_state_get(atlas->layer);
    return map && map->tileWidth == atlas->width && map->tileHeight == atlas->height &&
           gdmf_tiles_bitmap_count(atlas->layer) == atlas->tile_count &&
           gdmf_tiles_bitmap_data(atlas->layer, tile_id) != NULL;
}
