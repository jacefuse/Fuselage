// GDMF -- Win32 Vulkan surface creation.
// Isolates everything platform-specific about getting a VkSurfaceKHR from
// the rest of the renderer (gdmf_vulkan.c, which is otherwise platform-
// neutral). Adding a second platform later means writing one new file like
// this one and pointing the Makefile at it for that platform, not
// threading #ifdefs through the core instance/device/swapchain code.

#define VK_USE_PLATFORM_WIN32_KHR
#include "gdmf.h"
#include <vulkan/vulkan.h>
#include <stdio.h>

const char* gdmf_platform_surface_extension(void) {
    return "VK_KHR_win32_surface";
}

int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* outSurface) {
    HWND      hwnd  = GDMFgetHWND();
    HINSTANCE hinst = GetModuleHandleA(NULL);

    if (!hwnd) {
        printf("[Vulkan] No HWND available for surface creation\n");
        tlPrint("[Vulkan] No HWND available for surface creation");tlNewLine();

        return -1;
    }

    PFN_vkCreateWin32SurfaceKHR pfn_create_surface =
        (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(
            instance, "vkCreateWin32SurfaceKHR");
    if (!pfn_create_surface) {
        printf("[Vulkan] vkCreateWin32SurfaceKHR not found\n");
        tlPrint("[Vulkan] vkCreateWin32SurfaceKHR not found");tlNewLine();

        return -1;
    }

    VkWin32SurfaceCreateInfoKHR ci = {
        .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = hinst,
        .hwnd      = hwnd
    };

    if (pfn_create_surface(instance, &ci, NULL, outSurface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");
        tlPrint("[Vulkan] Surface creation failed");tlNewLine();

        return -1;
    }

    //FLOG("[Vulkan] Surface created\n");
    printf("[Vulkan] Surface created\n");
    tlPrint("[Vulkan] Surface created");tlNewLine();

    return 0;
}