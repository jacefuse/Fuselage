// Create the Vulkan surface for the selected Wayland window backend.
// Native objects stay opaque here; this adapter makes no Wayland client calls.
struct wl_display;
struct wl_surface;

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "../../gdmf.h"
#include "gdmf_vulkan_surface_linux.h"
#include <stdio.h>

const char* gdmf_surface_wayland_extension(void) {
    return "VK_KHR_wayland_surface";
}

int gdmf_surface_wayland_create(VkInstance instance, VkSurfaceKHR* surface) {
    struct wl_display* display_connection = (struct wl_display*)GDMF_GetWaylandDisplay();
    struct wl_surface* native_surface = (struct wl_surface*)GDMF_GetWaylandSurface();

    if (!display_connection || !native_surface) {
        printf("[Vulkan] No Wayland display/surface available for surface creation\n");
        return -1;
    }

    PFN_vkCreateWaylandSurfaceKHR create_surface =
        (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(
            instance, "vkCreateWaylandSurfaceKHR");
    if (!create_surface) {
        printf("[Vulkan] vkCreateWaylandSurfaceKHR not found\n");
        return -1;
    }

    VkWaylandSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = display_connection,
        .surface = native_surface
    };

    if (create_surface(instance, &surface_info, NULL, surface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");
        return -1;
    }

    printf("[Vulkan] Surface created\n");
    return 0;
}
