#ifndef GDMF_VULKAN_INSTANCE_H
#define GDMF_VULKAN_INSTANCE_H

#include <vulkan/vulkan.h>

// Main/render thread, once per renderer lifecycle, after window selection.
// Returns 0 on success or -1 on failure. DEBUG reporting is optional.
int gdmf_vulkan_instance_create(void);

// Borrowed handle; VK_NULL_HANDLE before creation and after destruction.
VkInstance gdmf_vulkan_instance_get(void);

// Destroy dependent devices and surfaces first. Also releases debug reporting.
void gdmf_vulkan_instance_destroy(void);

#endif // GDMF_VULKAN_INSTANCE_H
