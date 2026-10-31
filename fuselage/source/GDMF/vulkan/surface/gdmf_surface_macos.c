// Wrap the AppKit-owned CAMetalLayer in the current Vulkan/MoltenVK surface.
// SEE: (GDMF_MACOS.md, Rendering surface).
#define VK_USE_PLATFORM_METAL_EXT
#include "../../gdmf.h"
#include "gdmf_vulkan_surface.h"
#include <stdio.h>

const char* gdmf_platform_surface_extension(void) {
    return "VK_EXT_metal_surface";
}

int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* surface) {
    void* metal_layer = GDMF_GetMetalLayer();

    if (!metal_layer) {
        printf("[Vulkan] No CAMetalLayer available for surface creation\n");
        return -1;
    }

    PFN_vkCreateMetalSurfaceEXT create_surface =
        (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(
            instance, "vkCreateMetalSurfaceEXT");
    if (!create_surface) {
        printf("[Vulkan] vkCreateMetalSurfaceEXT not found\n");
        return -1;
    }

    VkMetalSurfaceCreateInfoEXT surface_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pLayer = metal_layer
    };

    if (create_surface(instance, &surface_info, NULL, surface) != VK_SUCCESS) {
        printf("[Vulkan] Surface creation failed\n");
        return -1;
    }

    printf("[Vulkan] Surface created\n");
    return 0;
}
