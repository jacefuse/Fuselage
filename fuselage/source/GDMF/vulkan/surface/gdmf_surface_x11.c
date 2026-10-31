// Create the Vulkan surface from the selected X11 display and window.
#include <X11/Xlib.h>
#define VK_USE_PLATFORM_XLIB_KHR
#include "../../gdmf.h"
#include "gdmf_vulkan_surface_linux.h"
#include <stdio.h>
#include <stdint.h>

const char* gdmf_surface_x11_extension(void) {
    return "VK_KHR_xlib_surface";
}

int gdmf_surface_x11_create(VkInstance instance, VkSurfaceKHR* surface) {
    Display* display_connection = (Display*)GDMF_GetX11Display();
    Window window_id = (Window)(uintptr_t)GDMF_GetX11Window();

    if (!display_connection || !window_id) {
        printf("[Vulkan] No X11 display/window available for surface creation\n");
        return -1;
    }

    PFN_vkCreateXlibSurfaceKHR create_surface =
        (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(
            instance, "vkCreateXlibSurfaceKHR");
    if (!create_surface) {
        printf("[Vulkan] vkCreateXlibSurfaceKHR not found\n");
        return -1;
    }

    VkXlibSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = display_connection,
        .window = window_id
    };

    if (create_surface(instance, &surface_info, NULL, surface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");
        return -1;
    }

    printf("[Vulkan] Surface created\n");
    return 0;
}
