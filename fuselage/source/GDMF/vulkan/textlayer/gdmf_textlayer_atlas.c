// Font image and upload ownership. SEE: (GDMF_TEXT.md, Vulkan atlas)
#include "gdmf_textlayer_atlas.h"
#include <string.h>

static void gdmf_textlayer_atlas_release_staging(gdmf_textlayer_atlas* atlas) {
    if (atlas->staging_buffer) {
        vkDestroyBuffer(atlas->device, atlas->staging_buffer, NULL);
    }
    if (atlas->staging_memory) {
        vkFreeMemory(atlas->device, atlas->staging_memory, NULL);
    }
    atlas->staging_buffer = VK_NULL_HANDLE;
    atlas->staging_memory = VK_NULL_HANDLE;
}
static void gdmf_textlayer_atlas_release(gdmf_textlayer_atlas* atlas) {
    gdmf_textlayer_atlas_release_staging(atlas);
    if (atlas->sampler) {
        vkDestroySampler(atlas->device, atlas->sampler, NULL);
    }
    if (atlas->view) {
        vkDestroyImageView(atlas->device, atlas->view, NULL);
    }
    if (atlas->image) {
        vkDestroyImage(atlas->device, atlas->image, NULL);
    }
    if (atlas->memory) {
        vkFreeMemory(atlas->device, atlas->memory, NULL);
    }
    memset(atlas, 0, sizeof(*atlas));
}
bool gdmf_textlayer_atlas_destroy(gdmf_textlayer_atlas* atlas, bool quiesced) {
    if (!atlas) {
        return false;
    }
    if (!atlas->device) {
        return !atlas->image && !atlas->memory && !atlas->view && !atlas->sampler && !atlas->staging_buffer &&
               !atlas->staging_memory && !atlas->unresolved_uploads && !atlas->ready;
    }
    if (!quiesced || (atlas->unresolved_uploads && (atlas->unresolved_uploads->retained_command_buffer ||
                                                    atlas->unresolved_uploads->retained_fence))) {
        return false;
    }
    gdmf_textlayer_atlas_release(atlas);
    return true;
}
static void gdmf_textlayer_atlas_record(VkCommandBuffer command_buffer, void* context) {
    const gdmf_textlayer_atlas* atlas = context;
    VkImageMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .image = atlas->image,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT};
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &barrier);
    VkBufferImageCopy copy = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                              .imageExtent = {GDMF_TEXT_ATLAS_WIDTH, GDMF_TEXT_ATLAS_HEIGHT, 1}};
    vkCmdCopyBufferToImage(command_buffer, atlas->staging_buffer, atlas->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}
static bool gdmf_textlayer_atlas_allocate(VkDevice device, const VkPhysicalDeviceMemoryProperties* properties,
                                          const VkMemoryRequirements* requirements,
                                          VkMemoryPropertyFlags flags, VkDeviceMemory* output) {
    uint32_t memory_type = gdmf_find_memory_type(properties, requirements->memoryTypeBits, flags);
    if (memory_type == UINT32_MAX) {
        return false;
    }
    VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                       .allocationSize = requirements->size,
                                       .memoryTypeIndex = memory_type};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocation, NULL, &memory) != VK_SUCCESS) {
        return false;
    }
    *output = memory;
    return true;
}
gdmf_vulkan_upload_result gdmf_textlayer_atlas_create(const gdmf_textlayer_atlas_config* config,
                                                      gdmf_textlayer_atlas* output) {
    if (!config || !config->device || !config->physical_device || !config->memory_properties ||
        !config->uploads || config->uploads->device != config->device || !config->uploads->graphics_queue ||
        !config->uploads->upload_pool || !config->uploads->upload_lock || !config->uploads->queue_lock ||
        !output || output->device || output->image || output->memory || output->view || output->sampler ||
        output->staging_buffer || output->staging_memory || output->unresolved_uploads || output->ready) {
        return GDMF_VULKAN_UPLOAD_FAILED;
    }
    fuselage_mutex_lock(config->uploads->upload_lock);
    bool blocked = config->uploads->retained_command_buffer || config->uploads->retained_fence;
    fuselage_mutex_unlock(config->uploads->upload_lock);
    if (blocked) {
        return GDMF_VULKAN_UPLOAD_UNRESOLVED;
    }
    VkFormatProperties format;
    vkGetPhysicalDeviceFormatProperties(config->physical_device, VK_FORMAT_R8G8B8A8_UNORM, &format);
    VkImageFormatProperties limits;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!(format.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ||
        vkGetPhysicalDeviceImageFormatProperties(config->physical_device, VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0,
                                                 &limits) != VK_SUCCESS ||
        limits.maxExtent.width < GDMF_TEXT_ATLAS_WIDTH || limits.maxExtent.height < GDMF_TEXT_ATLAS_HEIGHT ||
        limits.maxExtent.depth < 1 || limits.maxArrayLayers < 1 || limits.maxMipLevels < 1 ||
        !(limits.sampleCounts & VK_SAMPLE_COUNT_1_BIT)) {
        return GDMF_VULKAN_UPLOAD_FAILED;
    }
    gdmf_textlayer_atlas pending = {.device = config->device};
    VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                    .imageType = VK_IMAGE_TYPE_2D,
                                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                                    .extent = {GDMF_TEXT_ATLAS_WIDTH, GDMF_TEXT_ATLAS_HEIGHT, 1},
                                    .mipLevels = 1,
                                    .arrayLayers = 1,
                                    .samples = VK_SAMPLE_COUNT_1_BIT,
                                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(config->device, &image_info, NULL, &image) != VK_SUCCESS) {
        goto fail;
    }
    pending.image = image;
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(config->device, image, &requirements);
    if (!gdmf_textlayer_atlas_allocate(config->device, config->memory_properties, &requirements,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pending.memory) ||
        vkBindImageMemory(config->device, image, pending.memory, 0) != VK_SUCCESS) {
        goto fail;
    }
    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = GDMF_TEXT_ATLAS_BYTES,
                                      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(config->device, &buffer_info, NULL, &buffer) != VK_SUCCESS) {
        goto fail;
    }
    pending.staging_buffer = buffer;
    vkGetBufferMemoryRequirements(config->device, buffer, &requirements);
    if (!gdmf_textlayer_atlas_allocate(config->device, config->memory_properties, &requirements,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &pending.staging_memory) ||
        vkBindBufferMemory(config->device, buffer, pending.staging_memory, 0) != VK_SUCCESS) {
        goto fail;
    }
    void* mapped = NULL;
    if (vkMapMemory(config->device, pending.staging_memory, 0, GDMF_TEXT_ATLAS_BYTES, 0, &mapped) !=
        VK_SUCCESS) {
        goto fail;
    }
    bool decoded = gdmf_textlayer_font_decode(mapped, GDMF_TEXT_ATLAS_BYTES);
    vkUnmapMemory(config->device, pending.staging_memory);
    if (!decoded) {
        goto fail;
    }
    VkImageViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .image = image,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = VK_FORMAT_R8G8B8A8_UNORM,
                                       .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(config->device, &view_info, NULL, &view) != VK_SUCCESS) {
        goto fail;
    }
    pending.view = view;
    VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                        .magFilter = VK_FILTER_NEAREST,
                                        .minFilter = VK_FILTER_NEAREST,
                                        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                        .maxAnisotropy = 1.0f};
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(config->device, &sampler_info, NULL, &sampler) != VK_SUCCESS) {
        goto fail;
    }
    pending.sampler = sampler;
    gdmf_vulkan_upload_result result =
        gdmf_execute_one_time_commands(config->uploads, gdmf_textlayer_atlas_record, &pending);
    if (result == GDMF_VULKAN_UPLOAD_FAILED) {
        goto fail;
    }
    if (result == GDMF_VULKAN_UPLOAD_UNRESOLVED) {
        pending.unresolved_uploads = config->uploads;
        *output = pending;
        return result;
    }
    gdmf_textlayer_atlas_release_staging(&pending);
    pending.ready = true;
    *output = pending;
    return GDMF_VULKAN_UPLOAD_COMPLETE;
fail:
    gdmf_textlayer_atlas_release(&pending);
    return GDMF_VULKAN_UPLOAD_FAILED;
}
