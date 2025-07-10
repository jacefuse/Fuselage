// gdmf.c - Minimal Vulkan Bootstrap for Fuselage on Windows (other systems eventually)
// See header for more details

#include "gdmf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global state
static HWND g_hWnd = NULL;
static HINSTANCE g_hInstance = NULL;
static VkInstance g_vkInstance = VK_NULL_HANDLE;
static VkSurfaceKHR g_vkSurface = VK_NULL_HANDLE;

// Devices
static GDMFDeviceCandidate* g_device_candidates = NULL;
static GDMFDeviceCandidate* g_selectedDevice = NULL;
static uint32_t g_device_count = 0;
static VkDevice g_vkDevice = VK_NULL_HANDLE;
static VkQueue g_graphicsQueue = VK_NULL_HANDLE;
static VkQueue g_presentQueue = VK_NULL_HANDLE;

// Swap Chain
static VkSwapchainKHR g_vkSwapchain = VK_NULL_HANDLE;
static VkFormat g_swapchainImageFormat;
static VkExtent2D g_swapchainExtent;
static VkImage* g_swapchainImages = NULL;
static VkImageView* g_swapchainImageViews = NULL;
static uint32_t g_swapchainImageCount = 0;
static VkImage g_depthImage = VK_NULL_HANDLE;
static VkDeviceMemory g_depthImageMemory = VK_NULL_HANDLE;
static VkImageView g_depthImageView = VK_NULL_HANDLE;
static VkFormat g_depthFormat = VK_FORMAT_D32_SFLOAT;  // 32-bit float depth
static VkRenderPass g_renderPass = VK_NULL_HANDLE;
static VkFramebuffer* g_swapchainFramebuffers = NULL;

// GDMF internal functions
VkInstance gdmf_get_instance(void) { return g_vkInstance; }
VkSurfaceKHR gdmf_get_surface(void) { return g_vkSurface; }
int gdmf_enumerate_devices(void);
int gdmf_evaluate_devices(void);
int gdmf_evaluate_devices(void);
static bool find_queue_families(GDMFDeviceCandidate* candidate);
static bool check_device_extensions(GDMFDeviceCandidate* candidate);
static bool check_surface_support(GDMFDeviceCandidate* candidate);
static int calculate_device_score(GDMFDeviceCandidate* candidate);
static int gdmf_create_logical_device(void);
static int gdmf_create_depth_buffer(void);
static int gdmf_create_framebuffers(void);
static int gdmf_create_render_pass(void);
static int gdmf_create_swapchain(void);
static int createVulkanInstance(void);
static int createVulkanSurface(void);
GDMFDeviceCandidate* gdmf_select_device(void);
LRESULT CALLBACK GDMFWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// Main facing functions
int GDMFinit(void) {
    g_hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = GDMFWindowProc; // Our Window callback
    wc.hInstance = g_hInstance;
    wc.lpszClassName = TEXT("GDMFWindowClass");

    if (!RegisterClass(&wc)) return -1;

    // Create Window (This will likely be extended or replaced.)
    g_hWnd = CreateWindowEx(0, wc.lpszClassName, TEXT("GDMF Window"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        NULL, NULL, g_hInstance, NULL);
    if (!g_hWnd) return -1;

    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hWnd);

    // Vulkanic Start
    if (createVulkanInstance() != VK_SUCCESS) return -1;
    if (createVulkanSurface() != VK_SUCCESS) return -1;

    // Enumerate and evaluate devices to select the most suitable
    if (gdmf_enumerate_devices() != 0) return -1;
    if (gdmf_evaluate_devices() != 0) return -1;

    GDMFDeviceCandidate* selectedDevice = gdmf_select_device();
    if (!selectedDevice) return -1;  // No suitable device found

    // Select device
    g_selectedDevice = selectedDevice;
    if (gdmf_create_logical_device() != 0) return -1;

    printf("[GDMF Init]\nSelected Device: %s (Type: %d, Score: %d)\n",
        g_selectedDevice->properties.deviceName,
        g_selectedDevice->properties.deviceType,
        g_selectedDevice->score);

    // Create swapchain
    if (gdmf_create_swapchain() != 0) return -1;
    printf("Swapchain created.\n");

    // Create render pass
    if (gdmf_create_render_pass() != 0) return -1;
    printf("Render pass created.\n");
    if (gdmf_create_depth_buffer() != 0) return -1;
    printf("Depth buffer created.\n");
    // Create framebuffers
    if (gdmf_create_framebuffers() != 0) return -1;
    printf("Framebuffers created.\n");

    return 0;
}

// Cleanup happens in the reverse order of Init
void GDMFshutdown(void) {
    // Destroy the framebuffers
    if (g_swapchainFramebuffers) {
        for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
            vkDestroyFramebuffer(g_vkDevice, g_swapchainFramebuffers[i], NULL);
        }
        free(g_swapchainFramebuffers);
        g_swapchainFramebuffers = NULL;
    }

    // Destroy depth buffer
    if (g_depthImageView) {
        vkDestroyImageView(g_vkDevice, g_depthImageView, NULL);
        g_depthImageView = VK_NULL_HANDLE;
    }
    if (g_depthImage) {
        vkDestroyImage(g_vkDevice, g_depthImage, NULL);
        g_depthImage = VK_NULL_HANDLE;
    }
    if (g_depthImageMemory) {
        vkFreeMemory(g_vkDevice, g_depthImageMemory, NULL);
        g_depthImageMemory = VK_NULL_HANDLE;
    }

    // Destroy render pass
    if (g_renderPass) {
        vkDestroyRenderPass(g_vkDevice, g_renderPass, NULL);
        g_renderPass = VK_NULL_HANDLE;
    }

    // Clean up swapchain image views
    if (g_swapchainImageViews) {
        for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
            vkDestroyImageView(g_vkDevice, g_swapchainImageViews[i], NULL);
        }
        free(g_swapchainImageViews);
        g_swapchainImageViews = NULL;
    }

    // Clean up swapchain images array (the images themselves are owned by swapchain)
    if (g_swapchainImages) {
        free(g_swapchainImages);
        g_swapchainImages = NULL;
    }

    // Destroy Swapchain
    if (g_vkSwapchain) {
        vkDestroySwapchainKHR(g_vkDevice, g_vkSwapchain, NULL);
        g_vkSwapchain = VK_NULL_HANDLE;
    }

    // Destroy Device
    if (g_vkDevice) {
        vkDestroyDevice(g_vkDevice, NULL);
        g_vkDevice = VK_NULL_HANDLE;
    }

    // Release Candidates
    if (g_device_candidates) {
        free(g_device_candidates);
        g_device_candidates = NULL;
        g_device_count = 0;
    }

    // Poof
    if (g_vkSurface) {
        PFN_vkDestroySurfaceKHR pfnDestroySurfaceKHR =
            (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(g_vkInstance, "vkDestroySurfaceKHR");
        if (pfnDestroySurfaceKHR)
            pfnDestroySurfaceKHR(g_vkInstance, g_vkSurface, NULL);
    }

    // Gone
    if (g_vkInstance)
        vkDestroyInstance(g_vkInstance, NULL);

    // Close the window
    if (g_hWnd)
        DestroyWindow(g_hWnd);

    UnregisterClass(TEXT("GDMFWindowClass"), g_hInstance);

    printf("[GDMF Shutdown]\n");

    return;
}

LRESULT CALLBACK GDMFWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Vulkan related functions
static int createVulkanInstance(void) {
    VkApplicationInfo appInfo = { 0 };
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Fuselage Runtime";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "GDMF";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const char* extensions[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };

    VkInstanceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    return vkCreateInstance(&createInfo, NULL, &g_vkInstance);
}

static int createVulkanSurface(void) {
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = { 0 };
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hinstance = g_hInstance;
    surfaceCreateInfo.hwnd = g_hWnd;

    PFN_vkCreateWin32SurfaceKHR pfnCreateWin32SurfaceKHR =
        (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(g_vkInstance, "vkCreateWin32SurfaceKHR");

    if (!pfnCreateWin32SurfaceKHR)
        return -1;

    return pfnCreateWin32SurfaceKHR(g_vkInstance, &surfaceCreateInfo, NULL, &g_vkSurface);
}

// We count the devices so we can test them for suitability
int gdmf_enumerate_devices(void) {
    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, NULL);
    if (deviceCount == 0) {
        return -1;
    }

    // Allocate an array for physical devices
    VkPhysicalDevice* devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    if (!devices) return -1;
    vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, devices);

    // Allocate our candidate array
    g_device_candidates = malloc(deviceCount * sizeof(GDMFDeviceCandidate));
    if (!g_device_candidates) {
        free(devices);
        return -1;
    }
    g_device_count = deviceCount;

    // Copy device handles and initialize candidates
    for (uint32_t i = 0; i < deviceCount; i++) {
        g_device_candidates[i].device = devices[i];
        g_device_candidates[i].suitable = false;
        g_device_candidates[i].score = 0;
    }

    free(devices);

    return 0;
}

int gdmf_evaluate_devices(void) {
    if (!g_device_candidates || g_device_count == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < g_device_count; i++) {
        GDMFDeviceCandidate* candidate = &g_device_candidates[i];

        // Get device properties and features
        vkGetPhysicalDeviceProperties(candidate->device, &candidate->properties);
        vkGetPhysicalDeviceFeatures(candidate->device, &candidate->features);
        vkGetPhysicalDeviceMemoryProperties(candidate->device, &candidate->memory_properties);

        // Find queue families and check suitability
        if (find_queue_families(candidate) &&
            check_device_extensions(candidate) &&
            check_surface_support(candidate)) {

            candidate->suitable = true;
            candidate->score = calculate_device_score(candidate);
        }
    }

    return 0;
}

static bool find_queue_families(GDMFDeviceCandidate* candidate) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate->device, &queueFamilyCount, NULL);

    VkQueueFamilyProperties* queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    if (!queueFamilies) return false;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate->device, &queueFamilyCount, queueFamilies);

    bool foundGraphics = false;
    bool foundPresent = false;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        // Check for graphics support
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            candidate->graphics_family = i;
            foundGraphics = true;
        }

        // Check for present support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(candidate->device, i, g_vkSurface, &presentSupport);
        if (presentSupport) {
            candidate->present_family = i;
            foundPresent = true;
        }

        // Early exit if we found both
        if (foundGraphics && foundPresent) {
            break;
        }
    }

    free(queueFamilies);
    return foundGraphics && foundPresent;
}

static bool check_device_extensions(GDMFDeviceCandidate* candidate) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(candidate->device, NULL, &extensionCount, NULL);

    VkExtensionProperties* availableExtensions = malloc(extensionCount * sizeof(VkExtensionProperties));
    if (!availableExtensions) return false;
    vkEnumerateDeviceExtensionProperties(candidate->device, NULL, &extensionCount, availableExtensions);

    // Required extensions for basic functionality
    const char* requiredExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    const uint32_t requiredExtensionCount = 1;

    bool allFound = true;
    for (uint32_t i = 0; i < requiredExtensionCount; i++) {
        bool found = false;
        for (uint32_t j = 0; j < extensionCount; j++) {
            if (strcmp(requiredExtensions[i], availableExtensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            allFound = false;
            break;
        }
    }

    free(availableExtensions);
    return allFound;
}

static bool check_surface_support(GDMFDeviceCandidate* candidate) {
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate->device, g_vkSurface, &capabilities) != VK_SUCCESS) {
        return false;
    }

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate->device, g_vkSurface, &formatCount, NULL);
    if (formatCount == 0) {
        return false;
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate->device, g_vkSurface, &presentModeCount, NULL);
    if (presentModeCount == 0) {
        return false;
    }

    // If we have at least one format and one present mode, the surface is adequate
    return true;
}

static int calculate_device_score(GDMFDeviceCandidate* candidate) {
    int score = 0;

    // Discrete GPUs have a significant advantage over integrated GPUs
    if (candidate->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += candidate->properties.limits.maxImageDimension2D;

    // More memory is generally better
    VkDeviceSize totalMemory = 0;
    for (uint32_t i = 0; i < candidate->memory_properties.memoryHeapCount; i++) {
        if (candidate->memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalMemory += candidate->memory_properties.memoryHeaps[i].size;
        }
    }
    score += (int)(totalMemory / (1024 * 1024));  // Convert to MB for scoring

    // Bonus for having unified graphics/present queue (slightly more efficient)
    if (candidate->graphics_family == candidate->present_family) {
        score += 50;
    }

    return score;
}

GDMFDeviceCandidate* gdmf_select_device(void) {
    if (!g_device_candidates || g_device_count == 0) {
        return NULL;  // No devices to select from
    }

    GDMFDeviceCandidate* bestDevice = NULL;
    int bestScore = -1;

    for (uint32_t i = 0; i < g_device_count; i++) {
        GDMFDeviceCandidate* candidate = &g_device_candidates[i];

        // Only consider suitable devices
        if (candidate->suitable && candidate->score > bestScore) {
            bestScore = candidate->score;
            bestDevice = candidate;
        }
    }

    return bestDevice;  // Returns NULL if no suitable device found
}

static int gdmf_create_logical_device(void) {
    if (!g_selectedDevice) {
        return -1;  // No device selected
    }

    // Set up queue create info
    VkDeviceQueueCreateInfo queueCreateInfos[2];
    uint32_t queueCreateInfoCount = 0;
    float queuePriority = 1.0f;

    // Graphics queue
    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].pNext = NULL;
    queueCreateInfos[0].flags = 0;
    queueCreateInfos[0].queueFamilyIndex = g_selectedDevice->graphics_family;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = &queuePriority;
    queueCreateInfoCount++;

    // Present queue (only add if different from graphics)
    if (g_selectedDevice->graphics_family != g_selectedDevice->present_family) {
        queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[1].pNext = NULL;
        queueCreateInfos[1].flags = 0;
        queueCreateInfos[1].queueFamilyIndex = g_selectedDevice->present_family;
        queueCreateInfos[1].queueCount = 1;
        queueCreateInfos[1].pQueuePriorities = &queuePriority;
        queueCreateInfoCount++;
    }

    // Device extensions
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Device create info
    VkDeviceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.pEnabledFeatures = &g_selectedDevice->features;

    // Create the logical device
    if (vkCreateDevice(g_selectedDevice->device, &createInfo, NULL, &g_vkDevice) != VK_SUCCESS) {
        return -1;
    }

    // Get queue handles
    vkGetDeviceQueue(g_vkDevice, g_selectedDevice->graphics_family, 0, &g_graphicsQueue);
    vkGetDeviceQueue(g_vkDevice, g_selectedDevice->present_family, 0, &g_presentQueue);

    return 0;
}

static int gdmf_create_swapchain(void) {
    // Query swapchain support details
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_selectedDevice->device, g_vkSurface, &capabilities);

    // Choose surface format (prefer BGRA8 SRGB)
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_selectedDevice->device, g_vkSurface, &formatCount, NULL);
    VkSurfaceFormatKHR* formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    if (!formats) return -1;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_selectedDevice->device, g_vkSurface, &formatCount, formats);

    VkSurfaceFormatKHR chosenFormat = formats[0]; // Default to first available
    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = formats[i];
            break;
        }
    }
    free(formats);

    // Choose present mode (prefer FIFO, which is always available)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Choose extent
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    }
    else {
        extent.width = 1280;  // Default window size
        extent.height = 720;
        // Clamp to supported range
        extent.width = extent.width > capabilities.maxImageExtent.width ?
            capabilities.maxImageExtent.width : extent.width;
        extent.width = extent.width < capabilities.minImageExtent.width ?
            capabilities.minImageExtent.width : extent.width;
        extent.height = extent.height > capabilities.maxImageExtent.height ?
            capabilities.maxImageExtent.height : extent.height;
        extent.height = extent.height < capabilities.minImageExtent.height ?
            capabilities.minImageExtent.height : extent.height;
    }

    // Choose image count (prefer one more than minimum for triple buffering)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = g_vkSurface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosenFormat.format;
    createInfo.imageColorSpace = chosenFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { g_selectedDevice->graphics_family, g_selectedDevice->present_family };
    if (g_selectedDevice->graphics_family != g_selectedDevice->present_family) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(g_vkDevice, &createInfo, NULL, &g_vkSwapchain) != VK_SUCCESS) {
        return -1;
    }

    // Store swapchain properties
    g_swapchainImageFormat = chosenFormat.format;
    g_swapchainExtent = extent;

    // Get swapchain images
    vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_swapchainImageCount, NULL);
    g_swapchainImages = malloc(g_swapchainImageCount * sizeof(VkImage));
    if (!g_swapchainImages) return -1;
    vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_swapchainImageCount, g_swapchainImages);

    // Create image views
    g_swapchainImageViews = malloc(g_swapchainImageCount * sizeof(VkImageView));
    if (!g_swapchainImageViews) {
        free(g_swapchainImages);
        return -1;
    }

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkImageViewCreateInfo createInfo = { 0 };
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = g_swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = g_swapchainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(g_vkDevice, &createInfo, NULL, &g_swapchainImageViews[i]) != VK_SUCCESS) {
            // Clean up previously created image views on failure
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyImageView(g_vkDevice, g_swapchainImageViews[j], NULL);
            }
            free(g_swapchainImageViews);
            free(g_swapchainImages);
            return -1;
        }
    }

    return 0;
}

static int gdmf_create_depth_buffer(void) {
    // Create depth image
    VkImageCreateInfo imageInfo = { 0 };
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = g_swapchainExtent.width;
    imageInfo.extent.height = g_swapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = g_depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(g_vkDevice, &imageInfo, NULL, &g_depthImage) != VK_SUCCESS) {
        return -1;
    }

    // Allocate memory for depth image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(g_vkDevice, g_depthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = { 0 };
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memProperties = g_selectedDevice->memory_properties;
    uint32_t memoryType = UINT32_MAX;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memoryType = i;
            break;
        }
    }
    if (memoryType == UINT32_MAX) return -1;
    allocInfo.memoryTypeIndex = memoryType;

    if (vkAllocateMemory(g_vkDevice, &allocInfo, NULL, &g_depthImageMemory) != VK_SUCCESS) {
        return -1;
    }

    vkBindImageMemory(g_vkDevice, g_depthImage, g_depthImageMemory, 0);

    // Create depth image view
    VkImageViewCreateInfo viewInfo = { 0 };
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = g_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = g_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(g_vkDevice, &viewInfo, NULL, &g_depthImageView) != VK_SUCCESS) {
        return -1;
    }

    return 0;
}

static int gdmf_create_render_pass(void) {
    // Color attachment (swapchain image)
    VkAttachmentDescription colorAttachment = { 0 };
    colorAttachment.format = g_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;     // Clear at start
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // Store for presentation
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    VkAttachmentDescription depthAttachment = { 0 };
    depthAttachment.format = g_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;     // Clear depth to 1.0
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't need to keep depth
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachment references
    VkAttachmentReference colorAttachmentRef = { 0 };
    colorAttachmentRef.attachment = 0;  // Index into attachment descriptions
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = { 0 };
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass description
    VkSubpassDescription subpass = { 0 };
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependency for proper synchronization
    VkSubpassDependency dependency = { 0 };
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Create render pass
    VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo = { 0 };
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(g_vkDevice, &renderPassInfo, NULL, &g_renderPass) != VK_SUCCESS) {
        return -1;
    }

    return 0;
}

static int gdmf_create_framebuffers(void) {
    g_swapchainFramebuffers = malloc(g_swapchainImageCount * sizeof(VkFramebuffer));
    if (!g_swapchainFramebuffers) return -1;

    for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
        VkImageView attachments[] = {
            g_swapchainImageViews[i],  // Color attachment
            g_depthImageView           // Depth attachment (shared across all framebuffers)
        };

        VkFramebufferCreateInfo framebufferInfo = { 0 };
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = g_swapchainExtent.width;
        framebufferInfo.height = g_swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(g_vkDevice, &framebufferInfo, NULL, &g_swapchainFramebuffers[i]) != VK_SUCCESS) {
            // Clean up previously created framebuffers on failure
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(g_vkDevice, g_swapchainFramebuffers[j], NULL);
            }
            free(g_swapchainFramebuffers);
            return -1;
        }
    }

    return 0;
}