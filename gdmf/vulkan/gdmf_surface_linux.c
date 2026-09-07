// GDMF -- Linux Vulkan surface dispatcher.
// The platform surface seam (gdmf_platform_surface_extension /
// gdmf_create_platform_surface) implemented by forwarding to whichever
// window backend won the runtime probe (see gdmf_window_linux.c). Safe
// because of GDMF_Init's ordering: the window exists -- and the backend is
// therefore decided -- before gdmf_vulkan_init ever asks for the
// extension name, so no both-extensions instance juggling is needed.

#include "gdmf.h"
#include "gdmf_window_linux.h"
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <string.h>

// The Wayland half (gdmf_surface_wayland.c) and the X11 half
// (gdmf_surface_x11.c).
const char* gdmf_surface_wl_extension(void);
int         gdmf_surface_wl_create(VkInstance instance, VkSurfaceKHR* outSurface);
const char* gdmf_surface_x11_extension(void);
int         gdmf_surface_x11_create(VkInstance instance, VkSurfaceKHR* outSurface);

static bool gdmf_surface_backend_is(const char* name) {
    const gdmf_linux_backend* backend = gdmf_linux_active_backend();

    return backend && strcmp(backend->name, name) == 0;
}

const char* gdmf_platform_surface_extension(void) {
    if (gdmf_surface_backend_is("wayland")) { return gdmf_surface_wl_extension(); }
    if (gdmf_surface_backend_is("x11"))     { return gdmf_surface_x11_extension(); }

    // No backend decided: GDMF_Init calls the window backend first, so
    // reaching here means window creation failed and Vulkan init should
    // fail right behind it -- an unsatisfiable extension name guarantees
    // that without a special case in gdmf_vulkan.c.
    printf("[Vulkan] No window backend active -- no surface extension\n");
    return "VK_KHR_surface_missing_backend";
}

int gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* outSurface) {
    if (gdmf_surface_backend_is("wayland")) { return gdmf_surface_wl_create(instance, outSurface); }
    if (gdmf_surface_backend_is("x11"))     { return gdmf_surface_x11_create(instance, outSurface); }

    printf("[Vulkan] No window backend active -- cannot create a surface\n");
    return -1;
}
