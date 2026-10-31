// Match the Vulkan surface to the already-selected Linux window backend.
// SEE: (GDMF_VULKAN.md, Linux surface dispatch).
#include "../../window/gdmf_window_linux.h"
#include "gdmf_vulkan_surface.h"
#include "gdmf_vulkan_surface_linux.h"
#include <stdio.h>
#include <string.h>

static bool gdmf_surface_backend_is(const char* backend_name) {
    const gdmf_linux_window_backend* window_backend = gdmf_linux_get_active_window_backend();
    return window_backend && strcmp(window_backend->name, backend_name) == 0;
}

const char* gdmf_platform_surface_extension(void) {
    if (gdmf_surface_backend_is("wayland")) { return gdmf_surface_wayland_extension(); }
    if (gdmf_surface_backend_is("x11")) { return gdmf_surface_x11_extension(); }

    // Current failure sentinel, not a supported Vulkan extension.
    printf("[Vulkan] No window backend active; no surface extension\n");
    return "VK_KHR_surface_missing_backend";
}

int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* surface) {
    if (gdmf_surface_backend_is("wayland")) { return gdmf_surface_wayland_create(instance, surface); }
    if (gdmf_surface_backend_is("x11")) { return gdmf_surface_x11_create(instance, surface); }

    printf("[Vulkan] No window backend active; cannot create a surface\n");
    return -1;
}
