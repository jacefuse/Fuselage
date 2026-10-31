// Recorded uploads through the shared frame arena. SEE: (GDMF_SPRITES.md, Vulkan atlas)
#include "gdmf_sprites_atlas.h"
#include "../../sprites/gdmf_sprites_state_private.h"

static void gdmf_sprites_atlas_initialize(gdmf_sprites_atlas* atlas, VkCommandBuffer command_buffer) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = atlas->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, MAX_SPRITE_BITMAPS},
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    atlas->layout_initialized = true;
}

static void gdmf_sprites_atlas_copy(gdmf_sprites_atlas* atlas, VkCommandBuffer command_buffer,
                                    VkBuffer buffer, VkDeviceSize offset, uint32_t bitmap_id) {
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
        .imageExtent = {SPRITE_WIDTH, SPRITE_HEIGHT, 1},
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

uint32_t gdmf_sprites_atlas_upload(gdmf_sprites_atlas* atlas, VkCommandBuffer command_buffer,
                                   gdmf_vulkan_staging* staging, uint32_t image_index) {
    if (!atlas || !atlas->image || !atlas->device || !command_buffer || !staging ||
        staging->device != atlas->device || image_index >= staging->image_count) {
        return 0;
    }
    if (!atlas->layout_initialized) {
        gdmf_sprites_atlas_initialize(atlas, command_buffer);
    }
    uint32_t uploaded = 0;
    for (int bitmap_id = 0; bitmap_id < MAX_SPRITE_BITMAPS; ++bitmap_id) {
        const unsigned char* packed = gdmf_sprites_bitmap_data((SpriteBitmapID)bitmap_id);
        if (!packed ||
            (atlas->layer_ready[bitmap_id] && !gdmf_sprites_bitmap_dirty((SpriteBitmapID)bitmap_id))) {
            continue;
        }
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        unsigned char* pixels =
            gdmf_frame_staging_alloc(staging, image_index, SPRITE_WIDTH * SPRITE_HEIGHT, &buffer, &offset);
        // Continue requesting failed slots so arena growth sees the full demand.
        if (!pixels || !buffer || offset % 4 != 0) {
            continue;
        }
        for (int pixel = 0; pixel < SPRITE_WIDTH * SPRITE_HEIGHT; ++pixel) {
            pixels[pixel] = (packed[pixel / 2] >> (pixel % 2 == 0 ? 4 : 0)) & 0x0F;
        }
        gdmf_sprites_atlas_copy(atlas, command_buffer, buffer, offset, (uint32_t)bitmap_id);
        atlas->layer_ready[bitmap_id] = true;
        gdmf_sprites_bitmap_mark_uploaded((SpriteBitmapID)bitmap_id);
        ++uploaded;
    }
    return uploaded;
}

bool gdmf_sprites_atlas_layer_ready(const gdmf_sprites_atlas* atlas, SpriteBitmapID bitmap_id) {
    return atlas && atlas->image && atlas->layout_initialized && gdmf_sprites_bitmap_id_valid(bitmap_id) &&
           atlas->layer_ready[bitmap_id] && gdmf_sprites_bitmap_data(bitmap_id) != NULL;
}
