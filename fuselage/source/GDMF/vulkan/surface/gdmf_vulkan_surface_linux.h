#ifndef GDMF_VULKAN_SURFACE_LINUX_H
#define GDMF_VULKAN_SURFACE_LINUX_H

// Internal Linux surface adapters, selected by gdmf_surface_linux.c.
// Platform-specific declarations stay in each adapter implementation.
#include <vulkan/vulkan.h>

const char* gdmf_surface_wayland_extension(void);
const char* gdmf_surface_x11_extension(void);

// Native window objects must exist. Returns 0 on success or -1 on failure.
// Supply a valid output pointer; do not consume its value on failure.
int gdmf_surface_wayland_create(VkInstance instance, VkSurfaceKHR* surface);
int gdmf_surface_x11_create(VkInstance instance, VkSurfaceKHR* surface);

#endif // GDMF_VULKAN_SURFACE_LINUX_H
