// Caller-owned samplers, descriptors and images. SEE: (GDMF_PIXIES.md, Vulkan images)
#include "gdmf_pixies_images.h"
#include "../../pixies/gdmf_pixies.h"
#include <string.h>

void gdmf_pixies_image_context_destroy(gdmf_pixies_image_context* context) {
    if (!context || !context->device)
        return;
    if (context->descriptor_pool)
        vkDestroyDescriptorPool(context->device, context->descriptor_pool, NULL);
    if (context->descriptor_layout)
        vkDestroyDescriptorSetLayout(context->device, context->descriptor_layout, NULL);
    if (context->linear_sampler)
        vkDestroySampler(context->device, context->linear_sampler, NULL);
    if (context->nearest_sampler)
        vkDestroySampler(context->device, context->nearest_sampler, NULL);
    memset(context, 0, sizeof *context);
}

bool gdmf_pixies_image_context_create(VkDevice device, VkPhysicalDevice physical_device,
                                      const VkPhysicalDeviceMemoryProperties* memory_properties,
                                      gdmf_pixies_image_context* context) {
    if (!device || !physical_device || !memory_properties || !context)
        return false;
    gdmf_pixies_image_context pending = {
        .device = device, .physical_device = physical_device, .memory_properties = *memory_properties};
    VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .magFilter = VK_FILTER_NEAREST,
                                   .minFilter = VK_FILTER_NEAREST,
                                   .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                   .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   .maxAnisotropy = 1.0f};
    VkSampler nearest = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &sampler, NULL, &nearest) != VK_SUCCESS)
        goto fail;
    pending.nearest_sampler = nearest;
    sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSampler linear = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &sampler, NULL, &linear) == VK_SUCCESS)
        pending.linear_sampler = linear;
    VkDescriptorSetLayoutBinding binding = {.binding = 0,
                                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            .descriptorCount = 1,
                                            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo layout = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                              .bindingCount = 1,
                                              .pBindings = &binding};
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &layout, NULL, &descriptor_layout) != VK_SUCCESS)
        goto fail;
    pending.descriptor_layout = descriptor_layout;
    VkDescriptorPoolSize size = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 .descriptorCount = MAX_PIXIES * 2};
    VkDescriptorPoolCreateInfo pool = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                       .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                       .maxSets = MAX_PIXIES * 2,
                                       .poolSizeCount = 1,
                                       .pPoolSizes = &size};
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &pool, NULL, &descriptor_pool) != VK_SUCCESS)
        goto fail;
    pending.descriptor_pool = descriptor_pool;
    *context = pending;
    return true;
fail:
    gdmf_pixies_image_context_destroy(&pending);
    return false;
}

static bool gdmf_pixies_image_descriptors(const gdmf_pixies_image_context* context, bool linear_supported,
                                          gdmf_pixies_image* image) {
    VkDescriptorSetAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = context->descriptor_pool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = &context->descriptor_layout};
    if (vkAllocateDescriptorSets(context->device, &allocation, &image->nearest_set) != VK_SUCCESS)
        return false;
    VkDescriptorImageInfo info = {.sampler = context->nearest_sampler,
                                  .imageView = image->view,
                                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                  .dstSet = image->nearest_set,
                                  .dstBinding = 0,
                                  .descriptorCount = 1,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .pImageInfo = &info};
    vkUpdateDescriptorSets(context->device, 1, &write, 0, NULL);
    if (linear_supported && context->linear_sampler) {
        VkDescriptorSet linear = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(context->device, &allocation, &linear) == VK_SUCCESS) {
            image->linear_set = linear;
            info.sampler = context->linear_sampler;
            write.dstSet = linear;
            vkUpdateDescriptorSets(context->device, 1, &write, 0, NULL);
        }
    }
    return true;
}

bool gdmf_pixies_image_create(const gdmf_pixies_image_context* context, uint32_t width, uint32_t height,
                              bool mask, gdmf_pixies_image* image) {
    if (!context || !context->device || !context->physical_device || !context->descriptor_pool ||
        !context->descriptor_layout || !context->nearest_sampler || !width || !height || !image)
        return false;
    VkFormat format = mask ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!mask)
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageFormatProperties limits;
    if (vkGetPhysicalDeviceImageFormatProperties(context->physical_device, format, VK_IMAGE_TYPE_2D,
                                                 VK_IMAGE_TILING_OPTIMAL, usage, 0, &limits) != VK_SUCCESS ||
        width > limits.maxExtent.width || height > limits.maxExtent.height ||
        !(limits.sampleCounts & VK_SAMPLE_COUNT_1_BIT))
        return false;
    gdmf_pixies_image pending = {.width = width, .height = height, .mask = mask};
    VkImageCreateInfo creation = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                  .imageType = VK_IMAGE_TYPE_2D,
                                  .format = format,
                                  .extent = {width, height, 1},
                                  .mipLevels = 1,
                                  .arrayLayers = 1,
                                  .samples = VK_SAMPLE_COUNT_1_BIT,
                                  .tiling = VK_IMAGE_TILING_OPTIMAL,
                                  .usage = usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    if (vkCreateImage(context->device, &creation, NULL, &pending.image) != VK_SUCCESS)
        return false;
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(context->device, pending.image, &requirements);
    uint32_t memory_type = gdmf_find_memory_type(&context->memory_properties, requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == UINT32_MAX)
        goto fail;
    VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                       .allocationSize = requirements.size,
                                       .memoryTypeIndex = memory_type};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(context->device, &allocation, NULL, &memory) != VK_SUCCESS)
        goto fail;
    pending.memory = memory;
    if (vkBindImageMemory(context->device, pending.image, pending.memory, 0) != VK_SUCCESS)
        goto fail;
    VkImageViewCreateInfo view = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                  .image = pending.image,
                                  .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                  .format = format,
                                  .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkImageView image_view = VK_NULL_HANDLE;
    if (vkCreateImageView(context->device, &view, NULL, &image_view) != VK_SUCCESS)
        goto fail;
    pending.view = image_view;
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(context->physical_device, format, &properties);
    if (!gdmf_pixies_image_descriptors(
            context,
            (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0,
            &pending))
        goto fail;
    *image = pending;
    return true;
fail:
    // No descriptor was adopted on these failure paths; nothing was recorded.
    if (pending.view)
        vkDestroyImageView(context->device, pending.view, NULL);
    if (pending.image)
        vkDestroyImage(context->device, pending.image, NULL);
    if (pending.memory)
        vkFreeMemory(context->device, pending.memory, NULL);
    return false;
}
