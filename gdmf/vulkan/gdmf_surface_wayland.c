// GDMF -- Wayland Vulkan surface creation.
// Isolates everything platform-specific about getting a VkSurfaceKHR from
// the rest of the renderer (gdmf_vulkan.c, which is otherwise platform-
// neutral) -- the Linux/Wayland sibling of gdmf_surface_win32.c and
// gdmf_surface_macos.c. Unlike those, Wayland needs two handles: the
// wl_display connection and the wl_surface on it (see gdmf.h's accessor
// comment); both come from the window backend (gdmf_window_wayland.c).
// Namespaced (gdmf_surface_wl_*) rather than implementing the platform
// seam directly: on Linux one binary carries both backends, and
// gdmf_surface_linux.c forwards the seam to whichever won the probe.

#include <wayland-client.h>
#define VK_USE_PLATFORM_WAYLAND_KHR
#include "gdmf.h"
#include <vulkan/vulkan.h>
#include <stdio.h>

const char* gdmf_surface_wl_extension(void) {
    return "VK_KHR_wayland_surface";
}

int gdmf_surface_wl_create(VkInstance instance, VkSurfaceKHR* outSurface) {
    struct wl_display* display = (struct wl_display*)GDMF_GetWaylandDisplay();
    struct wl_surface* surface = (struct wl_surface*)GDMF_GetWaylandSurface();

    if (!display || !surface) {
        printf("[Vulkan] No Wayland display/surface available for surface creation\n");

        return -1;
    }

    PFN_vkCreateWaylandSurfaceKHR pfn_create_surface =
        (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(
            instance, "vkCreateWaylandSurfaceKHR");
    if (!pfn_create_surface) {
        printf("[Vulkan] vkCreateWaylandSurfaceKHR not found\n");

        return -1;
    }

    VkWaylandSurfaceCreateInfoKHR ci = {
        .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = display,
        .surface = surface
    };

    if (pfn_create_surface(instance, &ci, NULL, outSurface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");

        return -1;
    }

    printf("[Vulkan] Surface created\n");

    return 0;
}
