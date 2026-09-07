// GDMF -- X11 Vulkan surface creation.
// Isolates everything platform-specific about getting a VkSurfaceKHR from
// the rest of the renderer (gdmf_vulkan.c, which is otherwise platform-
// neutral) -- the Linux/X11 sibling of gdmf_surface_wayland.c. Xlib needs
// two handles: the Display connection and the Window id on it (see gdmf.h's
// accessor comment); both come from the window backend (gdmf_window_x11.c).
// Namespaced (gdmf_surface_x11_*) rather than implementing the platform
// seam directly: on Linux one binary carries both backends, and
// gdmf_surface_linux.c forwards the seam to whichever won the probe.

#include <X11/Xlib.h>
#define VK_USE_PLATFORM_XLIB_KHR
#include "gdmf.h"
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdint.h>

const char* gdmf_surface_x11_extension(void) {
    return "VK_KHR_xlib_surface";
}

int gdmf_surface_x11_create(VkInstance instance, VkSurfaceKHR* outSurface) {
    Display* display = (Display*)GDMF_GetX11Display();
    Window   window  = (Window)(uintptr_t)GDMF_GetX11Window();

    if (!display || !window) {
        printf("[Vulkan] No X11 display/window available for surface creation\n");

        return -1;
    }

    PFN_vkCreateXlibSurfaceKHR pfn_create_surface =
        (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(
            instance, "vkCreateXlibSurfaceKHR");
    if (!pfn_create_surface) {
        printf("[Vulkan] vkCreateXlibSurfaceKHR not found\n");

        return -1;
    }

    VkXlibSurfaceCreateInfoKHR ci = {
        .sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy    = display,
        .window = window
    };

    if (pfn_create_surface(instance, &ci, NULL, outSurface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");

        return -1;
    }

    printf("[Vulkan] Surface created\n");

    return 0;
}
