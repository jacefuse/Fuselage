#ifndef GDMF_VULKAN_SWAPCHAIN_H
#define GDMF_VULKAN_SWAPCHAIN_H

#include "../device/gdmf_vulkan_physical_device.h"
#include <stdbool.h>

// Caller owns the handle, views, and arrays. Images belong to the swapchain.
typedef struct {
    VkSwapchainKHR handle;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkExtent2D extent;
    VkPresentModeKHR present_mode;
    VkImageUsageFlags image_usage;
    uint32_t image_count;
    VkImage* images;
    VkImageView* image_views;
} gdmf_vulkan_swapchain;

// Main/render thread. Supply valid device/surface/selection and output pointers.
// window_extent is the current pixel-size pair, used when the surface permits.
// Returns 0 on success or -1 on failure. Failed creation cleans up its resources
// and leaves output untouched. Do not overwrite an existing owned swapchain.
int gdmf_vulkan_swapchain_create(const gdmf_vulkan_physical_device_selection* selection,
    VkDevice device, VkSurfaceKHR surface, VkExtent2D window_extent,
    bool vsync_enabled, gdmf_vulkan_swapchain* swapchain);

// Finish GPU/presentation work and release dependent framebuffers first.
// Does not wait. Supply a zeroed or successfully created result; clears it.
void gdmf_vulkan_swapchain_destroy(VkDevice device, gdmf_vulkan_swapchain* swapchain);

#endif // GDMF_VULKAN_SWAPCHAIN_H
