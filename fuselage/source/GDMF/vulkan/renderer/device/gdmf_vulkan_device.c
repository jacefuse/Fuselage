// Logical device, queues, and optional device-lifetime pipeline cache.
// SEE: (GDMF_VULKAN.md, Logical-device creation).
#include "gdmf_vulkan_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gdmf_vulkan_device_create(const gdmf_vulkan_physical_device_selection* selection,
    gdmf_vulkan_logical_device* logical_device) {
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_infos[2];
    uint32_t queue_info_count = 0;

    queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = selection->graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    if (selection->present_family != selection->graphics_family) {
        queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = selection->present_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
    }

    const char* extensions[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    uint32_t extension_count = 1;
    uint32_t available_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(selection->device, NULL,
        &available_extension_count, NULL);
    VkExtensionProperties* available_extensions =
        malloc(available_extension_count * sizeof(VkExtensionProperties));
    if (available_extensions) {
        vkEnumerateDeviceExtensionProperties(selection->device, NULL,
            &available_extension_count, available_extensions);
        for (uint32_t extension_index = 0; extension_index < available_extension_count; extension_index++) {
            if (strcmp(available_extensions[extension_index].extensionName, "VK_KHR_portability_subset") == 0) {
                extensions[extension_count++] = "VK_KHR_portability_subset";
                break;
            }
        }
        free(available_extensions);
    }

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_info_count,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = NULL
    };

    gdmf_vulkan_logical_device created_device = {0};
    if (vkCreateDevice(selection->device, &create_info, NULL, &created_device.device) != VK_SUCCESS) {
        printf("[Vulkan] Logical device creation failed\n");
        return -1;
    }

    vkGetDeviceQueue(created_device.device, selection->graphics_family, 0, &created_device.graphics_queue);
    vkGetDeviceQueue(created_device.device, selection->present_family, 0, &created_device.present_queue);

    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    if (vkCreatePipelineCache(created_device.device, &cache_info, NULL,
            &created_device.pipeline_cache) != VK_SUCCESS) {
        created_device.pipeline_cache = VK_NULL_HANDLE;
        printf("[Vulkan] Pipeline cache creation failed; continuing without\n");
    }

    *logical_device = created_device;
    printf("[Vulkan] Logical device created\n");
    return 0;
}

void gdmf_vulkan_device_destroy(gdmf_vulkan_logical_device* logical_device) {
    if (logical_device->pipeline_cache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(logical_device->device, logical_device->pipeline_cache, NULL);
    }
    if (logical_device->device != VK_NULL_HANDLE) {
        vkDestroyDevice(logical_device->device, NULL);
    }
    *logical_device = (gdmf_vulkan_logical_device){0};
}
