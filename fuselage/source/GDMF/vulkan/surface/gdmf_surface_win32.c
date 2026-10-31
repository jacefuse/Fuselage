// Create a Vulkan surface for the existing Win32 window.
#define VK_USE_PLATFORM_WIN32_KHR
#include "../../gdmf.h"
#include "gdmf_vulkan_surface.h"
#include <stdio.h>

const char* gdmf_platform_surface_extension(void) {
    return "VK_KHR_win32_surface";
}

int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* surface) {
    HWND window_handle = (HWND)GDMF_GetHWND();
    HINSTANCE instance_handle = GetModuleHandleA(NULL);

    if (!window_handle) {
        printf("[Vulkan] No HWND available for surface creation\n");
        return -1;
    }

    PFN_vkCreateWin32SurfaceKHR create_surface =
        (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(
            instance, "vkCreateWin32SurfaceKHR");
    if (!create_surface) {
        printf("[Vulkan] vkCreateWin32SurfaceKHR not found\n");
        return -1;
    }

    VkWin32SurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = instance_handle,
        .hwnd = window_handle
    };

    if (create_surface(instance, &surface_info, NULL, surface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");
        return -1;
    }

    printf("[Vulkan] Surface created\n");
    return 0;
}
