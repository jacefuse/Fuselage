#ifndef GDMF_VULKAN_SURFACE_H
#define GDMF_VULKAN_SURFACE_H

// Internal platform-surface interface. The build selects Windows, macOS, or
// the Linux Wayland/X11 dispatcher. Only platform implementation files need
// VK_USE_PLATFORM_* definitions and native surface types.
#include <vulkan/vulkan.h>

const char* gdmf_platform_surface_extension(void);

// The native window must already exist. Returns 0 on success, -1 on failure.
// Supply a valid output pointer; do not consume its value on failure.
int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* surface);

#endif // GDMF_VULKAN_SURFACE_H
