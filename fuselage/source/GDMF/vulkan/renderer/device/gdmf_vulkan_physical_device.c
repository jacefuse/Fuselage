// Choose a physical device for the existing presentation surface.
// SEE: (GDMF_VULKAN.md, Physical-device selection).
#include "gdmf_vulkan_physical_device.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkDeviceSize local_heap_bytes;
    uint32_t graphics_family;
    uint32_t present_family;
    bool suitable;
    int score;
} gdmf_vulkan_device_candidate;
static bool gdmf_vulkan_device_find_queues(gdmf_vulkan_device_candidate* candidate, VkSurfaceKHR surface) {
    uint32_t queue_family_count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(candidate->device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_properties = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
    if (!queue_properties) { return false; }
    vkGetPhysicalDeviceQueueFamilyProperties(candidate->device, &queue_family_count, queue_properties);

    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family  = UINT32_MAX;

    for (uint32_t queue_family_index = 0; queue_family_index < queue_family_count; queue_family_index++) {
        VkBool32 can_present = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(candidate->device, queue_family_index, surface, &can_present);
        if ((queue_properties[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && can_present) {
            graphics_family = present_family = queue_family_index;
            break;
        }
    }

    if (graphics_family == UINT32_MAX) {
        for (uint32_t queue_family_index = 0; queue_family_index < queue_family_count; queue_family_index++) {
            if ((queue_properties[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphics_family == UINT32_MAX) { graphics_family = queue_family_index; }
            VkBool32 can_present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate->device, queue_family_index, surface, &can_present);
            if (can_present && present_family == UINT32_MAX) { present_family = queue_family_index; }
        }
    }

    free(queue_properties);
    if (graphics_family == UINT32_MAX || present_family == UINT32_MAX) { return false; }
    candidate->graphics_family = graphics_family;
    candidate->present_family  = present_family;

    return true;
}

static bool gdmf_vulkan_device_check_extensions(gdmf_vulkan_device_candidate* candidate) {
    uint32_t extension_count = 0;

    vkEnumerateDeviceExtensionProperties(candidate->device, NULL, &extension_count, NULL);
    VkExtensionProperties* extensions = malloc(extension_count * sizeof(VkExtensionProperties));
    if (!extensions) { return false; }
    vkEnumerateDeviceExtensionProperties(candidate->device, NULL, &extension_count, extensions);

    bool found = false;
    for (uint32_t extension_index = 0; extension_index < extension_count; extension_index++) {
        if (strcmp(extensions[extension_index].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            found = true;
            break;
        }
    }
    free(extensions);

    return found;
}

static bool gdmf_vulkan_device_check_surface(gdmf_vulkan_device_candidate* candidate, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate->device, surface, &capabilities) != VK_SUCCESS) { return false; }
    uint32_t format_count = 0, present_mode_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate->device, surface, &format_count, NULL);
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate->device, surface, &present_mode_count, NULL);

    return format_count > 0 && present_mode_count > 0;
}

static int gdmf_vulkan_device_score(gdmf_vulkan_device_candidate* candidate) {
    int score = 0;

    if (candidate->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { score += 100000; }
    score += (int)(candidate->properties.limits.maxImageDimension2D / 16);
    VkDeviceSize local_heap_bytes = candidate->local_heap_bytes;
    const VkDeviceSize local_heap_score_cap = 64ULL * 1024 * 1024 * 1024; // 64 GiB scoring cap.
    if (local_heap_bytes > local_heap_score_cap) { local_heap_bytes = local_heap_score_cap; }

    score += (int)(local_heap_bytes / (1024ULL * 1024 * 1024)) * 100;

    return score;
}

const char* gdmf_vulkan_physical_device_type_name(VkPhysicalDeviceType device_type) {
    switch (device_type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";

    default:                                     return "Other";
    }
}

int gdmf_vulkan_select_physical_device(VkInstance instance, VkSurfaceKHR surface,
    gdmf_vulkan_physical_device_selection* selection) {
    uint32_t device_count = 0;

    if (vkEnumeratePhysicalDevices(instance, &device_count, NULL) != VK_SUCCESS || device_count == 0) {
        printf("[Vulkan] No Vulkan-capable devices found\n");

        return -1;
    }

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    if (!devices) { return -1; }
    vkEnumeratePhysicalDevices(instance, &device_count, devices);

    gdmf_vulkan_device_candidate* candidates = malloc(device_count * sizeof(gdmf_vulkan_device_candidate));
    if (!candidates) { free(devices); return -1; }
    for (uint32_t device_index = 0; device_index < device_count; device_index++) {
        gdmf_vulkan_device_candidate* candidate = &candidates[device_index];

        *candidate = (gdmf_vulkan_device_candidate){ .device = devices[device_index], .graphics_family = UINT32_MAX, .present_family = UINT32_MAX };
        vkGetPhysicalDeviceProperties(candidate->device, &candidate->properties);
        vkGetPhysicalDeviceMemoryProperties(candidate->device, &candidate->memory_properties);

        candidate->local_heap_bytes = 0;
        for (uint32_t heap_index = 0; heap_index < candidate->memory_properties.memoryHeapCount; heap_index++)
            if (candidate->memory_properties.memoryHeaps[heap_index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) { candidate->local_heap_bytes += candidate->memory_properties.memoryHeaps[heap_index].size; }
        unsigned long long local_heap_mebibytes = (unsigned long long)(candidate->local_heap_bytes / (1024 * 1024));
        const char* device_type = gdmf_vulkan_physical_device_type_name(candidate->properties.deviceType);

        const char* rejection_reason = NULL;
        if (!gdmf_vulkan_device_find_queues(candidate, surface))           { rejection_reason = "no suitable graphics/present queue family"; }
        else if (!gdmf_vulkan_device_check_extensions(candidate)) { rejection_reason = "missing required device extensions"; }
        else if (!gdmf_vulkan_device_check_surface(candidate, surface))    { rejection_reason = "no usable surface formats/present modes"; }

        if (!rejection_reason) {
            candidate->suitable = true;
            candidate->score    = gdmf_vulkan_device_score(candidate);
            printf("[Vulkan] Candidate %u: %s (%s, %llu MiB device-local heaps); suitable, score %d\n",
                device_index, candidate->properties.deviceName, device_type, local_heap_mebibytes, candidate->score);
        } else {
            printf("[Vulkan] Candidate %u: %s (%s, %llu MiB device-local heaps); rejected: %s\n",
                device_index, candidate->properties.deviceName, device_type, local_heap_mebibytes, rejection_reason);
        }
    }
    free(devices);

    gdmf_vulkan_device_candidate* best_candidate = NULL;
    for (uint32_t device_index = 0; device_index < device_count; device_index++) {
        if (candidates[device_index].suitable && (!best_candidate || candidates[device_index].score > best_candidate->score)) { best_candidate = &candidates[device_index]; }
    }

    if (!best_candidate) {
        printf("[Vulkan] No suitable device found\n");

        free(candidates);
        return -1;
    }

    *selection = (gdmf_vulkan_physical_device_selection){
        .device = best_candidate->device,
        .properties = best_candidate->properties,
        .memory_properties = best_candidate->memory_properties,
        .graphics_family = best_candidate->graphics_family,
        .present_family = best_candidate->present_family,
        .score = best_candidate->score
    };
    printf("[Vulkan] Queues: graphics=%u present=%u%s\n",
        selection->graphics_family, selection->present_family,
        selection->graphics_family == selection->present_family ? " (unified)" : " (separate)");

    free(candidates);

    return 0;
}



