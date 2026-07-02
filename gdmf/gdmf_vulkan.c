// GDMF -- Vulkan subsystem
// Instance, surface, device, swapchain, render pass, framebuffers, render loop.
// Platform-neutral: surface creation is delegated to gdmf_create_platform_surface()
// (see gdmf_surface_win32.c), so this file has no VK_USE_PLATFORM_*_KHR define
// and no platform-specific Vulkan types of its own.

#include "gdmf.h"
#include <vulkan/vulkan.h>
#include "gdmf_vulkan.h"
#include "gdmf_vulkan_internal.h"
//#include "gdmf_textlayer.h"
//#include "fuselage_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Instance, surface, device
static VkInstance       g_vkInstance     = VK_NULL_HANDLE;
static VkSurfaceKHR     g_vkSurface      = VK_NULL_HANDLE;
static VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
static uint32_t         g_graphicsFamily = UINT32_MAX;
static uint32_t         g_presentFamily  = UINT32_MAX;
static VkDevice         g_vkDevice       = VK_NULL_HANDLE;
static VkQueue          g_graphicsQueue  = VK_NULL_HANDLE;
static VkQueue          g_presentQueue   = VK_NULL_HANDLE;

// Swapchain
static VkRenderPass     g_renderPass          = VK_NULL_HANDLE;
static VkSwapchainKHR   g_vkSwapchain         = VK_NULL_HANDLE;
static VkFormat         g_swapchainFormat     = VK_FORMAT_UNDEFINED;
static VkExtent2D       g_swapchainExtent     = {0};
static uint32_t         g_swapchainImageCount = 0;
static VkImage*         g_swapchainImages     = NULL;
static VkImageView*     g_swapchainImageViews = NULL;
static VkFramebuffer*   g_framebuffers        = NULL;

// Per-image sync and command objects
// All indexed by swapchain image index. Recreated with the swapchain.
//
// Acquire semaphore swap pattern: g_acquireSpare is always passed to
// vkAcquireNextImageKHR. After acquiring image N, swap spare with
// g_perImageAcquire[N]. The swapped-out semaphore (previously associated
// with image N) becomes the new spare -- safe to reuse because image N
// couldn't have been returned by the presentation engine unless its previous
// rendering + present cycle is complete, meaning that semaphore is consumed.
static VkCommandPool    g_commandPool      = VK_NULL_HANDLE;  // stable across recreation
static VkCommandBuffer* g_commandBuffers   = NULL;  // [swapchainImageCount]
static VkSemaphore*     g_perImageAcquire  = NULL;  // [swapchainImageCount]
static VkSemaphore      g_acquireSpare     = VK_NULL_HANDLE;
static VkSemaphore*     g_renderFinished   = NULL;  // [swapchainImageCount]
static VkFence*         g_inFlightFence    = NULL;  // [swapchainImageCount]

// Shared palette buffer -- one per swapchain image, holding the full
// Colors[256][16] table packed via PackRGBA8. Previously each of
// gdmf_sprites.c and gdmf_tiles.c (and, per tile layer, up to
// MAX_TILE_LAYERS times over) maintained its own private copy, all
// uploading byte-for-byte identical content every single frame. GDMF now
// owns this once; sprites/tiles (and any future consumer) just bind this
// same buffer in their own descriptor set instead. Same in-flight-frame
// reasoning as every other per-image resource here: a previous frame's
// command buffer using a different image index may still be executing on
// the GPU and reading its own copy while this one is rewritten.
// GDMF_PALETTE_BUFFER_SIZE itself lives in gdmf_vulkan_internal.h -- every
// consumer (this file, sprites, tiles) needs to agree on the exact same
// size, so it's declared once where they all already include from.
static VkBuffer*        g_paletteBuffers  = NULL;  // [swapchainImageCount]
static VkDeviceMemory*  g_paletteMemories = NULL;  // [swapchainImageCount]

static bool             g_needsSwapchainRecreate = false;

// Debug messenger
#ifdef DEBUG
static VkDebugUtilsMessengerEXT g_debugMessenger = VK_NULL_HANDLE;
#endif

// Device candidate (local to selection, not kept)
typedef struct {
    VkPhysicalDevice                 device;
    VkPhysicalDeviceProperties       properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkDeviceSize                     vram_bytes;  // sum of DEVICE_LOCAL heaps
    uint32_t                         graphics_family;
    uint32_t                         present_family;
    bool                             suitable;
    int                              score;
} DeviceCandidate;

// Forward declarations
static int  gdmf_create_instance(void);
static int  gdmf_create_surface(void);
static int  gdmf_pick_physical_device(void);
static bool device_find_queues(DeviceCandidate* c);
static bool device_check_extensions(DeviceCandidate* c);
static bool device_check_surface(DeviceCandidate* c);
static int  device_score(DeviceCandidate* c);
static const char* device_type_name(VkPhysicalDeviceType type);
static int  gdmf_create_logical_device(void);
static int  gdmf_create_swapchain(void);
static void gdmf_destroy_swapchain(void);
static int  gdmf_create_render_pass(void);
static void gdmf_destroy_render_pass(void);
static int  gdmf_create_framebuffers(void);
static void gdmf_destroy_framebuffers(void);
static int  gdmf_create_command_pool(void);
static int  gdmf_create_per_image_objects(void);
static void gdmf_destroy_per_image_objects(void);
static int  gdmf_create_palette_buffers(void);
static void gdmf_destroy_palette_buffers(void);
static void gdmf_palette_prepare(uint32_t imageIndex);
static int  gdmf_recreate_swapchain(void);

#ifdef DEBUG
static int  gdmf_create_debug_messenger(void);
static void gdmf_destroy_debug_messenger(void);
static VKAPI_ATTR VkBool32 VKAPI_CALL gdmf_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT        type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user);
#endif

// Public
int gdmf_vulkan_init(void) {
    if (gdmf_create_instance()           != 0) { return -1; }
#ifdef DEBUG
    gdmf_create_debug_messenger();
#endif
    if (gdmf_create_surface()            != 0) { return -1; }
    if (gdmf_pick_physical_device()      != 0) { return -1; }
    if (gdmf_create_logical_device()     != 0) { return -1; }
    if (gdmf_create_swapchain()          != 0) { return -1; }
    if (gdmf_create_render_pass()        != 0) { return -1; }
    if (gdmf_create_framebuffers()       != 0) { return -1; }
    if (gdmf_create_command_pool()       != 0) { return -1; }
    if (gdmf_create_per_image_objects()  != 0) { return -1; }
    if (gdmf_create_palette_buffers()    != 0) { return -1; }

    return 0;
}

void gdmf_vulkan_shutdown(void) {
    if (g_vkDevice != VK_NULL_HANDLE) { vkDeviceWaitIdle(g_vkDevice); }

    gdmf_destroy_palette_buffers();
    gdmf_destroy_per_image_objects();

    if (g_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vkDevice, g_commandPool, NULL);
        g_commandPool = VK_NULL_HANDLE;
    }

    gdmf_destroy_framebuffers();
    gdmf_destroy_render_pass();
    gdmf_destroy_swapchain();

    if (g_vkDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(g_vkDevice, NULL);
        g_vkDevice = VK_NULL_HANDLE;
    }
    if (g_vkSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_vkInstance, g_vkSurface, NULL);
        g_vkSurface = VK_NULL_HANDLE;
    }
#ifdef DEBUG
    gdmf_destroy_debug_messenger();
#endif
    if (g_vkInstance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_vkInstance, NULL);
        g_vkInstance = VK_NULL_HANDLE;
    }

    //FLOG("[Vulkan] Shutdown\n");
    printf("[Vulkan] Shutdown\n");
    tlPrint("[Vulkan] Shutdown");tlNewLine();

    return;
}

// Instance
static int gdmf_create_instance(void) {
    uint32_t loader_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion pfn_enumerate_version =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");

    if (pfn_enumerate_version) { pfn_enumerate_version(&loader_version); }

    if (loader_version < VK_API_VERSION_1_3) {
        //FLOG("[Vulkan] Loader reports %u.%u -- Vulkan 1.3 required\n",
        //    VK_API_VERSION_MAJOR(loader_version),
        //    VK_API_VERSION_MINOR(loader_version));
        printf("[Vulkan] Loader reports %u.%u -- Vulkan 1.3 required\n",
            VK_API_VERSION_MAJOR(loader_version),
            VK_API_VERSION_MINOR(loader_version));
        tlPrintFormatted("[Vulkan] Loader reports %u.%u -- Vulkan 1.3 required",
            VK_API_VERSION_MAJOR(loader_version),
            VK_API_VERSION_MINOR(loader_version));tlNewLine();

        return -1;
    }

    uint32_t avail_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, NULL);
    VkExtensionProperties* avail_exts = malloc(avail_ext_count * sizeof(VkExtensionProperties));
    if (!avail_exts) { return -1; }
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, avail_exts);

    const char* required_exts[] = { "VK_KHR_surface", gdmf_platform_surface_extension() };
    const uint32_t required_ext_count = sizeof(required_exts) / sizeof(required_exts[0]);

    for (uint32_t r = 0; r < required_ext_count; r++) {
        bool found = false;

        for (uint32_t i = 0; i < avail_ext_count; i++) {
            if (strcmp(required_exts[r], avail_exts[i].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            //FLOG("[Vulkan] Missing required extension: %s\n", required_exts[r]);
            printf("[Vulkan] Missing required extension: %s\n", required_exts[r]);
            tlPrintFormatted("[Vulkan] Missing required extension: %s", required_exts[r]);tlNewLine();

            free(avail_exts);
            return -1;
        }
    }

    const char* enabled_exts[4];
    uint32_t    enabled_ext_count = 0;
    for (uint32_t i = 0; i < required_ext_count; i++)
        enabled_exts[enabled_ext_count++] = required_exts[i];

#ifdef DEBUG
    bool debug_utils_available = false;
    for (uint32_t i = 0; i < avail_ext_count; i++) {
        if (strcmp(avail_exts[i].extensionName, "VK_EXT_debug_utils") == 0) {
            debug_utils_available = true;
            break;
        }
    }
    if (debug_utils_available) { enabled_exts[enabled_ext_count++] = "VK_EXT_debug_utils"; }
    else{
    //FLOG("[Vulkan] VK_EXT_debug_utils not available -- debug messenger disabled\n");
    printf("[Vulkan] VK_EXT_debug_utils not available -- debug messenger disabled\n");
    tlPrint("[Vulkan] VK_EXT_debug_utils not available -- debug messenger disabled");tlNewLine();

    }

#endif

    free(avail_exts);

    const char* enabled_layers[1];
    uint32_t    enabled_layer_count = 0;

#ifdef DEBUG
    uint32_t avail_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&avail_layer_count, NULL);
    VkLayerProperties* avail_layers = malloc(avail_layer_count * sizeof(VkLayerProperties));
    if (avail_layers) {
        vkEnumerateInstanceLayerProperties(&avail_layer_count, avail_layers);
        for (uint32_t i = 0; i < avail_layer_count; i++) {
            if (strcmp(avail_layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                enabled_layers[enabled_layer_count++] = "VK_LAYER_KHRONOS_validation";
                break;
            }
        }
        free(avail_layers);
    }
    if (enabled_layer_count > 0){
        //FLOG("[Vulkan] Validation layer enabled\n");
        printf("[Vulkan] Validation layer enabled\n");
        tlPrint("[Vulkan] Validation layer enabled");tlNewLine();
    }
        else {
        //FLOG("[Vulkan] VK_LAYER_KHRONOS_validation not available\n");
        printf("[Vulkan] VK_LAYER_KHRONOS_validation not available\n");
        tlPrint("[Vulkan] VK_LAYER_KHRONOS_validation not available");tlNewLine();
    }

#endif

    VkApplicationInfo app_info = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Fuselage",
        .applicationVersion = VK_MAKE_VERSION(0, 3, 0),
        .pEngineName        = "Fuselage BUTTOCKS",
        .engineVersion      = VK_MAKE_VERSION(0, 3, 0),
        .apiVersion         = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo ci = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &app_info,
        .enabledExtensionCount   = enabled_ext_count,
        .ppEnabledExtensionNames = enabled_exts,
        .enabledLayerCount       = enabled_layer_count,
        .ppEnabledLayerNames     = enabled_layer_count ? enabled_layers : NULL
    };

    if (vkCreateInstance(&ci, NULL, &g_vkInstance) != VK_SUCCESS) {
        printf("[Vulkan] vkCreateInstance failed\n");
        tlPrint("[Vulkan] vkCreateInstance failed");tlNewLine();

        return -1;
    }

    //FLOG("[Vulkan] Instance created (API 1.3)\n");
    printf("[Vulkan] Instance created (API 1.3)\n");
    tlPrint("[Vulkan] Instance created (API 1.3)");tlNewLine();

    return 0;
}

// Surface -- the actual platform work lives in gdmf_create_platform_surface()
// (gdmf_surface_win32.c); this is just the lifecycle step name, kept stable
// so gdmf_vulkan_init()'s call site never needs to change per platform.
static int gdmf_create_surface(void) {
    return gdmf_create_platform_surface(g_vkInstance, &g_vkSurface);
}

// Physical device selection
static bool device_find_queues(DeviceCandidate* c) {
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(c->device, &count, NULL);
    VkQueueFamilyProperties* props = malloc(count * sizeof(VkQueueFamilyProperties));
    if (!props) { return false; }
    vkGetPhysicalDeviceQueueFamilyProperties(c->device, &count, props);

    uint32_t graphics = UINT32_MAX;
    uint32_t present  = UINT32_MAX;

    for (uint32_t i = 0; i < count; i++) {
        VkBool32 can_present = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(c->device, i, g_vkSurface, &can_present);
        if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && can_present) {
            graphics = present = i;
            break;
        }
    }

    if (graphics == UINT32_MAX) {
        for (uint32_t i = 0; i < count; i++) {
            if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphics == UINT32_MAX) { graphics = i; }
            VkBool32 can_present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(c->device, i, g_vkSurface, &can_present);
            if (can_present && present == UINT32_MAX) { present = i; }
        }
    }

    free(props);
    if (graphics == UINT32_MAX || present == UINT32_MAX) { return false; }
    c->graphics_family = graphics;
    c->present_family  = present;

    return true;
}

static bool device_check_extensions(DeviceCandidate* c) {
    uint32_t count = 0;

    vkEnumerateDeviceExtensionProperties(c->device, NULL, &count, NULL);
    VkExtensionProperties* exts = malloc(count * sizeof(VkExtensionProperties));
    if (!exts) { return false; }
    vkEnumerateDeviceExtensionProperties(c->device, NULL, &count, exts);

    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            found = true;
            break;
        }
    }
    free(exts);

    return found;
}

static bool device_check_surface(DeviceCandidate* c) {
    VkSurfaceCapabilitiesKHR caps;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c->device, g_vkSurface, &caps) != VK_SUCCESS) { return false; }
    uint32_t fmt_count = 0, pm_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(c->device, g_vkSurface, &fmt_count, NULL);
    vkGetPhysicalDeviceSurfacePresentModesKHR(c->device, g_vkSurface, &pm_count, NULL);

    return fmt_count > 0 && pm_count > 0;
}

static int device_score(DeviceCandidate* c) {
    int score = 0;

    if (c->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { score += 100000; }
    score += (int)(c->properties.limits.maxImageDimension2D / 16);

    // Clamp before scoring -- some drivers (seen on this machine: Intel
    // UHD 630 on Windows) report a unified-memory DEVICE_LOCAL heap size
    // that's a placeholder/sentinel rather than real VRAM (134TB here),
    // which would otherwise turn this term into millions of points and
    // swamp the +100000 discrete-GPU bonus above. Real GPU VRAM is nowhere
    // near this; treat anything past the cap as the same artifact.
    VkDeviceSize vram = c->vram_bytes;
    const VkDeviceSize vram_cap_bytes = 64ULL * 1024 * 1024 * 1024; // 64GB
    if (vram > vram_cap_bytes) { vram = vram_cap_bytes; }

    score += (int)(vram / (1024ULL * 1024 * 1024)) * 100;

    return score;
}

static const char* device_type_name(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";

    default:                                     return "Other";
    }
}

static int gdmf_pick_physical_device(void) {
    uint32_t count = 0;

    if (vkEnumeratePhysicalDevices(g_vkInstance, &count, NULL) != VK_SUCCESS || count == 0) {
        printf("[Vulkan] No Vulkan-capable devices found\n");
        tlPrint("[Vulkan] No Vulkan-capable devices found");tlNewLine();

        return -1;
    }

    VkPhysicalDevice* devices = malloc(count * sizeof(VkPhysicalDevice));
    if (!devices) { return -1; }
    vkEnumeratePhysicalDevices(g_vkInstance, &count, devices);

    DeviceCandidate* candidates = malloc(count * sizeof(DeviceCandidate));
    if (!candidates) { free(devices); return -1; }

    // Diagnostic: log every enumerated candidate, not just the eventual
    // winner -- otherwise a GPU that's silently disqualified (missing
    // queue family, extension, or surface support) or never enumerated at
    // all by the driver (common on Optimus-style laptops where the
    // discrete GPU is power-gated) leaves no trace to debug from.
    for (uint32_t i = 0; i < count; i++) {
        DeviceCandidate* c = &candidates[i];

        *c = (DeviceCandidate){ .device = devices[i], .graphics_family = UINT32_MAX, .present_family = UINT32_MAX };
        vkGetPhysicalDeviceProperties(c->device, &c->properties);
        vkGetPhysicalDeviceMemoryProperties(c->device, &c->memory_properties);

        c->vram_bytes = 0;
        for (uint32_t h = 0; h < c->memory_properties.memoryHeapCount; h++)
            if (c->memory_properties.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) { c->vram_bytes += c->memory_properties.memoryHeaps[h].size; }
        unsigned long long vram_mb = (unsigned long long)(c->vram_bytes / (1024 * 1024));
        const char* type = device_type_name(c->properties.deviceType);

        const char* reject_reason = NULL;
        if (!device_find_queues(c))           { reject_reason = "no suitable graphics/present queue family"; }
        else if (!device_check_extensions(c)) { reject_reason = "missing required device extensions"; }
        else if (!device_check_surface(c))    { reject_reason = "no usable surface formats/present modes"; }

        if (!reject_reason) {
            c->suitable = true;
            c->score    = device_score(c);
            printf("[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- suitable, score %d\n",
                i, c->properties.deviceName, type, vram_mb, c->score);
            tlPrintFormattedC(WHITE, "[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- suitable, score %d",
                i, c->properties.deviceName, type, vram_mb, c->score);tlNewLine();
        } else {
            printf("[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- rejected: %s\n",
                i, c->properties.deviceName, type, vram_mb, reject_reason);
            tlPrintFormattedC(WHITE, "[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- rejected: %s",
                i, c->properties.deviceName, type, vram_mb, reject_reason);tlNewLine();
        }
    }
    free(devices);

    DeviceCandidate* best = NULL;
    for (uint32_t i = 0; i < count; i++) {
        if (candidates[i].suitable && (!best || candidates[i].score > best->score)) { best = &candidates[i]; }
    }

    if (!best) {
        printf("[Vulkan] No suitable device found\n");
        tlPrint("[Vulkan] No suitable device found");tlNewLine();

        free(candidates);
        return -1;
    }

    g_physicalDevice = best->device;
    g_graphicsFamily = best->graphics_family;
    g_presentFamily  = best->present_family;

    const char* type = device_type_name(best->properties.deviceType);

    //FLOG("[Vulkan] Device: %s (%s, score %d)\n",
    //    best->properties.deviceName, type, best->score);
    //FLOG("[Vulkan] Queues: graphics=%u present=%u%s\n",
    //    g_graphicsFamily, g_presentFamily,
    //    g_graphicsFamily == g_presentFamily ? " (unified)" : " (separate)");
    printf("[Vulkan] Device: %s (%s, score %d)\n",
        best->properties.deviceName, type, best->score);
    tlPrintFormattedC(WHITE, "[Vulkan] Device: %s (%s, score %d)",
        best->properties.deviceName, type, best->score);tlNewLine();
    printf("[Vulkan] Queues: graphics=%u present=%u%s\n",
        g_graphicsFamily, g_presentFamily,
        g_graphicsFamily == g_presentFamily ? " (unified)" : " (separate)");
    tlPrintFormattedC(WHITE, "[Vulkan] Queues: graphics=%u present=%u%s",
        g_graphicsFamily, g_presentFamily,
        g_graphicsFamily == g_presentFamily ? " (unified)" : " (separate)");tlNewLine();

    free(candidates);

    return 0;
}

// Logical device
static int gdmf_create_logical_device(void) {
    float priority = 1.0f;

    VkDeviceQueueCreateInfo queue_infos[2];
    uint32_t queue_info_count = 0;

    queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = g_graphicsFamily,
        .queueCount       = 1,
        .pQueuePriorities = &priority,
    };

    if (g_presentFamily != g_graphicsFamily) {
        queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = g_presentFamily,
            .queueCount       = 1,
            .pQueuePriorities = &priority,
        };
    }

    const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo ci = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = queue_info_count,
        .pQueueCreateInfos       = queue_infos,
        .enabledExtensionCount   = 1,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures        = NULL
    };

    if (vkCreateDevice(g_physicalDevice, &ci, NULL, &g_vkDevice) != VK_SUCCESS) {
        printf("[Vulkan] Logical device creation failed\n");
        tlPrint("[Vulkan] Logical device creation failed");tlNewLine();

        return -1;
    }

    vkGetDeviceQueue(g_vkDevice, g_graphicsFamily, 0, &g_graphicsQueue);
    vkGetDeviceQueue(g_vkDevice, g_presentFamily,  0, &g_presentQueue);

    //FLOG("[Vulkan] Logical device created\n");
    printf("[Vulkan] Logical device created\n");
    tlPrint("[Vulkan] Logical device created");tlNewLine();

    return 0;
}

// Swapchain
static void gdmf_destroy_swapchain(void) {
    if (g_swapchainImageViews) {
        for (uint32_t i = 0; i < g_swapchainImageCount; i++)
            vkDestroyImageView(g_vkDevice, g_swapchainImageViews[i], NULL);
        free(g_swapchainImageViews);
        g_swapchainImageViews = NULL;
    }
    free(g_swapchainImages);
    g_swapchainImages     = NULL;
    g_swapchainImageCount = 0;

    if (g_vkSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_vkDevice, g_vkSwapchain, NULL);
        g_vkSwapchain = VK_NULL_HANDLE;
    }

    return;
}

static int gdmf_create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_vkSurface, &caps);

    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_vkSurface, &fmt_count, NULL);
    VkSurfaceFormatKHR* formats = malloc(fmt_count * sizeof(VkSurfaceFormatKHR));
    if (!formats) { return -1; }
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_vkSurface, &fmt_count, formats);

    // UNORM, not SRGB: every color that reaches the fragment shaders (palette
    // bytes, packed straight from Colors[256][16]) is already display-ready
    // 0-255 sRGB-encoded data, not linear. An _SRGB swapchain format would
    // have the hardware re-apply sRGB encoding on store, double-gamma-
    // correcting and washing out every color.
    VkSurfaceFormatKHR chosen_format = formats[0];
    for (uint32_t i = 0; i < fmt_count; i++) {
        if (formats[i].format     == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = formats[i];
            break;
        }
    }
    free(formats);

    // FIFO (vsync) for now; revisit when DICE controls frame pacing
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = (uint32_t)GDMFgetWidth();
        extent.height = (uint32_t)GDMFgetHeight();
        if (extent.width  < caps.minImageExtent.width)  { extent.width  = caps.minImageExtent.width; }
        if (extent.width  > caps.maxImageExtent.width)  { extent.width  = caps.maxImageExtent.width; }
        if (extent.height < caps.minImageExtent.height) { extent.height = caps.minImageExtent.height; }
        if (extent.height > caps.maxImageExtent.height) { extent.height = caps.maxImageExtent.height; }
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) { image_count = caps.maxImageCount; }

    uint32_t family_indices[] = { g_graphicsFamily, g_presentFamily };

    VkSwapchainCreateInfoKHR ci = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = g_vkSurface,
        .minImageCount    = image_count,
        .imageFormat      = chosen_format.format,
        .imageColorSpace  = chosen_format.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = present_mode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = VK_NULL_HANDLE
    };

    if (g_graphicsFamily != g_presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = family_indices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(g_vkDevice, &ci, NULL, &g_vkSwapchain) != VK_SUCCESS) {
        printf("[Vulkan] Swapchain creation failed\n");
        tlPrint("[Vulkan] Swapchain creation failed");tlNewLine();

        return -1;
    }

    g_swapchainFormat = chosen_format.format;
    g_swapchainExtent = extent;

    vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_swapchainImageCount, NULL);
    g_swapchainImages = malloc(g_swapchainImageCount * sizeof(VkImage));
    if (!g_swapchainImages) { return -1; }
    vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_swapchainImageCount, g_swapchainImages);

    g_swapchainImageViews = malloc(g_swapchainImageCount * sizeof(VkImageView));
    if (!g_swapchainImageViews) { free(g_swapchainImages); g_swapchainImages = NULL; return -1; }

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkImageViewCreateInfo view_ci = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image    = g_swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = g_swapchainFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };

        if (vkCreateImageView(g_vkDevice, &view_ci, NULL, &g_swapchainImageViews[i]) != VK_SUCCESS) {
            for (uint32_t j = 0; j < i; j++)
                vkDestroyImageView(g_vkDevice, g_swapchainImageViews[j], NULL);
            free(g_swapchainImageViews); g_swapchainImageViews = NULL;
            free(g_swapchainImages);     g_swapchainImages     = NULL;

            //FLOG("[Vulkan] Image view creation failed (index %u)\n", i);
            printf("[Vulkan] Image view creation failed (index %u)\n", i);
            tlPrintFormattedC(WHITE, "[Vulkan] Image view creation failed (index %u)\n", i);tlNewLine();

            return -1;
        }
    }

    //FLOG("[Vulkan] Swapchain: %ux%u, %u images\n",
    //    extent.width, extent.height, g_swapchainImageCount);
    printf("[Vulkan] Swapchain: %ux%u, %u images\n",
        extent.width, extent.height, g_swapchainImageCount);
    tlPrintFormattedC(WHITE, "[Vulkan] Swapchain: %ux%u, %u images",
        extent.width, extent.height, g_swapchainImageCount);tlNewLine();

    return 0;
}

// Render pass
static int gdmf_create_render_pass(void) {
    VkAttachmentDescription color_attachment = {
        .format         = g_swapchainFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_ref
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo ci = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &color_attachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency
    };

    if (vkCreateRenderPass(g_vkDevice, &ci, NULL, &g_renderPass) != VK_SUCCESS) {
        printf("[Vulkan] Render pass creation failed\n");
        tlPrint("[Vulkan] Render pass creation failed");tlNewLine();

        return -1;
    }

    //FLOG("[Vulkan] Render pass created\n");
    printf("[Vulkan] Render pass created\n");
    tlPrint("[Vulkan] Render pass created");tlNewLine();

    return 0;
}

static void gdmf_destroy_render_pass(void) {
    if (g_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vkDevice, g_renderPass, NULL);
        g_renderPass = VK_NULL_HANDLE;
    }

    return;
}

// Framebuffers
static void gdmf_destroy_framebuffers(void) {
    if (!g_framebuffers) { return; }
    for (uint32_t i = 0; i < g_swapchainImageCount; i++)
        vkDestroyFramebuffer(g_vkDevice, g_framebuffers[i], NULL);
    free(g_framebuffers);
    g_framebuffers = NULL;

    return;
}

static int gdmf_create_framebuffers(void) {
    g_framebuffers = malloc(g_swapchainImageCount * sizeof(VkFramebuffer));
    if (!g_framebuffers) { return -1; }

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkFramebufferCreateInfo ci = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = g_renderPass,
            .attachmentCount = 1,
            .pAttachments    = &g_swapchainImageViews[i],
            .width           = g_swapchainExtent.width,
            .height          = g_swapchainExtent.height,
            .layers          = 1
        };

        if (vkCreateFramebuffer(g_vkDevice, &ci, NULL, &g_framebuffers[i]) != VK_SUCCESS) {
            for (uint32_t j = 0; j < i; j++)
                vkDestroyFramebuffer(g_vkDevice, g_framebuffers[j], NULL);
            free(g_framebuffers);
            g_framebuffers = NULL;

            //FLOG("[Vulkan] Framebuffer creation failed (index %u)\n", i);
            printf("[Vulkan] Framebuffer creation failed (index %u)\n", i);
            tlPrintFormattedC(WHITE, "[Vulkan] Framebuffer creation failed (index %u)", i);tlNewLine();

            return -1;
        }
    }

    //FLOG("[Vulkan] %u framebuffers created\n", g_swapchainImageCount);
    printf("[Vulkan] %u framebuffers created\n", g_swapchainImageCount);
    tlPrintFormattedC(WHITE, "[Vulkan] %u framebuffers created", g_swapchainImageCount);tlNewLine();

    return 0;
}

// Command pool (stable -- survives swapchain recreation)
static int gdmf_create_command_pool(void) {
    VkCommandPoolCreateInfo ci = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = g_graphicsFamily
    };

    if (vkCreateCommandPool(g_vkDevice, &ci, NULL, &g_commandPool) != VK_SUCCESS) {
        printf("[Vulkan] Command pool creation failed\n");
        tlPrint("[Vulkan] Command pool creation failed");tlNewLine();

        return -1;
    }

    //FLOG("[Vulkan] Command pool created\n");
    printf("[Vulkan] Command pool created\n");
    tlPrint("[Vulkan] Command pool created");tlNewLine();

    return 0;
}

// Per-image objects (command buffers, semaphores, fences)
// Destroyed and recreated whenever the swapchain changes image count.
static void gdmf_destroy_per_image_objects(void) {
    if (g_commandBuffers && g_commandPool != VK_NULL_HANDLE && g_swapchainImageCount > 0) { vkFreeCommandBuffers(g_vkDevice, g_commandPool, g_swapchainImageCount, g_commandBuffers); }
    free(g_commandBuffers);
    g_commandBuffers = NULL;

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        if (g_perImageAcquire && g_perImageAcquire[i] != VK_NULL_HANDLE) { vkDestroySemaphore(g_vkDevice, g_perImageAcquire[i], NULL); }
        if (g_renderFinished && g_renderFinished[i] != VK_NULL_HANDLE) { vkDestroySemaphore(g_vkDevice, g_renderFinished[i], NULL); }
        if (g_inFlightFence && g_inFlightFence[i] != VK_NULL_HANDLE) { vkDestroyFence(g_vkDevice, g_inFlightFence[i], NULL); }
    }
    if (g_acquireSpare != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_vkDevice, g_acquireSpare, NULL);
        g_acquireSpare = VK_NULL_HANDLE;
    }
    free(g_perImageAcquire); g_perImageAcquire = NULL;
    free(g_renderFinished);  g_renderFinished  = NULL;
    free(g_inFlightFence);   g_inFlightFence   = NULL;

    return;
}

static int gdmf_create_per_image_objects(void) {
    // calloc, not malloc: gdmf_destroy_per_image_objects() (also used as
    // this function's own failure cleanup, below) decides what to destroy
    // by checking each entry against VK_NULL_HANDLE. That guard is only
    // safe if every entry this function doesn't reach is genuinely zero --
    // malloc leaves it as garbage, which would hand a garbage "handle" to
    // vkDestroySemaphore/vkDestroyFence on a later failed-then-retried
    // recreate or on shutdown.
    g_commandBuffers  = calloc(g_swapchainImageCount, sizeof(VkCommandBuffer));
    g_perImageAcquire = calloc(g_swapchainImageCount, sizeof(VkSemaphore));
    g_renderFinished  = calloc(g_swapchainImageCount, sizeof(VkSemaphore));
    g_inFlightFence   = calloc(g_swapchainImageCount, sizeof(VkFence));

    if (!g_commandBuffers || !g_perImageAcquire || !g_renderFinished || !g_inFlightFence) {
        printf("[Vulkan] Out of memory for per-image objects\n");
        tlPrint("[Vulkan] Out of memory for per-image objects");tlNewLine();

        goto fail;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = g_swapchainImageCount
    };
    VK_CHECK(vkAllocateCommandBuffers(g_vkDevice, &alloc_info, g_commandBuffers));

    VkSemaphoreCreateInfo sem_ci   = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT  // pre-signaled so first wait returns immediately
    };

    VK_CHECK(vkCreateSemaphore(g_vkDevice, &sem_ci, NULL, &g_acquireSpare));

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VK_CHECK(vkCreateSemaphore(g_vkDevice, &sem_ci,   NULL, &g_perImageAcquire[i]));
        VK_CHECK(vkCreateSemaphore(g_vkDevice, &sem_ci,   NULL, &g_renderFinished[i]));
        VK_CHECK(vkCreateFence    (g_vkDevice, &fence_ci, NULL, &g_inFlightFence[i]));
    }

    //FLOG("[Vulkan] Per-image objects ready (%u images)\n", g_swapchainImageCount);
    printf("[Vulkan] Per-image objects ready (%u images)\n", g_swapchainImageCount);
    tlPrintFormattedC(WHITE, "[Vulkan] Per-image objects ready (%u images)", g_swapchainImageCount);tlNewLine();

    return 0;

fail:
    // Reuses the regular teardown path -- safe to call on a partially
    // (or even completely un-) created set, since every entry is either a
    // real handle or a guaranteed-zero calloc'd slot.
    gdmf_destroy_per_image_objects();

    return -1;
}

static void gdmf_destroy_palette_buffers(void) {
    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        if (g_paletteBuffers && g_paletteBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_vkDevice, g_paletteBuffers[i], NULL);
        }
        if (g_paletteMemories && g_paletteMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(g_vkDevice, g_paletteMemories[i], NULL);
        }
    }
    free(g_paletteBuffers);  g_paletteBuffers  = NULL;
    free(g_paletteMemories); g_paletteMemories = NULL;

    return;
}

static int gdmf_create_palette_buffers(void) {
    // calloc, not malloc -- same reasoning as gdmf_create_per_image_objects:
    // gdmf_destroy_palette_buffers() (also this function's own failure
    // cleanup) decides what to destroy by checking each entry against
    // VK_NULL_HANDLE, which is only safe if every slot this function
    // doesn't reach is genuinely zero.
    g_paletteBuffers  = calloc(g_swapchainImageCount, sizeof(VkBuffer));
    g_paletteMemories = calloc(g_swapchainImageCount, sizeof(VkDeviceMemory));
    if (!g_paletteBuffers || !g_paletteMemories) {
        printf("[Vulkan] Out of memory for palette buffers\n");
        tlPrint("[Vulkan] Out of memory for palette buffers");tlNewLine();

        goto fail;
    }

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkBufferCreateInfo buf_info = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = GDMF_PALETTE_BUFFER_SIZE,
            .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VK_CHECK(vkCreateBuffer(g_vkDevice, &buf_info, NULL, &g_paletteBuffers[i]));

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(g_vkDevice, g_paletteBuffers[i], &mem_req);
        VkMemoryAllocateInfo alloc_info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = mem_req.size,
            .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        if (alloc_info.memoryTypeIndex == UINT32_MAX) {
            printf("[Vulkan] No suitable memory type for palette buffer %u\n", i);
            goto fail;
        }
        VK_CHECK(vkAllocateMemory(g_vkDevice, &alloc_info, NULL, &g_paletteMemories[i]));
        vkBindBufferMemory(g_vkDevice, g_paletteBuffers[i], g_paletteMemories[i], 0);
    }

    printf("[Vulkan] %u palette buffers ready\n", g_swapchainImageCount);
    tlPrintFormattedC(WHITE, "[Vulkan] %u palette buffers ready", g_swapchainImageCount);tlNewLine();

    return 0;

fail:
    gdmf_destroy_palette_buffers();

    return -1;
}

// Re-uploads the full palette table (Colors[256][16], PackRGBA8 per entry)
// to this frame's shared buffer. Called once per frame, before any
// subsystem's own prepare() -- sprites/tiles just bind this same buffer
// in their own descriptor set instead of each maintaining a redundant
// private copy. Colors[] can change any frame via SetPalette with no
// dirty tracking, so this always re-uploads in full -- 16 KiB, trivial
// bandwidth, same reasoning the old per-subsystem copies used, just paid
// once per frame instead of once per subsystem (and, for tiles, once per
// active layer on top of that).
static void gdmf_palette_prepare(uint32_t imageIndex) {
    if (imageIndex >= g_swapchainImageCount || !g_paletteMemories) { return; }

    void* mapped;
    if (vkMapMemory(g_vkDevice, g_paletteMemories[imageIndex], 0, GDMF_PALETTE_BUFFER_SIZE, 0, &mapped) != VK_SUCCESS) {
        return;
    }
    uint32_t* dst = (uint32_t*)mapped;
    for (int pal = 0; pal < 256; pal++) {
        for (int idx = 0; idx < 16; idx++) {
            dst[pal * 16 + idx] = PackRGBA8(Colors[pal][idx]);
        }
    }
    vkUnmapMemory(g_vkDevice, g_paletteMemories[imageIndex]);

    return;
}

// Swapchain recreation
static int gdmf_recreate_swapchain(void) {
    if (GDMFgetWidth() == 0 || GDMFgetHeight() == 0) { return 0; }  // minimized

    VkFormat oldFormat = g_swapchainFormat;

    vkDeviceWaitIdle(g_vkDevice);
    gdmf_destroy_palette_buffers();
    gdmf_destroy_per_image_objects();
    gdmf_destroy_framebuffers();
    gdmf_destroy_swapchain();
    if (gdmf_create_swapchain() != 0) { return -1; }

    // The render pass was created against the old swapchain format. Usually
    // a new surface format matches the old one, but that's not guaranteed
    // (display change, HDR transition, driver offering a different format)
    // -- if it doesn't, the render pass is no longer compatible with the new
    // images and must be rebuilt before framebuffers reference it again.
    if (g_swapchainFormat != oldFormat) {
        gdmf_destroy_render_pass();
        if (gdmf_create_render_pass() != 0) { return -1; }
    }

    if (gdmf_create_framebuffers()      != 0) { return -1; }
    if (gdmf_create_per_image_objects() != 0) { return -1; }
    if (gdmf_create_palette_buffers()   != 0) { return -1; }

    // Subsystem pipelines/frame resources may depend on the render pass
    // (rebuilt above, if the format changed) or on the swapchain image
    // count (which can also change independently of the format) -- treat
    // every recreation as an invalidation event rather than assuming
    // either one is still the same as when the pipeline was first built.
    gdmf_sprites_on_swapchain_recreated();
    gdmf_tiles_on_swapchain_recreated();
    gdmf_pixies_on_swapchain_recreated();
    gdmf_textlayer_on_swapchain_recreated();

    //FLOG("[Vulkan] Swapchain recreated\n");
    printf("[Vulkan] Swapchain recreated\n");
    tlPrint("[Vulkan] Swapchain recreated");tlNewLine();

    return 0;
}

// Render frame
void gdmf_vulkan_render_frame(void) {
    if (GDMFisMinimized()) { return; }

    if (GDMFresizeOccurred()) { g_needsSwapchainRecreate = true; }

    if (g_needsSwapchainRecreate) {
        g_needsSwapchainRecreate = false;
        gdmf_recreate_swapchain();
        return;
    }

    if (g_swapchainExtent.width == 0 || g_swapchainExtent.height == 0) { return; }

    // Acquire next image using the rotating spare semaphore
    VkSemaphore acquire_sem = g_acquireSpare;
    uint32_t    image_index;
    VkResult    result = vkAcquireNextImageKHR(
        g_vkDevice, g_vkSwapchain, UINT64_MAX,
        acquire_sem, VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        g_needsSwapchainRecreate = true;
        return;
    }
    if (result == VK_SUBOPTIMAL_KHR) { g_needsSwapchainRecreate = true; }  // continue this frame, recreate next

    // Swap: acquired sem becomes the per-image sem; old per-image sem becomes the spare
    g_acquireSpare             = g_perImageAcquire[image_index];
    g_perImageAcquire[image_index] = acquire_sem;

    // Wait for previous rendering to this image slot to finish
    vkWaitForFences(g_vkDevice, 1, &g_inFlightFence[image_index], VK_TRUE, UINT64_MAX);
    vkResetFences  (g_vkDevice, 1, &g_inFlightFence[image_index]);

    // Record
    VkCommandBuffer cmd = g_commandBuffers[image_index];
    VK_LOG_IF_FAILED(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_LOG_IF_FAILED(vkBeginCommandBuffer(cmd, &begin_info));

    // Shared palette upload -- once per frame, before any subsystem's own
    // prepare(). Sprites/tiles bind this same buffer in their own
    // descriptor sets rather than each re-uploading their own copy.
    gdmf_palette_prepare(image_index);

    // Subsystem prepare pass: fill vertex/CPU data before the render pass opens
    gdmf_sprites_prepare(image_index);
    gdmf_tiles_prepare(image_index);
    gdmf_pixies_prepare(image_index);
    gdmf_textlayer_prepare(image_index);

    VkClearValue clear_color = { .color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} } };
    VkRenderPassBeginInfo rp_begin = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass      = g_renderPass,
        .framebuffer     = g_framebuffers[image_index],
        .renderArea      = { .offset = {0, 0}, .extent = g_swapchainExtent },
        .clearValueCount = 1,
        .pClearValues    = &clear_color
    };
    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    // Interleaved tile/sprite/pixie draw, back-to-front: tile layer 15 is
    // furthest back, tile layer 0 is closest. Each tile layer is followed by
    // the sprite priority band in front of it, then the pixie priority band
    // in front of that -- band N covers priorities [N*16, N*16+15]. Pixies
    // at priority 0 draw last (in front of everything). All three functions
    // are no-ops when their layer/band is inactive.
    for (int band = (int)MAX_TILE_LAYERS - 1; band >= 0; band--) {
        gdmf_tiles_record_layer(cmd, image_index, (uint8_t)band);
        gdmf_sprites_record_band(cmd, image_index, (uint8_t)band);
        gdmf_pixies_record_band(cmd, image_index, (uint8_t)band);
    }
    gdmf_textlayer_record(cmd, image_index);

    vkCmdEndRenderPass(cmd);
    VK_LOG_IF_FAILED(vkEndCommandBuffer(cmd));

    // Submit
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &g_perImageAcquire[image_index],
        .pWaitDstStageMask    = &wait_stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &g_renderFinished[image_index]
    };
    VK_LOG_IF_FAILED(vkQueueSubmit(g_graphicsQueue, 1, &submit_info, g_inFlightFence[image_index]));

    // Present
    VkPresentInfoKHR present_info = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &g_renderFinished[image_index],
        .swapchainCount     = 1,
        .pSwapchains        = &g_vkSwapchain,
        .pImageIndices      = &image_index
    };
    result = vkQueuePresentKHR(g_presentQueue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) { g_needsSwapchainRecreate = true; }
    else if (result != VK_SUCCESS) { printf("[Vulkan] vkQueuePresentKHR failed: %d\n", result); }

    return;
}

// Internal utilities (gdmf_vulkan_internal.h)
VkDevice         gdmf_get_device(void)          { return g_vkDevice; }
VkPhysicalDevice gdmf_get_physical_device(void) { return g_physicalDevice; }
VkQueue          gdmf_get_graphics_queue(void)  { return g_graphicsQueue; }
VkCommandPool    gdmf_get_command_pool(void)    { return g_commandPool; }
VkRenderPass     gdmf_get_render_pass(void)     { return g_renderPass; }
VkExtent2D       gdmf_get_swapchain_extent(void){ return g_swapchainExtent; }
uint32_t         gdmf_get_swapchain_image_count(void) { return g_swapchainImageCount; }

// Centered, aspect-correct sub-rectangle of the current swapchain extent
// that rendering should target -- the fixed-aspect logical canvas sprites.c
// and gdmf_textlayer.c render onto (see SPRITE_REFERENCE_CANVAS_WIDTH/HEIGHT)
// always maps straight to this rect's NDC space rather than the swapchain's
// full extent, so a window/monitor shape that doesn't match the design
// aspect ratio (GDMFsetAspectRatio) never stretches the image -- the
// untouched border, already cleared to black by the render pass, becomes
// the letterbox/pillarbox bars instead.
VkRect2D gdmf_get_render_viewport_rect(void) {
    int aspectNum = GDMFgetAspectRatioNum();
    int aspectDen = GDMFgetAspectRatioDen();

    int ew = (int)g_swapchainExtent.width;
    int eh = (int)g_swapchainExtent.height;

    int w = ew;
    int h = (w * aspectDen) / aspectNum;

    if (h > eh) {
        h = eh;
        w = (h * aspectNum) / aspectDen;
    }

    return (VkRect2D){
        .offset = { (ew - w) / 2, (eh - h) / 2 },
        .extent = { (uint32_t)w, (uint32_t)h },
    };
}

VkBuffer gdmf_get_palette_buffer(uint32_t imageIndex) {
    if (!g_paletteBuffers || imageIndex >= g_swapchainImageCount) {
        return VK_NULL_HANDLE;
    }

    return g_paletteBuffers[imageIndex];
}

uint32_t gdmfFindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;

    vkGetPhysicalDeviceMemoryProperties(g_physicalDevice, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) { return i; }
    }

    return UINT32_MAX;
}

int gdmfExecuteOneTimeCommands(GDMFCommandRecordFunc record_func, void* user_data) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VK_CHECK(vkAllocateCommandBuffers(g_vkDevice, &alloc_info, &cmd));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    record_func(cmd, user_data);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(g_graphicsQueue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(g_graphicsQueue));

    vkFreeCommandBuffers(g_vkDevice, g_commandPool, 1, &cmd);
    return 0;

fail:
    // cmd may still be VK_NULL_HANDLE if the allocate itself is what
    // failed -- vkFreeCommandBuffers ignores VK_NULL_HANDLE entries, so
    // this is safe either way.
    vkFreeCommandBuffers(g_vkDevice, g_commandPool, 1, &cmd);

    return -1;
}

// Debug messenger (DEBUG builds only)
#ifdef DEBUG

static VKAPI_ATTR VkBool32 VKAPI_CALL gdmf_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT     severity,
    VkDebugUtilsMessageTypeFlagsEXT            type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       user) {
    (void)type; (void)user;
    const char* prefix =
        (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   ? "[Vulkan ERROR]"   :
        (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "[Vulkan WARNING]" :
                                                                         "[Vulkan INFO]";

        //FLOG("%s %s\n", prefix, data->pMessage);
        printf("%s %s\n", prefix, data->pMessage);
        tlPrintFormattedC(WHITE, "%s %s", prefix, data->pMessage);tlNewLine();

    return VK_FALSE;
}

static int gdmf_create_debug_messenger(void) {
    PFN_vkCreateDebugUtilsMessengerEXT pfn_create =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            g_vkInstance, "vkCreateDebugUtilsMessengerEXT");

    if (!pfn_create) {
        printf("[Vulkan] vkCreateDebugUtilsMessengerEXT not available\n");
        tlPrint("[Vulkan] vkCreateDebugUtilsMessengerEXT not available");tlNewLine();

        return -1;
    }

    VkDebugUtilsMessengerCreateInfoEXT ci = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = gdmf_debug_callback
    };

    if (pfn_create(g_vkInstance, &ci, NULL, &g_debugMessenger) != VK_SUCCESS) {
        printf("[Vulkan] Debug messenger creation failed\n");
        tlPrint("[Vulkan] Debug messenger creation failed");tlNewLine();

        return -1;
    }

    //FLOG("[Vulkan] Debug messenger active\n");
    printf("[Vulkan] Debug messenger active\n");
    tlPrint("[Vulkan] Debug messenger active");tlNewLine();

    return 0;
}

static void gdmf_destroy_debug_messenger(void) {
    if (g_debugMessenger == VK_NULL_HANDLE) { return; }
    PFN_vkDestroyDebugUtilsMessengerEXT pfn_destroy =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            g_vkInstance, "vkDestroyDebugUtilsMessengerEXT");
    if (pfn_destroy) { pfn_destroy(g_vkInstance, g_debugMessenger, NULL); }
    g_debugMessenger = VK_NULL_HANDLE;

    return;
}

#endif // DEBUG