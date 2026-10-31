// Lazily allocated color targets for per-priority mask composition.
// SEE: (GDMF_VULKAN.md, Mask offscreen targets).
#include "gdmf_vulkan_mask_targets.h"
#include "../resources/gdmf_vulkan_memory.h"
#include <stdio.h>
#include <stdlib.h>

static void gdmf_vulkan_mask_target_release(VkDevice device, gdmf_vulkan_mask_target* target) {
    if (target->framebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(device, target->framebuffer, NULL); }
    if (target->image_view != VK_NULL_HANDLE) { vkDestroyImageView(device, target->image_view, NULL); }
    if (target->image != VK_NULL_HANDLE) { vkDestroyImage(device, target->image, NULL); }
    if (target->memory != VK_NULL_HANDLE) { vkFreeMemory(device, target->memory, NULL); }
    *target = (gdmf_vulkan_mask_target){0};
}

void gdmf_vulkan_mask_targets_destroy(gdmf_vulkan_mask_targets* targets) {
    if (targets->targets) {
        for (uint32_t target_index = 0; target_index < targets->target_count; target_index++) {
            gdmf_vulkan_mask_target_release(targets->config.device, &targets->targets[target_index]);
        }
        free(targets->targets);
    }
    *targets = (gdmf_vulkan_mask_targets){0};
}

int gdmf_vulkan_mask_targets_create(const gdmf_vulkan_mask_target_config* config,
    gdmf_vulkan_mask_targets* targets) {
    if (config->device == VK_NULL_HANDLE || config->render_pass == VK_NULL_HANDLE ||
        config->format == VK_FORMAT_UNDEFINED || !config->extent.width || !config->extent.height ||
        !config->image_count || !config->masks_per_image ||
        config->image_count > UINT32_MAX / config->masks_per_image) { return -1; }
    uint32_t target_count = config->image_count * config->masks_per_image;
    gdmf_vulkan_mask_target* storage = calloc(target_count, sizeof(gdmf_vulkan_mask_target));
    if (!storage) { return -1; }
    *targets = (gdmf_vulkan_mask_targets){.config = *config, .target_count = target_count, .targets = storage};
    return 0;
}

const gdmf_vulkan_mask_target* gdmf_vulkan_mask_targets_ensure(gdmf_vulkan_mask_targets* targets,
    uint32_t image_index, uint32_t mask_slot) {
    if (!targets->targets || image_index >= targets->config.image_count ||
        mask_slot >= targets->config.masks_per_image) { return NULL; }
    uint32_t target_index = image_index * targets->config.masks_per_image + mask_slot;
    gdmf_vulkan_mask_target* target = &targets->targets[target_index];
    if (target->framebuffer != VK_NULL_HANDLE) { return target; }
    const gdmf_vulkan_mask_target_config* config = &targets->config;
    gdmf_vulkan_mask_target created_target = {0};
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D, .format = config->format,
        .extent = {config->extent.width, config->extent.height, 1},
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkImage image = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(config->device, &image_info, NULL, &image);
    if (result != VK_SUCCESS) { goto fail; }
    created_target.image = image;
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(config->device, image, &requirements);
    uint32_t memory_type = gdmf_find_memory_type(&config->memory_properties,
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == UINT32_MAX) { result = VK_ERROR_OUT_OF_DEVICE_MEMORY; goto fail; }
    VkMemoryAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size, .memoryTypeIndex = memory_type
    };
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(config->device, &allocation_info, NULL, &memory);
    if (result != VK_SUCCESS) { goto fail; }
    created_target.memory = memory;
    result = vkBindImageMemory(config->device, image, memory, 0);
    if (result != VK_SUCCESS) { goto fail; }

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = config->format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageView image_view = VK_NULL_HANDLE;
    result = vkCreateImageView(config->device, &view_info, NULL, &image_view);
    if (result != VK_SUCCESS) { goto fail; }
    created_target.image_view = image_view;
    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = config->render_pass, .attachmentCount = 1, .pAttachments = &image_view,
        .width = config->extent.width, .height = config->extent.height, .layers = 1
    };
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    result = vkCreateFramebuffer(config->device, &framebuffer_info, NULL, &framebuffer);
    if (result != VK_SUCCESS) { goto fail; }
    created_target.framebuffer = framebuffer;
    *target = created_target;
    return target;

fail:
    gdmf_vulkan_mask_target_release(config->device, &created_target);
    printf("[Masks] Offscreen target creation failed (image %u, slot %u): %d\n", image_index, mask_slot, result);
    return NULL;
}
