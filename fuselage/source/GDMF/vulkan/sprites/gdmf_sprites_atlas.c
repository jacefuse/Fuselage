// Array-image ownership. SEE: (GDMF_SPRITES.md, Vulkan atlas)
#include "gdmf_sprites_atlas.h"
#include <string.h>

void gdmf_sprites_atlas_destroy(gdmf_sprites_atlas* atlas) {
    if (!atlas || !atlas->device) {
        return;
    }
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

bool gdmf_sprites_atlas_create(VkDevice device, VkPhysicalDevice physical_device,
                               const VkPhysicalDeviceMemoryProperties* memory_properties,
                               gdmf_sprites_atlas* atlas) {
    if (!device || !physical_device || !memory_properties || !atlas || atlas->device || atlas->image ||
        atlas->memory || atlas->view || atlas->sampler) {
        return false;
    }
    VkFormatProperties format;
    vkGetPhysicalDeviceFormatProperties(physical_device, VK_FORMAT_R8_UINT, &format);
    if (!(format.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        return false;
    }
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageFormatProperties limits;
    if (vkGetPhysicalDeviceImageFormatProperties(physical_device, VK_FORMAT_R8_UINT, VK_IMAGE_TYPE_2D,
                                                 VK_IMAGE_TILING_OPTIMAL, usage, 0, &limits) != VK_SUCCESS ||
        limits.maxExtent.width < SPRITE_WIDTH || limits.maxExtent.height < SPRITE_HEIGHT ||
        limits.maxExtent.depth < 1 || limits.maxArrayLayers < MAX_SPRITE_BITMAPS || limits.maxMipLevels < 1 ||
        !(limits.sampleCounts & VK_SAMPLE_COUNT_1_BIT)) {
        return false;
    }
    gdmf_sprites_atlas pending = {.device = device};
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UINT,
        .extent = {SPRITE_WIDTH, SPRITE_HEIGHT, 1},
        .mipLevels = 1,
        .arrayLayers = MAX_SPRITE_BITMAPS,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &image_info, NULL, &image) != VK_SUCCESS) {
        goto fail;
    }
    pending.image = image;
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image, &requirements);
    uint32_t memory_type = gdmf_find_memory_type(memory_properties, requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == UINT32_MAX) {
        goto fail;
    }
    VkMemoryAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_type,
    };
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &allocation, NULL, &memory) != VK_SUCCESS) {
        goto fail;
    }
    pending.memory = memory;
    if (vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS) {
        goto fail;
    }
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = VK_FORMAT_R8_UINT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, MAX_SPRITE_BITMAPS},
    };
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &view_info, NULL, &view) != VK_SUCCESS) {
        goto fail;
    }
    pending.view = view;
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 1.0f,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &sampler_info, NULL, &sampler) != VK_SUCCESS) {
        goto fail;
    }
    pending.sampler = sampler;
    *atlas = pending;
    return true;
fail:
    gdmf_sprites_atlas_destroy(&pending);
    return false;
}
