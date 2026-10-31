// Swapchain images and views for an existing device and presentation surface.
// SEE: (GDMF_VULKAN.md, Swapchain creation).
#include "gdmf_vulkan_swapchain.h"
#include <stdio.h>
#include <stdlib.h>

void gdmf_vulkan_swapchain_destroy(VkDevice device, gdmf_vulkan_swapchain* swapchain) {
    if (swapchain->image_views) {
        for (uint32_t image_index = 0; image_index < swapchain->image_count; image_index++) {
            if (swapchain->image_views[image_index] != VK_NULL_HANDLE) {
                vkDestroyImageView(device, swapchain->image_views[image_index], NULL);
            }
        }
        free(swapchain->image_views);
    }
    free(swapchain->images);
    if (swapchain->handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain->handle, NULL);
    }
    *swapchain = (gdmf_vulkan_swapchain){0};
}

int gdmf_vulkan_swapchain_create(const gdmf_vulkan_physical_device_selection* selection,
    VkDevice device, VkSurfaceKHR surface, VkExtent2D window_extent,
    bool vsync_enabled, gdmf_vulkan_swapchain* swapchain) {
    gdmf_vulkan_swapchain created_swapchain = {0};
    VkSurfaceFormatKHR* formats = NULL;
    VkPresentModeKHR* available_modes = NULL;
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selection->device,
        surface, &capabilities);
    if (result != VK_SUCCESS) { goto fail; }

    uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(selection->device, surface, &format_count, NULL);
    if (result != VK_SUCCESS) { goto fail; }
    if (format_count == 0) { result = VK_ERROR_INITIALIZATION_FAILED; goto fail; }
    formats = malloc(format_count * sizeof(VkSurfaceFormatKHR));
    if (!formats) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto fail; }
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(selection->device, surface, &format_count, formats);
    if (result != VK_SUCCESS) { goto fail; }
    if (format_count == 0) { result = VK_ERROR_INITIALIZATION_FAILED; goto fail; }

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (uint32_t format_index = 0; format_index < format_count; format_index++) {
        if (formats[format_index].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[format_index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = formats[format_index];
            break;
        }
    }
    free(formats);
    formats = NULL;

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync_enabled) {
        uint32_t present_mode_count = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(selection->device,
            surface, &present_mode_count, NULL);
        if (result != VK_SUCCESS) { goto fail; }
        available_modes = malloc(present_mode_count * sizeof(VkPresentModeKHR));
        if (available_modes) {
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(selection->device,
                surface, &present_mode_count, available_modes);
            if (result != VK_SUCCESS) { goto fail; }
            bool immediate_available = false, mailbox_available = false;
            for (uint32_t mode_index = 0; mode_index < present_mode_count; mode_index++) {
                if (available_modes[mode_index] == VK_PRESENT_MODE_IMMEDIATE_KHR) { immediate_available = true; }
                if (available_modes[mode_index] == VK_PRESENT_MODE_MAILBOX_KHR) { mailbox_available = true; }
            }
            if (immediate_available) { present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
            else if (mailbox_available) { present_mode = VK_PRESENT_MODE_MAILBOX_KHR; }
            free(available_modes);
            available_modes = NULL;
        }
    }

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent = window_extent;
        if (extent.width < capabilities.minImageExtent.width) { extent.width = capabilities.minImageExtent.width; }
        if (extent.width > capabilities.maxImageExtent.width) { extent.width = capabilities.maxImageExtent.width; }
        if (extent.height < capabilities.minImageExtent.height) { extent.height = capabilities.minImageExtent.height; }
        if (extent.height > capabilities.maxImageExtent.height) { extent.height = capabilities.maxImageExtent.height; }
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }
    uint32_t family_indices[] = {selection->graphics_family, selection->present_family};
    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = chosen_format.format,
        .imageColorSpace = chosen_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    if (selection->graphics_family != selection->present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    result = vkCreateSwapchainKHR(device, &create_info, NULL, &created_swapchain.handle);
    if (result != VK_SUCCESS) {
        created_swapchain.handle = VK_NULL_HANDLE;
        goto fail;
    }
    created_swapchain.format = chosen_format.format;
    created_swapchain.color_space = chosen_format.colorSpace;
    created_swapchain.extent = extent;
    created_swapchain.present_mode = present_mode;
    created_swapchain.image_usage = create_info.imageUsage;

    result = vkGetSwapchainImagesKHR(device, created_swapchain.handle, &created_swapchain.image_count, NULL);
    if (result != VK_SUCCESS) { goto fail; }
    if (created_swapchain.image_count == 0) { result = VK_ERROR_INITIALIZATION_FAILED; goto fail; }
    created_swapchain.images = malloc(created_swapchain.image_count * sizeof(VkImage));
    if (!created_swapchain.images) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto fail; }
    result = vkGetSwapchainImagesKHR(device, created_swapchain.handle,
        &created_swapchain.image_count, created_swapchain.images);
    if (result != VK_SUCCESS) { goto fail; }
    if (created_swapchain.image_count == 0) { result = VK_ERROR_INITIALIZATION_FAILED; goto fail; }
    created_swapchain.image_views = calloc(created_swapchain.image_count, sizeof(VkImageView));
    if (!created_swapchain.image_views) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto fail; }

    for (uint32_t image_index = 0; image_index < created_swapchain.image_count; image_index++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = created_swapchain.images[image_index],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = created_swapchain.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1
            }
        };
        result = vkCreateImageView(device, &view_info, NULL,
            &created_swapchain.image_views[image_index]);
        if (result != VK_SUCCESS) {
            // Failed Vulkan creation leaves no owned view at this index.
            created_swapchain.image_views[image_index] = VK_NULL_HANDLE;
            printf("[Vulkan] Image view creation failed (index %u)\n", image_index);
            goto fail;
        }
    }

    *swapchain = created_swapchain;
    printf("[Vulkan] Swapchain: %ux%u, %u images\n", extent.width, extent.height, swapchain->image_count);
    return 0;

fail:
    free(formats);
    free(available_modes);
    gdmf_vulkan_swapchain_destroy(device, &created_swapchain);
    printf("[Vulkan] Swapchain setup failed: %d\n", result);
    return -1;
}
