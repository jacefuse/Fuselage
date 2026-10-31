#ifndef GDMF_VULKAN_MEMORY_H
#define GDMF_VULKAN_MEMORY_H

#include <vulkan/vulkan.h>

// Supply valid memory properties from the selected physical device.
// Returns the first permitted type containing all required_flags, or UINT32_MAX.
uint32_t gdmf_find_memory_type(const VkPhysicalDeviceMemoryProperties* memory_properties,
    uint32_t type_filter, VkMemoryPropertyFlags required_flags);

// Supply matching device/properties, valid requirements, and an output pointer.
// Prefer device-local, host-visible, coherent memory; otherwise try host-visible,
// coherent memory. Returns the successful/final allocation result, or
// VK_ERROR_OUT_OF_DEVICE_MEMORY if no fallback type exists. Output changes only
// on success. The caller owns the allocation; no binding or mapping is done.
VkResult gdmf_allocate_host_visible_prefer_device_local(VkDevice device,
    const VkPhysicalDeviceMemoryProperties* memory_properties,
    const VkMemoryRequirements* requirements, VkDeviceMemory* memory);

#endif // GDMF_VULKAN_MEMORY_H
