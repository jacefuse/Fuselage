#ifndef GDMF_VULKAN_PHYSICAL_DEVICE_H
#define GDMF_VULKAN_PHYSICAL_DEVICE_H

#include <vulkan/vulkan.h>

// Selection metadata, owned by the caller. The device handle is borrowed.
typedef struct {
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    uint32_t graphics_family;
    uint32_t present_family;
    int score;
} gdmf_vulkan_physical_device_selection;

// Main/render thread during initialization. Supply valid instance, surface,
// and output pointers. Returns 0 on success or -1 if selection fails.
// The output is written only on success. Does not create a logical device.
int gdmf_vulkan_select_physical_device(VkInstance instance, VkSurfaceKHR surface,
    gdmf_vulkan_physical_device_selection* selection);

// Static display label; unknown device types return "Other".
const char* gdmf_vulkan_physical_device_type_name(VkPhysicalDeviceType device_type);

#endif // GDMF_VULKAN_PHYSICAL_DEVICE_H
