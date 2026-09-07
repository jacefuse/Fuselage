// GDMF -- Vulkan subsystem
// Instance, surface, device, swapchain, render pass, framebuffers, render loop.
// Platform-neutral: surface creation is delegated to gdmf_create_platform_surface()
// (see gdmf_surface_win32.c), so this file has no VK_USE_PLATFORM_*_KHR define
// and no platform-specific Vulkan types of its own.

#include "gdmf.h"
#include <vulkan/vulkan.h>
#include "gdmf_vulkan.h"
#include "gdmf_vulkan_internal.h"
#include "fuselage_sync.h"   // pure utility (mutex); not a subsystem dependency
#include "shaders/mask_composite_vert.h"
#include "shaders/mask_composite_frag.h"
//#include "gdmf_textlayer.h"
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

// VkQueue access must be externally synchronized (Vulkan spec): no two threads
// may touch the same queue at once. The render thread submits + presents every
// frame in gdmf_vulkan_submit_frame(), while the sim thread (game() logic) can
// fire one-time upload commands concurrently -- UploadSpriteBitmap, tile/pixie
// uploads -- all through gdmfExecuteOneTimeCommands(). Those hit
// g_graphicsQueue / g_presentQueue (frequently the same underlying queue), so
// every vkQueueSubmit / vkQueuePresentKHR / vkQueueWaitIdle is serialized
// through this one lock. Initialized in gdmf_create_logical_device() where the
// queues are obtained; destroyed in gdmf_vulkan_shutdown(). The init guard lets
// the helpers no-op for any queue work that runs before the lock exists (engine
// bring-up is single-threaded until the sim timer starts).
static FuselageMutex g_queueLock;
static bool          g_queueLockReady = false;

static void gdmf_queue_lock(void)   { if (g_queueLockReady) { fuselage_mutex_lock(&g_queueLock); } }
static void gdmf_queue_unlock(void) { if (g_queueLockReady) { fuselage_mutex_unlock(&g_queueLock); } }

// Serializes whole gdmfExecuteOneTimeCommands bodies against each other.
// Command pools have the same external-synchronization rule queues do, and
// one-time commands can be recorded from two different threads (atlas
// init-layouts on the main thread during bring-up, mid-game uploads on the
// sim thread, lazy pixie image creation on the render thread inside
// prepare) -- so all one-time work funnels through its own dedicated pool
// (g_uploadCommandPool below) guarded by this lock, and never touches
// g_commandPool, which belongs to the render thread's per-frame recording
// alone. Same init-in-create-logical-device / no-op-before-ready pattern
// as g_queueLock above. Lock order where both are held: upload lock first,
// queue lock inside it -- the render thread only ever takes the queue
// lock, so no cycle exists.
static FuselageMutex g_uploadLock;
static bool          g_uploadLockReady = false;

static void gdmf_upload_lock(void)   { if (g_uploadLockReady) { fuselage_mutex_lock(&g_uploadLock); } }
static void gdmf_upload_unlock(void) { if (g_uploadLockReady) { fuselage_mutex_unlock(&g_uploadLock); } }

// Deferred-destruction queue (see gdmf_vulkan_internal.h's doc comment on
// the gdmf_defer_* functions). Entries are pushed from any thread under
// g_deferLock and destroyed on the render thread: one countdown tick per
// completed frame (gdmf_defer_tick, from prepare after the fence wait), or
// all at once when the device is known idle with no recorded frame pending
// (gdmf_defer_flush, from swapchain recreation and shutdown -- both run on
// the render/main thread at points where prepare has recorded nothing yet).
// framesLeft starts at swapchainImageCount + 1: at most swapchainImageCount
// frames can be submitted-but-incomplete (one per image, retired oldest-
// first since fences signal in submission order), plus the one frame that
// may be recorded but not yet submitted when the push happens.
typedef enum {
    GDMF_DEFER_BUFFER,
    GDMF_DEFER_IMAGE,
    GDMF_DEFER_DESCRIPTOR_SET,
    GDMF_DEFER_DESCRIPTOR_POOL,
    GDMF_DEFER_SAMPLER
} GdmfDeferKind;

typedef struct GdmfDeferEntry {
    struct GdmfDeferEntry* next;
    uint32_t               framesLeft;
    GdmfDeferKind          kind;
    union {
        struct { VkBuffer buffer; VkImage image; VkImageView view; VkDeviceMemory memory; } res;
        struct { VkDescriptorPool pool; VkDescriptorSet set; } dset;
        VkDescriptorPool dpool;
        VkSampler        sampler;
    } u;
} GdmfDeferEntry;

static GdmfDeferEntry* g_deferList = NULL;
static FuselageMutex   g_deferLock;
static bool            g_deferLockReady = false;

static void gdmf_defer_tick(void);
static void gdmf_defer_flush(void);

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
static VkCommandPool    g_uploadCommandPool = VK_NULL_HANDLE; // one-time commands only, guarded by g_uploadLock

// Device-lifetime pipeline cache shared by every vkCreateGraphicsPipelines
// call in the engine (sprites/tiles/pixies/text/mask compositor) -- repeat
// builds of the same pipeline (lazy rebuilds after swapchain recreation,
// mostly) hit the driver's cache instead of recompiling. Purely an
// optimization: creation failure just leaves it VK_NULL_HANDLE, which
// vkCreateGraphicsPipelines accepts as "no cache". Not persisted to disk.
static VkPipelineCache  g_pipelineCache    = VK_NULL_HANDLE;

// Memory properties of the picked physical device, cached once during
// device selection -- gdmfFindMemoryType is called from several threads
// (render prepare, sim uploads) and the properties never change for the
// lifetime of a picked device, so there's no reason to re-query per call.
static VkPhysicalDeviceMemoryProperties g_memProperties;
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
static void**           g_paletteMapped   = NULL;  // [swapchainImageCount] -- persistently mapped at creation
static uint32_t*        g_paletteUploadedGeneration = NULL;  // [swapchainImageCount] -- last Colors[] generation written; 0 = never

static bool             g_needsSwapchainRecreate = false;

// Set once vkQueueSubmit/vkQueuePresentKHR reports VK_ERROR_DEVICE_LOST --
// unlike other per-frame VkResult failures (see VK_LOG_IF_FAILED), this one
// is not transient and nothing later in the frame loop will resolve it: the
// GPU driver has reset the device, every fence/semaphore tied to it is in an
// undefined state, and every further Vulkan call on it will keep failing
// the same way. Read by gdmf_vulkan_device_lost() (GDMF_Tick() folds it into
// its alive check) so the render loop stops submitting to the dead device
// after the first failure instead of hammering it every frame forever --
// that retry storm is what was observed running up to an actual BSOD rather
// than a clean VK_ERROR_DEVICE_LOST failure.
static volatile bool    g_deviceLost = false;

// Set by gdmf_vulkan_prepare_frame() once it's acquired an image and
// finished the subsystem _prepare() calls -- gdmf_vulkan_submit_frame()
// checks this rather than assuming there's always something to record,
// since prepare legitimately has nothing to do some ticks (minimized,
// mid swapchain-recreate, zero-sized window, out-of-date swapchain).
static bool             g_framePending      = false;
static uint32_t         g_pendingImageIndex  = 0;
static VkCommandBuffer  g_pendingCmd         = VK_NULL_HANDLE;

// See GDMF_SetVSync's doc comment in gdmf.h. Read by gdmf_create_swapchain
// when choosing a present mode; defaults to on (matches the previous
// hardcoded-FIFO behavior for anyone not opting in).
static bool             g_vsyncEnabled = true;

void GDMF_SetVSync(bool enabled) {
    if (g_vsyncEnabled == enabled) { return; }

    g_vsyncEnabled           = enabled;
    g_needsSwapchainRecreate = true;

    return;
}

bool GDMF_GetVSync(void) { return g_vsyncEnabled; }

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

// Masked compositor (defined near gdmf_vulkan_submit_frame). Forward-
// declared so shutdown and swapchain recreation, both above it, can tear it
// down; it rebuilds lazily on the next frame a mask is present.
static void gdmf_mask_compositor_destroy(void);

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
    gdmf_device_wait_idle();

    // Device idle + render loop stopped (no recorded frame pending) -- safe
    // to destroy everything still sitting in the deferred queue.
    gdmf_defer_flush();

    gdmf_mask_compositor_destroy();
    gdmf_destroy_palette_buffers();
    gdmf_destroy_per_image_objects();

    if (g_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vkDevice, g_commandPool, NULL);
        g_commandPool = VK_NULL_HANDLE;
    }
    if (g_uploadCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_vkDevice, g_uploadCommandPool, NULL);
        g_uploadCommandPool = VK_NULL_HANDLE;
    }

    gdmf_destroy_framebuffers();
    gdmf_destroy_render_pass();
    gdmf_destroy_swapchain();

    if (g_pipelineCache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(g_vkDevice, g_pipelineCache, NULL);
        g_pipelineCache = VK_NULL_HANDLE;
    }
    if (g_vkDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(g_vkDevice, NULL);
        g_vkDevice = VK_NULL_HANDLE;
    }
    if (g_queueLockReady) {
        g_queueLockReady = false;
        fuselage_mutex_destroy(&g_queueLock);
    }
    if (g_uploadLockReady) {
        g_uploadLockReady = false;
        fuselage_mutex_destroy(&g_uploadLock);
    }
    if (g_deferLockReady) {
        g_deferLockReady = false;
        fuselage_mutex_destroy(&g_deferLock);
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

    printf("[Vulkan] Shutdown\n");
    //tlPrint("[Vulkan] Shutdown");tlNewLine();

    return;
}

// Instance
static int gdmf_create_instance(void) {
    uint32_t loader_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion pfn_enumerate_version =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");

    if (pfn_enumerate_version) { pfn_enumerate_version(&loader_version); }

    if (loader_version < VK_API_VERSION_1_3) {
        printf("[Vulkan] Loader reports %u.%u -- Vulkan 1.3 required\n",
            VK_API_VERSION_MAJOR(loader_version),
            VK_API_VERSION_MINOR(loader_version));
        //tlPrintFormatted("[Vulkan] Loader reports %u.%u -- Vulkan 1.3 required",
        //    VK_API_VERSION_MAJOR(loader_version),
        //    VK_API_VERSION_MINOR(loader_version));tlNewLine();

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
            printf("[Vulkan] Missing required extension: %s\n", required_exts[r]);
            //tlPrintFormatted("[Vulkan] Missing required extension: %s", required_exts[r]);tlNewLine();

            free(avail_exts);
            return -1;
        }
    }

    const char* enabled_exts[5];
    uint32_t    enabled_ext_count = 0;
    VkInstanceCreateFlags instance_flags = 0;
    for (uint32_t i = 0; i < required_ext_count; i++)
        enabled_exts[enabled_ext_count++] = required_exts[i];

#ifdef VK_KHR_portability_enumeration
    // Portability drivers (MoltenVK on macOS) are hidden from enumeration
    // unless the instance explicitly opts in. Query-based, not #ifdef-per-
    // platform: on a fully conformant driver the extension simply isn't
    // advertised and nothing changes.
    for (uint32_t i = 0; i < avail_ext_count; i++) {
        if (strcmp(avail_exts[i].extensionName,
                   VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            enabled_exts[enabled_ext_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            break;
        }
    }
#endif

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
    printf("[Vulkan] VK_EXT_debug_utils not available -- debug messenger disabled\n");
    //tlPrint("[Vulkan] VK_EXT_debug_utils not available -- debug messenger disabled");tlNewLine();

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
        printf("[Vulkan] Validation layer enabled\n");
        //tlPrint("[Vulkan] Validation layer enabled");tlNewLine();
    }
        else {
        printf("[Vulkan] VK_LAYER_KHRONOS_validation not available\n");
        //tlPrint("[Vulkan] VK_LAYER_KHRONOS_validation not available");tlNewLine();
    }

#endif

    VkApplicationInfo app_info = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Fuselage",
        .applicationVersion = VK_MAKE_VERSION(0, 3, 0),
        .pEngineName        = "Fuselage GDMF",
        .engineVersion      = VK_MAKE_VERSION(0, 3, 0),
        .apiVersion         = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo ci = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags                   = instance_flags,
        .pApplicationInfo        = &app_info,
        .enabledExtensionCount   = enabled_ext_count,
        .ppEnabledExtensionNames = enabled_exts,
        .enabledLayerCount       = enabled_layer_count,
        .ppEnabledLayerNames     = enabled_layer_count ? enabled_layers : NULL
    };

    if (vkCreateInstance(&ci, NULL, &g_vkInstance) != VK_SUCCESS) {
        printf("[Vulkan] vkCreateInstance failed\n");
        //tlPrint("[Vulkan] vkCreateInstance failed");tlNewLine();

        return -1;
    }

    printf("[Vulkan] Instance created (API 1.3)\n");
    //tlPrint("[Vulkan] Instance created (API 1.3)");tlNewLine();

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
        //tlPrint("[Vulkan] No Vulkan-capable devices found");tlNewLine();

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
            //tlPrintFormattedC(WHITE, "[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- suitable, score %d",
            //    i, c->properties.deviceName, type, vram_mb, c->score);tlNewLine();
        } else {
            printf("[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- rejected: %s\n",
                i, c->properties.deviceName, type, vram_mb, reject_reason);
            //tlPrintFormattedC(WHITE, "[Vulkan] Candidate %u: %s (%s, %lluMB VRAM) -- rejected: %s",
            //    i, c->properties.deviceName, type, vram_mb, reject_reason);tlNewLine();
        }
    }
    free(devices);

    DeviceCandidate* best = NULL;
    for (uint32_t i = 0; i < count; i++) {
        if (candidates[i].suitable && (!best || candidates[i].score > best->score)) { best = &candidates[i]; }
    }

    if (!best) {
        printf("[Vulkan] No suitable device found\n");
        //tlPrint("[Vulkan] No suitable device found");tlNewLine();

        free(candidates);
        return -1;
    }

    g_physicalDevice = best->device;
    g_graphicsFamily = best->graphics_family;
    g_presentFamily  = best->present_family;
    g_memProperties  = best->memory_properties;  // cached for gdmfFindMemoryType -- see its doc comment

    const char* type = device_type_name(best->properties.deviceType);

    printf("[Vulkan] Device: %s (%s, score %d)\n",
        best->properties.deviceName, type, best->score);
    tlPrintFormattedC(LIGHTGRAY, "[Vulkan] Device: %s (%s, score %d)",
        best->properties.deviceName, type, best->score);tlNewLine();
    printf("[Vulkan] Queues: graphics=%u present=%u%s\n",
        g_graphicsFamily, g_presentFamily,
        g_graphicsFamily == g_presentFamily ? " (unified)" : " (separate)");
    //tlPrintFormattedC(WHITE, "[Vulkan] Queues: graphics=%u present=%u%s",
    //    g_graphicsFamily, g_presentFamily,
    //    g_graphicsFamily == g_presentFamily ? " (unified)" : " (separate)");tlNewLine();

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

    const char* extensions[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    uint32_t    extension_count = 1;

    // A portability-subset device (MoltenVK) is REQUIRED by spec to have
    // VK_KHR_portability_subset enabled when it advertises it. Query-based,
    // same reasoning as the instance-level portability opt-in above.
    {
        uint32_t avail_count = 0;
        vkEnumerateDeviceExtensionProperties(g_physicalDevice, NULL, &avail_count, NULL);
        VkExtensionProperties* avail = malloc(avail_count * sizeof(VkExtensionProperties));
        if (avail) {
            vkEnumerateDeviceExtensionProperties(g_physicalDevice, NULL, &avail_count, avail);
            for (uint32_t i = 0; i < avail_count; i++) {
                if (strcmp(avail[i].extensionName, "VK_KHR_portability_subset") == 0) {
                    extensions[extension_count++] = "VK_KHR_portability_subset";
                    break;
                }
            }
            free(avail);
        }
    }

    VkDeviceCreateInfo ci = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = queue_info_count,
        .pQueueCreateInfos       = queue_infos,
        .enabledExtensionCount   = extension_count,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures        = NULL
    };

    if (vkCreateDevice(g_physicalDevice, &ci, NULL, &g_vkDevice) != VK_SUCCESS) {
        printf("[Vulkan] Logical device creation failed\n");
        //tlPrint("[Vulkan] Logical device creation failed");tlNewLine();

        return -1;
    }

    vkGetDeviceQueue(g_vkDevice, g_graphicsFamily, 0, &g_graphicsQueue);
    vkGetDeviceQueue(g_vkDevice, g_presentFamily,  0, &g_presentQueue);

    // Guard queue access from here on: the sim thread may now upload while the
    // render thread submits/presents (see g_queueLock).
    fuselage_mutex_init(&g_queueLock);
    g_queueLockReady = true;
    fuselage_mutex_init(&g_uploadLock);
    g_uploadLockReady = true;
    fuselage_mutex_init(&g_deferLock);
    g_deferLockReady = true;

    VkPipelineCacheCreateInfo cache_ci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    if (vkCreatePipelineCache(g_vkDevice, &cache_ci, NULL, &g_pipelineCache) != VK_SUCCESS) {
        g_pipelineCache = VK_NULL_HANDLE;  // optimization only -- see its doc comment
        printf("[Vulkan] Pipeline cache creation failed -- continuing without\n");
    }

    printf("[Vulkan] Logical device created\n");
    //tlPrint("[Vulkan] Logical device created");tlNewLine();

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

    // FIFO is the only present mode Vulkan guarantees every surface
    // supports, so it's always the fallback. When vsync is off, prefer
    // IMMEDIATE (uncapped, may tear); if the surface doesn't report it,
    // fall back to MAILBOX (also uncapped -- newest frame replaces the
    // queued one -- but tear-free) before giving up and landing on FIFO,
    // which would silently reintroduce the vsync cap the caller asked to
    // turn off.
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!g_vsyncEnabled) {
        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_vkSurface, &present_mode_count, NULL);
        VkPresentModeKHR* available_modes = malloc(present_mode_count * sizeof(VkPresentModeKHR));
        if (available_modes) {
            vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_vkSurface, &present_mode_count, available_modes);
            bool have_immediate = false, have_mailbox = false;
            for (uint32_t i = 0; i < present_mode_count; i++) {
                if (available_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) { have_immediate = true; }
                if (available_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)   { have_mailbox   = true; }
            }
            if      (have_immediate) { present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
            else if (have_mailbox)   { present_mode = VK_PRESENT_MODE_MAILBOX_KHR; }
            free(available_modes);
        }
    }

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = (uint32_t)GDMF_GetWidth();
        extent.height = (uint32_t)GDMF_GetHeight();
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
        //tlPrint("[Vulkan] Swapchain creation failed");tlNewLine();

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

            printf("[Vulkan] Image view creation failed (index %u)\n", i);
            //tlPrintFormattedC(WHITE, "[Vulkan] Image view creation failed (index %u)\n", i);tlNewLine();

            return -1;
        }
    }

    printf("[Vulkan] Swapchain: %ux%u, %u images\n",
        extent.width, extent.height, g_swapchainImageCount);
    //tlPrintFormattedC(WHITE, "[Vulkan] Swapchain: %ux%u, %u images",
    //    extent.width, extent.height, g_swapchainImageCount);tlNewLine();

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
        //tlPrint("[Vulkan] Render pass creation failed");tlNewLine();

        return -1;
    }

    printf("[Vulkan] Render pass created\n");
    //tlPrint("[Vulkan] Render pass created");tlNewLine();

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

            printf("[Vulkan] Framebuffer creation failed (index %u)\n", i);
            //FormattedC(WHITE, "[Vulkan] Framebuffer creation failed (index %u)", i);tlNewLine();

            return -1;
        }
    }

    //printf("[Vulkan] %u framebuffers created\n", g_swapchainImageCount);
    //tlPrintFormattedC(WHITE, "[Vulkan] %u framebuffers created", g_swapchainImageCount);tlNewLine();

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
        //tlPrint("[Vulkan] Command pool creation failed");tlNewLine();

        return -1;
    }

    // Separate pool for one-time upload commands (see g_uploadLock's doc
    // comment for why they can't share g_commandPool). TRANSIENT: every
    // buffer from this pool is recorded once, submitted once, and freed.
    VkCommandPoolCreateInfo upload_ci = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = g_graphicsFamily
    };

    if (vkCreateCommandPool(g_vkDevice, &upload_ci, NULL, &g_uploadCommandPool) != VK_SUCCESS) {
        printf("[Vulkan] Upload command pool creation failed\n");

        return -1;
    }

    printf("[Vulkan] Command pool created\n");
    //tlPrint("[Vulkan] Command pool created");tlNewLine();

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
        //tlPrint("[Vulkan] Out of memory for per-image objects");tlNewLine();

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

    printf("[Vulkan] Per-image objects ready (%u images)\n", g_swapchainImageCount);
    //tlPrintFormattedC(WHITE, "[Vulkan] Per-image objects ready (%u images)", g_swapchainImageCount);tlNewLine();

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
            // Persistently mapped -- vkFreeMemory implicitly unmaps.
            vkFreeMemory(g_vkDevice, g_paletteMemories[i], NULL);
        }
    }
    free(g_paletteBuffers);  g_paletteBuffers  = NULL;
    free(g_paletteMemories); g_paletteMemories = NULL;
    free(g_paletteMapped);   g_paletteMapped   = NULL;
    free(g_paletteUploadedGeneration); g_paletteUploadedGeneration = NULL;

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
    g_paletteMapped   = calloc(g_swapchainImageCount, sizeof(void*));
    g_paletteUploadedGeneration = calloc(g_swapchainImageCount, sizeof(uint32_t));  // 0 = never uploaded (generation starts at 1)
    if (!g_paletteBuffers || !g_paletteMemories || !g_paletteMapped || !g_paletteUploadedGeneration) {
        printf("[Vulkan] Out of memory for palette buffers\n");
        //tlPrint("[Vulkan] Out of memory for palette buffers");tlNewLine();

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
        // Prefer BAR memory: this buffer is read per-fragment by the
        // sprite/tile shaders, the single hottest GPU read path in the
        // engine -- see gdmfAllocateHostVisiblePreferDeviceLocal's doc comment.
        VK_CHECK(gdmfAllocateHostVisiblePreferDeviceLocal(&mem_req, &g_paletteMemories[i]));
        vkBindBufferMemory(g_vkDevice, g_paletteBuffers[i], g_paletteMemories[i], 0);

        // Persistent mapping: mapped once here, written directly every
        // upload, implicitly unmapped when the memory is freed. HOST_
        // COHERENT (both the preferred BAR type and the fallback include
        // it), so no flush is ever needed.
        VK_CHECK(vkMapMemory(g_vkDevice, g_paletteMemories[i], 0, GDMF_PALETTE_BUFFER_SIZE, 0, &g_paletteMapped[i]));
    }

    printf("[Vulkan] %u palette buffers ready\n", g_swapchainImageCount);
    //tlPrintFormattedC(WHITE, "[Vulkan] %u palette buffers ready", g_swapchainImageCount);tlNewLine();

    return 0;

fail:
    gdmf_destroy_palette_buffers();

    return -1;
}

// Re-uploads the full palette table (Colors[256][16], PackRGBA8 per entry)
// to this frame's shared buffer. Called once per frame, before any
// subsystem's own prepare() -- sprites/tiles just bind this same buffer
// in their own descriptor set instead of each maintaining a redundant
// private copy. Skipped entirely (the common case) when this image's
// buffer already holds the current Colors[] generation -- see
// GetColorsGeneration(); the full 4096-entry re-pack + 16 KiB write only
// happens on frames where a palette actually changed (or the first
// frame(s) after buffer creation, since the last-uploaded array starts
// at 0 and the generation counter starts at 1).
static void gdmf_palette_prepare(uint32_t imageIndex) {
    if (imageIndex >= g_swapchainImageCount || !g_paletteMapped) { return; }
    if (g_paletteMapped[imageIndex] == NULL) { return; }

    uint32_t generation = GetColorsGeneration();
    if (g_paletteUploadedGeneration[imageIndex] == generation) { return; }

    uint32_t* dst = (uint32_t*)g_paletteMapped[imageIndex];
    for (int pal = 0; pal < 256; pal++) {
        for (int idx = 0; idx < 16; idx++) {
            dst[pal * 16 + idx] = PackRGBA8(Colors[pal][idx]);
        }
    }
    g_paletteUploadedGeneration[imageIndex] = generation;

    return;
}

// Swapchain recreation
// Returns 0 on success, -1 on failure, -2 if skipped because the window is
// currently zero-sized (minimized, or a transient state during a display
// mode change) -- the caller must tell -2 apart from 0 and leave the
// recreation request pending rather than treat "skipped" as "done", or a
// resize/mode-change landing during exactly that zero-sized window
// permanently drops the request. That's what let a stale, already-destroyed
// palette buffer keep getting drawn from after a fullscreen-exclusive
// transition -- the flag was cleared as if recreation had happened, when it
// had silently done nothing.
static int gdmf_recreate_swapchain(void) {
    if (GDMF_GetWidth() == 0 || GDMF_GetHeight() == 0) { return -2; }

    VkFormat oldFormat = g_swapchainFormat;

    VkResult waitResult = gdmf_device_wait_idle();
    if (waitResult == VK_ERROR_DEVICE_LOST) {
        printf("[Vulkan] Device lost -- stopping rendering.\n");
        g_deviceLost = true;
        return -1;
    }
    // Device now idle, and we're inside prepare before anything was
    // recorded this tick (last tick's frame was already submitted) -- the
    // two conditions that make a full deferred-destruction flush safe.
    gdmf_defer_flush();
    // The mask compositor's offscreen targets are sized to the old extent and
    // its render pass/pipeline baked against the old format/image count -- tear
    // it all down here; the next masked frame lazily rebuilds it against the
    // new swapchain.
    gdmf_mask_compositor_destroy();
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

    printf("[Vulkan] Swapchain recreated\n");
    //tlPrint("[Vulkan] Swapchain recreated");tlNewLine();

    return 0;
}

// Render frame
void gdmf_vulkan_prepare_frame(void) {
    g_framePending = false;

    if (g_deviceLost) { return; }  // see g_deviceLost's doc comment
    if (GDMF_IsMinimized()) { return; }

    if (GDMF_ResizeOccurred()) { g_needsSwapchainRecreate = true; }

    if (g_needsSwapchainRecreate) {
        // Only clear the pending flag on actual success (0) -- -2 (skipped,
        // zero-sized right now) and -1 (failed) both leave it set so
        // recreation is retried on a later frame instead of being silently
        // dropped. See gdmf_recreate_swapchain's doc comment.
        if (gdmf_recreate_swapchain() == 0) { g_needsSwapchainRecreate = false; }
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
    if (result == VK_ERROR_DEVICE_LOST) {
        printf("[Vulkan] Device lost -- stopping rendering.\n");
        g_deviceLost = true;
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        // Any other failure (VK_ERROR_SURFACE_LOST_KHR, out-of-memory, ...)
        // means image_index was never written -- using it would index the
        // per-image arrays with garbage. Skip the frame and request a
        // recreate; if the failure is persistent (surface gone for good),
        // recreation keeps failing and the flag just stays pending.
        printf("[Vulkan] vkAcquireNextImageKHR failed: %d\n", result);
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

    // One frame provably completed (the fence wait above; fences signal in
    // submission order, so everything older is done too) -- advance the
    // deferred-destruction countdowns.
    gdmf_defer_tick();

    // Record (begin) -- left open across this call. gdmf_vulkan_submit_frame
    // appends the actual render-pass/draw commands to it below; everything
    // in *this* function either sets up GPU-side bookkeeping (acquire,
    // fences) or reads live game state into per-image buffers (the
    // _prepare() calls) -- nothing here blocks on vsync.
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

    // Subsystem prepare pass: fill vertex/CPU data before the render pass
    // opens. This is the only part of frame rendering that reads live game
    // state (sprites[], tile/pixie buffers, text layer cells) -- see
    // gdmf_vulkan.h's doc comment on why the split happens exactly here.
    gdmf_sprites_prepare(image_index);
    gdmf_tiles_prepare(image_index);
    gdmf_pixies_prepare(cmd, image_index);
    gdmf_textlayer_prepare(image_index);

    g_pendingImageIndex = image_index;
    g_pendingCmd        = cmd;
    g_framePending       = true;

    return;
}

// ============================================================================
// Masked compositor (Approach C -- PIXIE_MODE_MASK, see gdmf_pixies.h).
// ----------------------------------------------------------------------------
// A MASK-mode pixie governs only the items at its own priority: those
// tiles/sprites/normal-pixies are rendered to an offscreen color target, then
// composited onto the swapchain with their alpha scaled by the mask, so layers
// already drawn behind that priority show through the mask's shape. Everything
// here is created lazily the first frame a mask is actually present and torn
// down on swapchain recreate / shutdown -- scenes with no masks pay nothing.
//
// A mask may sit at any of the 256 priority levels, but at most MAX_PIXIES
// masks can exist at once, so the compositor keeps a small per-image pool of
// MAX_ACTIVE_MASKS "active mask" slots and assigns masked priorities to slots
// on the fly each frame (see gdmf_vulkan_submit_frame). MAX_ACTIVE_MASKS ==
// MAX_TILE_LAYERS keeps the allocation identical to the old per-band pool.
#define MAX_ACTIVE_MASKS MAX_TILE_LAYERS

typedef struct {
    VkImage        image;
    VkDeviceMemory memory;
    VkImageView    view;
    VkFramebuffer  framebuffer;
    bool           ready;
} MaskOffscreenTarget;

// Offscreen pass that renders one priority's items into a SHADER_READ-able
// color target. Same format/samples/single-subpass as g_renderPass so the
// existing tile/sprite/pixie pipelines are render-pass-compatible and record
// into it unchanged. g_mask_targets/g_mask_descsets are flat
// [imageCount*MAX_ACTIVE_MASKS] arrays indexed [img*MAX_ACTIVE_MASKS + slot],
// where slot is an active-mask index assigned this frame; targets are allocated
// lazily per slot actually used, descriptor sets up front.
static VkRenderPass          g_mask_offscreen_render_pass = VK_NULL_HANDLE;
static VkFormat              g_mask_target_format         = VK_FORMAT_UNDEFINED;
static VkSampler             g_mask_sampler               = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_mask_descset_layout        = VK_NULL_HANDLE;
static VkDescriptorPool      g_mask_descriptor_pool       = VK_NULL_HANDLE;
static VkPipelineLayout      g_mask_pipeline_layout       = VK_NULL_HANDLE;
static VkPipeline            g_mask_pipeline              = VK_NULL_HANDLE;
static MaskOffscreenTarget*  g_mask_targets               = NULL;
static VkDescriptorSet*      g_mask_descsets              = NULL;
static uint32_t              g_mask_slot_count            = 0;
static bool                  g_mask_ready                 = false;

// Must match mask_composite.frag's Push{ vec4 ndcRect; uint invert; }.
typedef struct { float ndcRect[4]; uint32_t invert; } MaskPushConstants;

// Full teardown -- safe to call repeatedly. Descriptor sets are reclaimed with
// the pool, so only the array of handles is freed, not each set individually.
static void gdmf_mask_compositor_destroy(void) {
    VkDevice dev = g_vkDevice;
    if (dev == VK_NULL_HANDLE) {
        free(g_mask_targets);  g_mask_targets  = NULL;
        free(g_mask_descsets); g_mask_descsets = NULL;
        g_mask_slot_count = 0;
        g_mask_ready = false;
        return;
    }

    if (g_mask_targets) {
        for (uint32_t i = 0; i < g_mask_slot_count; i++) {
            MaskOffscreenTarget* t = &g_mask_targets[i];
            if (t->framebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(dev, t->framebuffer, NULL); }
            if (t->view        != VK_NULL_HANDLE) { vkDestroyImageView(dev, t->view, NULL); }
            if (t->image       != VK_NULL_HANDLE) { vkDestroyImage(dev, t->image, NULL); }
            if (t->memory      != VK_NULL_HANDLE) { vkFreeMemory(dev, t->memory, NULL); }
        }
        free(g_mask_targets);
        g_mask_targets = NULL;
    }
    free(g_mask_descsets);
    g_mask_descsets   = NULL;
    g_mask_slot_count = 0;

    if (g_mask_pipeline        != VK_NULL_HANDLE) { vkDestroyPipeline(dev, g_mask_pipeline, NULL); g_mask_pipeline = VK_NULL_HANDLE; }
    if (g_mask_pipeline_layout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, g_mask_pipeline_layout, NULL); g_mask_pipeline_layout = VK_NULL_HANDLE; }
    if (g_mask_descriptor_pool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(dev, g_mask_descriptor_pool, NULL); g_mask_descriptor_pool = VK_NULL_HANDLE; }
    if (g_mask_descset_layout  != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, g_mask_descset_layout, NULL); g_mask_descset_layout = VK_NULL_HANDLE; }
    if (g_mask_sampler         != VK_NULL_HANDLE) { vkDestroySampler(dev, g_mask_sampler, NULL); g_mask_sampler = VK_NULL_HANDLE; }
    if (g_mask_offscreen_render_pass != VK_NULL_HANDLE) { vkDestroyRenderPass(dev, g_mask_offscreen_render_pass, NULL); g_mask_offscreen_render_pass = VK_NULL_HANDLE; }

    g_mask_target_format = VK_FORMAT_UNDEFINED;
    g_mask_ready = false;
}

// Builds the pipeline/pass/pool/sampler/descriptor-set arrays. Idempotent: a
// cheap flag check once ready. Returns false (and leaves masking disabled for
// the frame) on any failure -- the caller falls back to un-masked rendering.
static bool gdmf_mask_compositor_ensure(void) {
    if (g_mask_ready) { return true; }

    VkDevice dev = g_vkDevice;
    uint32_t imageCount = g_swapchainImageCount;
    if (dev == VK_NULL_HANDLE || imageCount == 0 ||
        g_renderPass == VK_NULL_HANDLE || g_swapchainFormat == VK_FORMAT_UNDEFINED) {
        return false;
    }

    // --- offscreen render pass (compatible with g_renderPass) ---
    VkAttachmentDescription color = {
        .format         = g_swapchainFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkAttachmentReference color_ref = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1, .pColorAttachments = &color_ref
    };
    // Deliberately a byte-for-byte copy of gdmf_create_render_pass's single
    // dependency (same attachment/subpass structure too), so pipelines baked
    // against g_renderPass are render-pass-compatible when recorded into this
    // pass -- the validation layer treats a differing dependencyCount as an
    // incompatibility (VUID-vkCmdDraw-renderPass-02684). The offscreen-write ->
    // composite-read hazard is instead ordered by an explicit pipeline barrier
    // after each offscreen pass ends (see gdmf_vulkan_submit_frame); the
    // cross-frame write-after-read on a reused target is covered by the
    // per-image fence the frame already waits on before re-recording.
    VkSubpassDependency dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo rp_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &color,
        .subpassCount = 1, .pSubpasses = &subpass,
        .dependencyCount = 1, .pDependencies = &dep
    };
    if (vkCreateRenderPass(dev, &rp_ci, NULL, &g_mask_offscreen_render_pass) != VK_SUCCESS) {
        printf("[Masks] Offscreen render pass creation failed\n");
        goto fail;
    }
    g_mask_target_format = g_swapchainFormat;

    // --- sampler (shared by offscreen + mask bindings) ---
    VkSamplerCreateInfo samp_ci = {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .borderColor  = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .maxLod       = 0.0f
    };
    if (vkCreateSampler(dev, &samp_ci, NULL, &g_mask_sampler) != VK_SUCCESS) {
        printf("[Masks] Sampler creation failed\n");
        goto fail;
    }

    // --- descriptor set layout: binding 0 = band color, binding 1 = mask ---
    VkDescriptorSetLayoutBinding binds[2] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    VkDescriptorSetLayoutCreateInfo dsl_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2, .pBindings = binds
    };
    if (vkCreateDescriptorSetLayout(dev, &dsl_ci, NULL, &g_mask_descset_layout) != VK_SUCCESS) {
        printf("[Masks] Descriptor set layout creation failed\n");
        goto fail;
    }

    // --- descriptor pool + one set per (image, active-mask slot) ---
    uint32_t slotCount = imageCount * (uint32_t)MAX_ACTIVE_MASKS;
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 2 * slotCount
    };
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = slotCount, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    if (vkCreateDescriptorPool(dev, &pool_ci, NULL, &g_mask_descriptor_pool) != VK_SUCCESS) {
        printf("[Masks] Descriptor pool creation failed\n");
        goto fail;
    }

    g_mask_targets  = (MaskOffscreenTarget*)calloc(slotCount, sizeof(MaskOffscreenTarget));
    g_mask_descsets = (VkDescriptorSet*)calloc(slotCount, sizeof(VkDescriptorSet));
    if (!g_mask_targets || !g_mask_descsets) {
        printf("[Masks] Out of memory allocating compositor slot arrays\n");
        goto fail;
    }
    g_mask_slot_count = slotCount;

    {
        VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)malloc(slotCount * sizeof(VkDescriptorSetLayout));
        if (!layouts) { printf("[Masks] Out of memory allocating layout list\n"); goto fail; }
        for (uint32_t i = 0; i < slotCount; i++) { layouts[i] = g_mask_descset_layout; }
        VkDescriptorSetAllocateInfo ds_ai = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = g_mask_descriptor_pool,
            .descriptorSetCount = slotCount, .pSetLayouts = layouts
        };
        VkResult r = vkAllocateDescriptorSets(dev, &ds_ai, g_mask_descsets);
        free(layouts);
        if (r != VK_SUCCESS) { printf("[Masks] Descriptor set allocation failed: %d\n", r); goto fail; }
    }

    // --- composite pipeline (fullscreen triangle, drawn into g_renderPass) ---
    VkShaderModuleCreateInfo vert_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = mask_composite_vert_spv_len, .pCode = (const uint32_t*)mask_composite_vert_spv
    };
    VkShaderModuleCreateInfo frag_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = mask_composite_frag_spv_len, .pCode = (const uint32_t*)mask_composite_frag_spv
    };
    VkShaderModule vert_module = VK_NULL_HANDLE, frag_module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &vert_ci, NULL, &vert_module) != VK_SUCCESS ||
        vkCreateShaderModule(dev, &frag_ci, NULL, &frag_module) != VK_SUCCESS) {
        printf("[Masks] Shader module creation failed\n");
        if (vert_module) { vkDestroyShaderModule(dev, vert_module, NULL); }
        if (frag_module) { vkDestroyShaderModule(dev, frag_module, NULL); }
        goto fail;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_module, .pName = "main" }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };  // no buffers -- geometry comes from gl_VertexIndex
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blend_attachment
    };
    VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2, .pDynamicStates = dyn_states
    };
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(MaskPushConstants)
    };
    VkPipelineLayoutCreateInfo pl_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &g_mask_descset_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push_range
    };
    if (vkCreatePipelineLayout(dev, &pl_ci, NULL, &g_mask_pipeline_layout) != VK_SUCCESS) {
        printf("[Masks] Pipeline layout creation failed\n");
        vkDestroyShaderModule(dev, vert_module, NULL);
        vkDestroyShaderModule(dev, frag_module, NULL);
        goto fail;
    }
    VkGraphicsPipelineCreateInfo pipe_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vertex_input, .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state, .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling, .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state, .layout = g_mask_pipeline_layout,
        .renderPass = g_renderPass, .subpass = 0
    };
    VkResult pipe_r = vkCreateGraphicsPipelines(dev, g_pipelineCache, 1, &pipe_ci, NULL, &g_mask_pipeline);
    vkDestroyShaderModule(dev, vert_module, NULL);
    vkDestroyShaderModule(dev, frag_module, NULL);
    if (pipe_r != VK_SUCCESS) {
        printf("[Masks] Composite pipeline creation failed: %d\n", pipe_r);
        goto fail;
    }

    g_mask_ready = true;
    printf("[Masks] Compositor ready (%u slots)\n", slotCount);
    return true;

fail:
    gdmf_mask_compositor_destroy();
    return false;
}

// Lazily creates the offscreen color target for (imageIndex, active-mask slot).
// Returns NULL on failure (caller falls back to rendering that priority un-masked).
static MaskOffscreenTarget* gdmf_mask_ensure_target(uint32_t imageIndex, uint8_t maskSlot) {
    uint32_t slot = imageIndex * (uint32_t)MAX_ACTIVE_MASKS + maskSlot;
    if (slot >= g_mask_slot_count) { return NULL; }
    MaskOffscreenTarget* t = &g_mask_targets[slot];
    if (t->ready) { return t; }

    VkDevice dev = g_vkDevice;

    VkImageCreateInfo img_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
        .format = g_mask_target_format,
        .extent = { g_swapchainExtent.width, g_swapchainExtent.height, 1 },
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(dev, &img_ci, NULL, &t->image) != VK_SUCCESS) { goto fail; }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(dev, t->image, &mem_req);
    VkMemoryAllocateInfo mem_ai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (mem_ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &mem_ai, NULL, &t->memory) != VK_SUCCESS) { goto fail; }
    vkBindImageMemory(dev, t->image, t->memory, 0);

    VkImageViewCreateInfo view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = t->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = g_mask_target_format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    if (vkCreateImageView(dev, &view_ci, NULL, &t->view) != VK_SUCCESS) { goto fail; }

    VkFramebufferCreateInfo fb_ci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = g_mask_offscreen_render_pass, .attachmentCount = 1, .pAttachments = &t->view,
        .width = g_swapchainExtent.width, .height = g_swapchainExtent.height, .layers = 1
    };
    if (vkCreateFramebuffer(dev, &fb_ci, NULL, &t->framebuffer) != VK_SUCCESS) { goto fail; }

    t->ready = true;
    return t;

fail:
    if (t->framebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(dev, t->framebuffer, NULL); }
    if (t->view        != VK_NULL_HANDLE) { vkDestroyImageView(dev, t->view, NULL); }
    if (t->image       != VK_NULL_HANDLE) { vkDestroyImage(dev, t->image, NULL); }
    if (t->memory      != VK_NULL_HANDLE) { vkFreeMemory(dev, t->memory, NULL); }
    memset(t, 0, sizeof(*t));
    printf("[Masks] Failed to create offscreen target for mask slot %u\n", maskSlot);
    return NULL;
}

// Points (imageIndex, active-mask slot)'s descriptor set at its offscreen
// target (the priority's color) + the mask pixie's image (coverage). Safe to
// update per frame: the set is only ever consumed by frames rendering this same
// image index, whose prior use already completed on the per-image fence before
// re-recording.
static void gdmf_mask_update_descset(uint32_t imageIndex, uint8_t maskSlot,
                                     VkImageView colorView, VkImageView maskView) {
    uint32_t slot = imageIndex * (uint32_t)MAX_ACTIVE_MASKS + maskSlot;
    VkDescriptorImageInfo infos[2] = {
        { .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .imageView = colorView, .sampler = g_mask_sampler },
        { .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .imageView = maskView, .sampler = g_mask_sampler }
    };
    VkWriteDescriptorSet writes[2] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = g_mask_descsets[slot],
          .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .pImageInfo = &infos[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = g_mask_descsets[slot],
          .dstBinding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .pImageInfo = &infos[1] }
    };
    vkUpdateDescriptorSets(g_vkDevice, 2, writes, 0, NULL);
}

void gdmf_vulkan_submit_frame(void) {
    if (!g_framePending) { return; }
    g_framePending = false;

    uint32_t        image_index = g_pendingImageIndex;
    VkCommandBuffer cmd         = g_pendingCmd;

    VkClearValue clear_color = { .color = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} } };
    VkRenderPassBeginInfo rp_begin = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass      = g_renderPass,
        .framebuffer     = g_framebuffers[image_index],
        .renderArea      = { .offset = {0, 0}, .extent = g_swapchainExtent },
        .clearValueCount = 1,
        .pClearValues    = &clear_color
    };
    // --- Masked pre-pass (Approach C, see the mask compositor section above
    // and PIXIE_MODE_MASK in gdmf_pixies.h) -------------------------------
    // Discover which priorities host an active MASK pixie and assign each an
    // active-mask slot (at most MAX_PIXIES masks can exist, and MAX_ACTIVE_MASKS
    // covers that). Render each masked priority's items into its own offscreen
    // target so the main pass can composite it through the mask at its painter
    // position. Any priority whose compositor resources fail to materialize
    // silently falls back to un-masked rendering.
    int               maskSlotAt[256];               // priority -> active-mask slot, or -1
    int               slotPriority[MAX_ACTIVE_MASKS];
    int               slotMaskId[MAX_ACTIVE_MASKS];
    MaskPushConstants maskPush[MAX_ACTIVE_MASKS] = {0};
    int               activeMaskCount = 0;
    for (int prio = 0; prio < 256; prio++) { maskSlotAt[prio] = -1; }

    for (int prio = 255; prio >= 0 && activeMaskCount < (int)MAX_ACTIVE_MASKS; prio--) {
        int id = gdmf_pixies_priority_mask_id((uint8_t)prio);
        if (id < 0) { continue; }
        int slot           = activeMaskCount++;
        maskSlotAt[prio]   = slot;
        slotPriority[slot] = prio;
        slotMaskId[slot]   = id;
    }
    bool anyMask = activeMaskCount > 0;
    if (anyMask && !gdmf_mask_compositor_ensure()) {
        for (int prio = 0; prio < 256; prio++) { maskSlotAt[prio] = -1; }
        activeMaskCount = 0;
        anyMask = false;
    }

    if (anyMask) {
        VkClearValue offscreen_clear = { .color = { .float32 = {0.0f, 0.0f, 0.0f, 0.0f} } };
        for (int slot = 0; slot < activeMaskCount; slot++) {
            int prio = slotPriority[slot];

            VkImageView maskView = VK_NULL_HANDLE;
            float       ndc[4]   = {0};
            bool        invert   = false;
            MaskOffscreenTarget* t = gdmf_mask_ensure_target(image_index, (uint8_t)slot);
            if (!t || !gdmf_pixies_get_mask_info(slotMaskId[slot], &maskView, ndc, &invert)) {
                maskSlotAt[prio] = -1;  // this priority renders un-masked below
                continue;
            }

            VkRenderPassBeginInfo off_begin = {
                .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass      = g_mask_offscreen_render_pass,
                .framebuffer     = t->framebuffer,
                .renderArea      = { .offset = {0, 0}, .extent = g_swapchainExtent },
                .clearValueCount = 1,
                .pClearValues    = &offscreen_clear
            };
            vkCmdBeginRenderPass(cmd, &off_begin, VK_SUBPASS_CONTENTS_INLINE);
            // Type tie-break at this priority: pixies (back), tiles, sprites (front).
            gdmf_pixies_record_priority(cmd, image_index, (uint8_t)prio);
            gdmf_tiles_record_priority(cmd, image_index, (uint8_t)prio);
            gdmf_sprites_record_priority(cmd, image_index, (uint8_t)prio);
            vkCmdEndRenderPass(cmd);

            // Order this priority's offscreen color writes before the composite
            // samples the target in the main pass. The render pass already
            // left the image in SHADER_READ_ONLY_OPTIMAL (its finalLayout);
            // this barrier supplies the execution + memory dependency that a
            // subpass-external dependency would otherwise carry (dropped above
            // to keep the pass render-pass-compatible with g_renderPass).
            VkImageMemoryBarrier off_barrier = {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = t->image,
                .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT
            };
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, NULL, 0, NULL, 1, &off_barrier);

            gdmf_mask_update_descset(image_index, (uint8_t)slot, t->view, maskView);
            maskPush[slot].ndcRect[0] = ndc[0];
            maskPush[slot].ndcRect[1] = ndc[1];
            maskPush[slot].ndcRect[2] = ndc[2];
            maskPush[slot].ndcRect[3] = ndc[3];
            maskPush[slot].invert     = invert ? 1u : 0u;
        }
    }

    // --- Main swapchain pass --------------------------------------------
    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    // Genuine per-item priority draw, back-to-front: priority 255 is furthest
    // back, priority 0 is frontmost. At each level the type tie-break is pixies
    // (back), then tiles, then sprites (front). All three record hooks are
    // no-ops when nothing sits at that priority. These only read the per-image
    // buffers gdmf_vulkan_prepare_frame's _prepare() calls already filled --
    // never live game state directly -- so none of this needs whatever lock a
    // caller took around prepare(). A masked priority is instead composited
    // from its pre-rendered offscreen target (built just above).
    for (int prio = 255; prio >= 0; prio--) {
        int slot = maskSlotAt[prio];
        if (slot >= 0) {
            // Fullscreen triangle over the whole swapchain: the offscreen holds
            // this priority's letterboxed content at 1:1 screen position
            // (transparent elsewhere), its alpha scaled by the mask.
            VkViewport vp = { 0.0f, 0.0f,
                (float)g_swapchainExtent.width, (float)g_swapchainExtent.height, 0.0f, 1.0f };
            VkRect2D   sc = { .offset = {0, 0}, .extent = g_swapchainExtent };
            uint32_t   dsslot = image_index * (uint32_t)MAX_ACTIVE_MASKS + (uint32_t)slot;

            vkCmdSetViewport(cmd, 0, 1, &vp);
            vkCmdSetScissor(cmd, 0, 1, &sc);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_mask_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                g_mask_pipeline_layout, 0, 1, &g_mask_descsets[dsslot], 0, NULL);
            vkCmdPushConstants(cmd, g_mask_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(MaskPushConstants), &maskPush[slot]);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        } else {
            gdmf_pixies_record_priority(cmd, image_index, (uint8_t)prio);
            gdmf_tiles_record_priority(cmd, image_index, (uint8_t)prio);
            gdmf_sprites_record_priority(cmd, image_index, (uint8_t)prio);
        }
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
    // Serialize this frame's submit + present against any concurrent upload
    // submit on the sim thread (see g_queueLock). g_state_lock does NOT cover
    // this -- it guards live game state around PrepareFrame, not the queue.
    gdmf_queue_lock();
    VkResult submitResult = vkQueueSubmit(g_graphicsQueue, 1, &submit_info, g_inFlightFence[image_index]);
    if (submitResult == VK_ERROR_DEVICE_LOST) {
        gdmf_queue_unlock();
        printf("[Vulkan] Device lost -- stopping rendering.\n");
        g_deviceLost = true;
        return;
    }
    if (submitResult != VK_SUCCESS) {
        // The submit that would have signaled this image's fence and its
        // renderFinished semaphore never happened: presenting now would wait
        // on a semaphore that never signals, and the next frame to land on
        // this image index would block forever on the fence (reset in
        // prepare, waited with no timeout). Skip the present and request a
        // swapchain recreate instead -- gdmf_create_per_image_objects()
        // rebuilds every fence pre-signaled, which is what makes this
        // recoverable rather than a permanent render-thread hang.
        gdmf_queue_unlock();
        printf("[Vulkan] vkQueueSubmit failed: %d\n", submitResult);
        g_needsSwapchainRecreate = true;
        return;
    }

    // Present -- the vsync-blocking call. Must never run under g_state_lock
    // (the game-state lock); see gdmf_vulkan.h's doc comment. The queue lock is
    // different -- it is only contended when the sim thread actually uploads,
    // not every frame, so a rare ~1-frame wait for an upload is acceptable.
    VkPresentInfoKHR present_info = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &g_renderFinished[image_index],
        .swapchainCount     = 1,
        .pSwapchains        = &g_vkSwapchain,
        .pImageIndices      = &image_index
    };
    VkResult result = vkQueuePresentKHR(g_presentQueue, &present_info);
    gdmf_queue_unlock();

    if (result == VK_ERROR_DEVICE_LOST) {
        printf("[Vulkan] Device lost -- stopping rendering.\n");
        g_deviceLost = true;
    }
    else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) { g_needsSwapchainRecreate = true; }
    else if (result != VK_SUCCESS) { printf("[Vulkan] vkQueuePresentKHR failed: %d\n", result); }

    return;
}

// Internal utilities (gdmf_vulkan_internal.h)
bool             gdmf_vulkan_device_lost(void)  { return g_deviceLost; }
VkDevice         gdmf_get_device(void)          { return g_vkDevice; }
VkPhysicalDevice gdmf_get_physical_device(void) { return g_physicalDevice; }
VkQueue          gdmf_get_graphics_queue(void)  { return g_graphicsQueue; }
VkCommandPool    gdmf_get_command_pool(void)    { return g_commandPool; }
VkRenderPass     gdmf_get_render_pass(void)     { return g_renderPass; }
VkPipelineCache  gdmf_get_pipeline_cache(void)  { return g_pipelineCache; }
VkExtent2D       gdmf_get_swapchain_extent(void){ return g_swapchainExtent; }
uint32_t         gdmf_get_swapchain_image_count(void) { return g_swapchainImageCount; }

// Centered, aspect-correct sub-rectangle of the current swapchain extent
// that rendering should target -- the fixed-aspect logical canvas sprites.c
// and gdmf_textlayer.c render onto (see SPRITE_REFERENCE_CANVAS_WIDTH/HEIGHT)
// always maps straight to this rect's NDC space rather than the swapchain's
// full extent, so a window/monitor shape that doesn't match the design
// aspect ratio (GDMF_SetAspectRatio) never stretches the image -- the
// untouched border, already cleared to black by the render pass, becomes
// the letterbox/pillarbox bars instead.
VkRect2D gdmf_get_render_viewport_rect(void) {
    int aspectNum = GDMF_GetAspectRatioNum();
    int aspectDen = GDMF_GetAspectRatioDen();

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
    // g_memProperties was cached when the physical device was picked --
    // no per-call vkGetPhysicalDeviceMemoryProperties query needed.
    for (uint32_t i = 0; i < g_memProperties.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) &&
            (g_memProperties.memoryTypes[i].propertyFlags & properties) == properties) { return i; }
    }

    return UINT32_MAX;
}

// See the doc comment in gdmf_vulkan_internal.h -- BAR-preferring
// allocation with fall-through to plain host memory when the BAR type is
// absent OR its (small) heap can't satisfy this allocation.
VkResult gdmfAllocateHostVisiblePreferDeviceLocal(const VkMemoryRequirements* req,
                                                  VkDeviceMemory* out_memory) {
    VkMemoryAllocateInfo alloc_info = {
        .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req->size
    };

    uint32_t preferred = gdmfFindMemoryType(req->memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uint32_t fallback = gdmfFindMemoryType(req->memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (preferred != UINT32_MAX && preferred != fallback) {
        alloc_info.memoryTypeIndex = preferred;
        if (vkAllocateMemory(g_vkDevice, &alloc_info, NULL, out_memory) == VK_SUCCESS) {
            return VK_SUCCESS;
        }
        // BAR heap exhausted (or fragmented) -- fall through to ordinary
        // host memory, exactly what this buffer used before BAR preference
        // existed. Deliberately not logged: a huge tile layer recreating
        // its buffer during scrolling hits this every grow, and that's
        // working as intended, not an error.
    }

    if (fallback == UINT32_MAX) { return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
    alloc_info.memoryTypeIndex = fallback;

    return vkAllocateMemory(g_vkDevice, &alloc_info, NULL, out_memory);
}

// See the doc comment in gdmf_vulkan_internal.h -- the queue-lock wrapper
// that makes device-wait-idle safe to call from the sim thread while the
// render thread is submitting/presenting.
VkResult gdmf_device_wait_idle(void) {
    if (g_vkDevice == VK_NULL_HANDLE) { return VK_SUCCESS; }

    gdmf_queue_lock();
    VkResult result = vkDeviceWaitIdle(g_vkDevice);
    gdmf_queue_unlock();

    return result;
}

// Deferred-destruction implementation -- see the statics + doc comment near
// g_deferList at the top of this file, and the API doc comments in
// gdmf_vulkan_internal.h.

// Destroys one entry's handles, in dependency order (view before image,
// buffer before memory). Every vkDestroy*/vkFree* here ignores
// VK_NULL_HANDLE, so partially-filled entries are fine.
static void gdmf_defer_destroy_entry(GdmfDeferEntry* e) {
    VkDevice dev = g_vkDevice;

    if (dev == VK_NULL_HANDLE) { return; }

    switch (e->kind) {
    case GDMF_DEFER_BUFFER:
    case GDMF_DEFER_IMAGE:
        if (e->u.res.buffer != VK_NULL_HANDLE) { vkDestroyBuffer(dev, e->u.res.buffer, NULL); }
        if (e->u.res.view   != VK_NULL_HANDLE) { vkDestroyImageView(dev, e->u.res.view, NULL); }
        if (e->u.res.image  != VK_NULL_HANDLE) { vkDestroyImage(dev, e->u.res.image, NULL); }
        if (e->u.res.memory != VK_NULL_HANDLE) { vkFreeMemory(dev, e->u.res.memory, NULL); }
        break;
    case GDMF_DEFER_DESCRIPTOR_SET:
        vkFreeDescriptorSets(dev, e->u.dset.pool, 1, &e->u.dset.set);
        break;
    case GDMF_DEFER_DESCRIPTOR_POOL:
        vkDestroyDescriptorPool(dev, e->u.dpool, NULL);
        break;
    case GDMF_DEFER_SAMPLER:
        vkDestroySampler(dev, e->u.sampler, NULL);
        break;
    }

    return;
}

static void gdmf_defer_push(const GdmfDeferEntry* proto) {
    GdmfDeferEntry* e = malloc(sizeof(*e));

    if (!e || !g_deferLockReady) {
        // Out of memory (or called before the device/lock exist, which no
        // current caller does): fall back to draining the GPU and
        // destroying on the spot. This reopens the recorded-but-
        // unsubmitted-frame window the queue exists to close, but only in
        // a corner where allocation of a 6-pointer node just failed --
        // strictly better than leaking, and no worse than the pre-queue
        // behavior.
        free(e);
        printf("[Vulkan] Deferred-destroy queue unavailable -- destroying immediately\n");
        gdmf_device_wait_idle();
        GdmfDeferEntry tmp = *proto;
        gdmf_defer_destroy_entry(&tmp);
        return;
    }

    *e = *proto;
    fuselage_mutex_lock(&g_deferLock);
    e->framesLeft = g_swapchainImageCount + 1;
    e->next       = g_deferList;
    g_deferList   = e;
    fuselage_mutex_unlock(&g_deferLock);

    return;
}

void gdmf_defer_destroy_buffer(VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE) { return; }
    GdmfDeferEntry e = { .kind = GDMF_DEFER_BUFFER };

    e.u.res.buffer = buffer;
    e.u.res.memory = memory;
    gdmf_defer_push(&e);

    return;
}

void gdmf_defer_destroy_image(VkImage image, VkImageView view, VkDeviceMemory memory) {
    if (image == VK_NULL_HANDLE && view == VK_NULL_HANDLE && memory == VK_NULL_HANDLE) { return; }
    GdmfDeferEntry e = { .kind = GDMF_DEFER_IMAGE };

    e.u.res.image  = image;
    e.u.res.view   = view;
    e.u.res.memory = memory;
    gdmf_defer_push(&e);

    return;
}

void gdmf_defer_free_descriptor_set(VkDescriptorPool pool, VkDescriptorSet set) {
    if (pool == VK_NULL_HANDLE || set == VK_NULL_HANDLE) { return; }
    GdmfDeferEntry e = { .kind = GDMF_DEFER_DESCRIPTOR_SET };

    e.u.dset.pool = pool;
    e.u.dset.set  = set;
    gdmf_defer_push(&e);

    return;
}

void gdmf_defer_destroy_descriptor_pool(VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE) { return; }
    GdmfDeferEntry e = { .kind = GDMF_DEFER_DESCRIPTOR_POOL };

    e.u.dpool = pool;
    gdmf_defer_push(&e);

    return;
}

void gdmf_defer_destroy_sampler(VkSampler sampler) {
    if (sampler == VK_NULL_HANDLE) { return; }
    GdmfDeferEntry e = { .kind = GDMF_DEFER_SAMPLER };

    e.u.sampler = sampler;
    gdmf_defer_push(&e);

    return;
}

void gdmf_defer_forget_descriptor_pool(VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE || !g_deferLockReady) { return; }

    fuselage_mutex_lock(&g_deferLock);
    GdmfDeferEntry** pp = &g_deferList;
    while (*pp) {
        GdmfDeferEntry* e = *pp;

        if (e->kind == GDMF_DEFER_DESCRIPTOR_SET && e->u.dset.pool == pool) {
            *pp = e->next;
            free(e);  // dropped, not freed -- the caller is about to destroy the whole pool
        } else {
            pp = &e->next;
        }
    }
    fuselage_mutex_unlock(&g_deferLock);

    return;
}

// One countdown step per completed frame; destroys whatever reached zero.
// Render thread only (called from gdmf_vulkan_prepare_frame).
static void gdmf_defer_tick(void) {
    if (!g_deferLockReady) { return; }

    fuselage_mutex_lock(&g_deferLock);
    GdmfDeferEntry** pp = &g_deferList;
    while (*pp) {
        GdmfDeferEntry* e = *pp;

        if (e->framesLeft > 0) { e->framesLeft--; }
        if (e->framesLeft == 0) {
            *pp = e->next;
            gdmf_defer_destroy_entry(e);
            free(e);
        } else {
            pp = &e->next;
        }
    }
    fuselage_mutex_unlock(&g_deferLock);

    return;
}

// Destroys everything queued, regardless of countdown. Only valid when the
// device is idle AND no recorded-but-unsubmitted frame exists -- see the
// two call sites (swapchain recreation, shutdown).
static void gdmf_defer_flush(void) {
    if (!g_deferLockReady) { return; }

    fuselage_mutex_lock(&g_deferLock);
    while (g_deferList) {
        GdmfDeferEntry* e = g_deferList;

        g_deferList = e->next;
        gdmf_defer_destroy_entry(e);
        free(e);
    }
    fuselage_mutex_unlock(&g_deferLock);

    return;
}

int gdmfExecuteOneTimeCommands(GDMF_CommandRecordFunc record_func, void* user_data) {
    // The whole body -- allocate, record, submit, wait, free -- runs under
    // the upload lock: everything here touches g_uploadCommandPool, which
    // (like any command pool) is not internally synchronized, and this
    // function is reachable from the main thread (bring-up), the sim thread
    // (mid-game uploads), and the render thread (lazy pixie image init).
    gdmf_upload_lock();

    VkCommandBufferAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_uploadCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd   = VK_NULL_HANDLE;
    VkFence         fence = VK_NULL_HANDLE;

    VK_CHECK(vkAllocateCommandBuffers(g_vkDevice, &alloc_info, &cmd));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    record_func(cmd, user_data);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fence_ci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VK_CHECK(vkCreateFence(g_vkDevice, &fence_ci, NULL, &fence));

    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    // This can run on the sim thread (a mid-game UploadSpriteBitmap etc.) while
    // the render thread submits/presents -- serialize the queue ops (see
    // g_queueLock). Only the submit itself needs the queue lock: the wait is
    // on a fence, not the queue, so it happens outside the lock -- and unlike
    // the old vkQueueWaitIdle here, it completes when THIS upload does, not
    // when every in-flight frame ahead of it has drained too.
    gdmf_queue_lock();
    VkResult submitRes = vkQueueSubmit(g_graphicsQueue, 1, &submit, fence);
    gdmf_queue_unlock();
    VkResult waitRes = (submitRes == VK_SUCCESS)
        ? vkWaitForFences(g_vkDevice, 1, &fence, VK_TRUE, UINT64_MAX)
        : VK_SUCCESS;
    VK_CHECK(submitRes);
    VK_CHECK(waitRes);

    vkDestroyFence(g_vkDevice, fence, NULL);
    vkFreeCommandBuffers(g_vkDevice, g_uploadCommandPool, 1, &cmd);
    gdmf_upload_unlock();
    return 0;

fail:
    // cmd/fence may still be VK_NULL_HANDLE if their own creation is what
    // failed -- vkFreeCommandBuffers ignores VK_NULL_HANDLE entries and
    // vkDestroyFence ignores a VK_NULL_HANDLE fence, so this is safe at
    // every failure point above.
    vkDestroyFence(g_vkDevice, fence, NULL);
    vkFreeCommandBuffers(g_vkDevice, g_uploadCommandPool, 1, &cmd);
    gdmf_upload_unlock();

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

        printf("%s %s\n", prefix, data->pMessage);
        //tlPrintFormattedC(WHITE, "%s %s", prefix, data->pMessage);tlNewLine();

    return VK_FALSE;
}

static int gdmf_create_debug_messenger(void) {
    PFN_vkCreateDebugUtilsMessengerEXT pfn_create =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            g_vkInstance, "vkCreateDebugUtilsMessengerEXT");

    if (!pfn_create) {
        printf("[Vulkan] vkCreateDebugUtilsMessengerEXT not available\n");
        //tlPrint("[Vulkan] vkCreateDebugUtilsMessengerEXT not available");tlNewLine();

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
        //tlPrint("[Vulkan] Debug messenger creation failed");tlNewLine();

        return -1;
    }

    printf("[Vulkan] Debug messenger active\n");
    //tlPrint("[Vulkan] Debug messenger active");tlNewLine();

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
