// Persistent per-image upload slots. SEE: (GDMF_PIXIES.md, Vulkan images)
#include "gdmf_pixies_images.h"
#include <stdlib.h>
#include <string.h>

static void gdmf_pixies_staging_destroy(VkDevice device, gdmf_pixies_image_staging* staging) {
    if (staging->mapped && staging->memory)
        vkUnmapMemory(device, staging->memory);
    if (staging->buffer)
        vkDestroyBuffer(device, staging->buffer, NULL);
    if (staging->memory)
        vkFreeMemory(device, staging->memory, NULL);
    memset(staging, 0, sizeof *staging);
}

void gdmf_pixies_image_staging_reset(const gdmf_pixies_image_context* context, gdmf_pixies_image* image) {
    if (!context || !context->device || !image)
        return;
    for (uint32_t slot = 0; slot < image->staging_count; slot++)
        gdmf_pixies_staging_destroy(context->device, &image->staging[slot]);
    free(image->staging);
    image->staging = NULL;
    image->staging_count = 0;
}

bool gdmf_pixies_image_destroy(const gdmf_pixies_image_context* context, gdmf_pixies_image* image) {
    if (!context || !context->device || !image)
        return false;
    VkDescriptorSet sets[2];
    uint32_t count = 0;
    if (image->nearest_set)
        sets[count++] = image->nearest_set;
    if (image->linear_set)
        sets[count++] = image->linear_set;
    if (count && !context->descriptor_pool)
        return false;
    if (count && vkFreeDescriptorSets(context->device, context->descriptor_pool, count, sets) != VK_SUCCESS)
        return false;
    gdmf_pixies_image_staging_reset(context, image);
    if (image->view)
        vkDestroyImageView(context->device, image->view, NULL);
    if (image->image)
        vkDestroyImage(context->device, image->image, NULL);
    if (image->memory)
        vkFreeMemory(context->device, image->memory, NULL);
    memset(image, 0, sizeof *image);
    return true;
}

static bool gdmf_pixies_staging_create(const gdmf_pixies_image_context* context, VkDeviceSize bytes,
                                       gdmf_pixies_image_staging* staging) {
    gdmf_pixies_image_staging pending = {0};
    VkBufferCreateInfo creation = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = bytes,
                                   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    if (vkCreateBuffer(context->device, &creation, NULL, &pending.buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(context->device, pending.buffer, &requirements);
    VkMemoryAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = gdmf_find_memory_type(&context->memory_properties, requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (allocation.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(context->device, &allocation, NULL, &memory) != VK_SUCCESS)
        goto fail;
    pending.memory = memory;
    if (vkBindBufferMemory(context->device, pending.buffer, pending.memory, 0) != VK_SUCCESS)
        goto fail;
    void* mapped = NULL;
    if (vkMapMemory(context->device, pending.memory, 0, bytes, 0, &mapped) != VK_SUCCESS)
        goto fail;
    pending.mapped = mapped;
    *staging = pending;
    return true;
fail:
    gdmf_pixies_staging_destroy(context->device, &pending);
    return false;
}

bool gdmf_pixies_image_upload(const gdmf_pixies_image_context* context, gdmf_pixies_image* image,
                              VkCommandBuffer command_buffer, uint32_t image_index, uint32_t image_count,
                              const void* pixels, size_t byte_count) {
    if (!context || !context->device || !image || !image->image || !command_buffer || !pixels ||
        !image_count || image_index >= image_count || !image->width || !image->height)
        return false;
    size_t bytes_per_pixel = image->mask ? 1u : 4u;
    if ((size_t)image->width > SIZE_MAX / image->height / bytes_per_pixel)
        return false;
    size_t bytes = (size_t)image->width * image->height * bytes_per_pixel;
    if (byte_count != bytes || (image->staging_count && image->staging_count != image_count))
        return false;
    if (!image->staging_count) {
        if ((size_t)image_count > SIZE_MAX / sizeof *image->staging)
            return false;
        image->staging = calloc(image_count, sizeof *image->staging);
        if (!image->staging)
            return false;
        image->staging_count = image_count;
    }
    gdmf_pixies_image_staging* staging = &image->staging[image_index];
    if (!staging->buffer && !gdmf_pixies_staging_create(context, bytes, staging))
        return false;
    memcpy(staging->mapped, pixels, bytes);
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout =
            image->layout_initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .srcAccessMask = image->layout_initialized ? VK_ACCESS_SHADER_READ_BIT : 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT};
    VkPipelineStageFlags previous = image->layout_initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                              : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vkCmdPipelineBarrier(command_buffer, previous, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &barrier);
    VkBufferImageCopy region = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                .imageExtent = {image->width, image->height, 1}};
    vkCmdCopyBufferToImage(command_buffer, staging->buffer, image->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                         NULL, 0, NULL, 1, &barrier);
    image->layout_initialized = true;
    return true;
}
