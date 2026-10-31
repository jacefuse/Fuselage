// Memory selection using caller-supplied physical-device properties.
// SEE: (GDMF_VULKAN.md, Memory selection and allocation).
#include "gdmf_vulkan_memory.h"

uint32_t gdmf_find_memory_type(const VkPhysicalDeviceMemoryProperties* memory_properties,
    uint32_t type_filter, VkMemoryPropertyFlags required_flags) {
    for (uint32_t memory_type_index = 0; memory_type_index < memory_properties->memoryTypeCount; memory_type_index++) {
        if ((type_filter & (1u << memory_type_index)) &&
            (memory_properties->memoryTypes[memory_type_index].propertyFlags & required_flags) == required_flags) {
            return memory_type_index;
        }
    }
    return UINT32_MAX;
}

VkResult gdmf_allocate_host_visible_prefer_device_local(VkDevice device,
    const VkPhysicalDeviceMemoryProperties* memory_properties,
    const VkMemoryRequirements* requirements, VkDeviceMemory* memory) {
    VkMemoryAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements->size
    };
    uint32_t preferred_type = gdmf_find_memory_type(memory_properties, requirements->memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uint32_t fallback_type = gdmf_find_memory_type(memory_properties, requirements->memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory allocated_memory = VK_NULL_HANDLE;
    if (preferred_type != UINT32_MAX && preferred_type != fallback_type) {
        allocation_info.memoryTypeIndex = preferred_type;
        VkResult result = vkAllocateMemory(device, &allocation_info, NULL, &allocated_memory);
        if (result == VK_SUCCESS) {
            *memory = allocated_memory;
            return VK_SUCCESS;
        }
    }

    if (fallback_type == UINT32_MAX) { return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
    allocation_info.memoryTypeIndex = fallback_type;
    VkResult result = vkAllocateMemory(device, &allocation_info, NULL, &allocated_memory);
    if (result == VK_SUCCESS) { *memory = allocated_memory; }
    return result;
}
