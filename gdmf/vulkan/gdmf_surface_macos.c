// GDMF -- macOS Vulkan surface creation.
// Isolates everything platform-specific about getting a VkSurfaceKHR from
// the rest of the renderer (gdmf_vulkan.c, which is otherwise platform-
// neutral) -- the macOS sibling of gdmf_surface_win32.c. Vulkan reaches the
// screen here through MoltenVK: VK_EXT_metal_surface wraps the CAMetalLayer
// that backs the window's content view (see gdmf_window_macos_stub.m).

#define VK_USE_PLATFORM_METAL_EXT
#include "gdmf.h"
#include <vulkan/vulkan.h>
#include <stdio.h>

const char* gdmf_platform_surface_extension(void) {
    return "VK_EXT_metal_surface";
}

int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* outSurface) {
    void* layer = GDMF_GetMetalLayer();

    if (!layer) {
        printf("[Vulkan] No CAMetalLayer available for surface creation\n");
        //tlPrint("[Vulkan] No CAMetalLayer available for surface creation");tlNewLine();

        return -1;
    }

    PFN_vkCreateMetalSurfaceEXT pfn_create_surface =
        (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(
            instance, "vkCreateMetalSurfaceEXT");
    if (!pfn_create_surface) {
        printf("[Vulkan] vkCreateMetalSurfaceEXT not found\n");
        //tlPrint("[Vulkan] vkCreateMetalSurfaceEXT not found");tlNewLine();

        return -1;
    }

    VkMetalSurfaceCreateInfoEXT ci = {
        .sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pLayer = layer
    };

    if (pfn_create_surface(instance, &ci, NULL, outSurface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");
        //tlPrint("[Vulkan] Surface creation failed");tlNewLine();

        return -1;
    }

    printf("[Vulkan] Surface created\n");
    //tlPrint("[Vulkan] Surface created");tlNewLine();

    return 0;
}
