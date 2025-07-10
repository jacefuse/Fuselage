// ---- Graphic Device Minimalist Framework ----
// GDMF is intended as a minimalist interface between Fuselage and Vulkan.
// It is not intended as an entire software development framework.
// While it may be possible to use it beyond it's intended purpose of
// providing a foundation for the Fuselage Virtual Machine it is
// recommended that for general purpose use a more suitable framework
// should be selected. GLFW, RGFW, RayLib, or SDL might be a better choice.

// Fuselage BUTTOCKS 0.2.250901
// Fuselage is provided as free software in an opened source nature.
// It should be considered experimental. No retrictions are in place.
// With no restrictions comes no warranty or liability.
// Any possible support is only offered on a purely voluntary basis.

#include <windows.h>
#include <tchar.h>
#include <stdbool.h>

#include <vulkan.h>
#include <vulkan_win32.h>

typedef struct {
    VkPhysicalDevice device;
    uint32_t graphics_family;
    uint32_t present_family;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;
    int score;
    bool suitable;
} GDMFDeviceCandidate;

// Main facing functions
int GDMFinit(void);
void GDMFshutdown(void);

