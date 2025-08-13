#include "gdmf.h"
#include "gdmf_vulkan_device.h"

// Public getter functions
VkInstance gdmfGetInstance(void) { return g_vkInstance; }
VkSurfaceKHR gdmfGetSurface(void) { return g_vkSurface; }

// Internal helper functions
bool findQueueFamilies(GDMFDeviceCandidate* candidate);
bool checkDeviceExtensions(GDMFDeviceCandidate* candidate);
bool checkSurfaceSupport(GDMFDeviceCandidate* candidate);
int calculateDeviceScore(GDMFDeviceCandidate* candidate);

// Vulkan instance
int gdmfCreateVulkanInstance(void) {
    // Query loader's Vulkan version
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
    if (pfnEnumerateInstanceVersion) {
        if (pfnEnumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) {
            loaderVersion = VK_API_VERSION_1_0;
        }
    }

    // Target 1.2 if available, else use loader version
    uint32_t targetApiVersion =
        (VK_API_VERSION_MAJOR(loaderVersion) > 1 ||
            (VK_API_VERSION_MAJOR(loaderVersion) == 1 && VK_API_VERSION_MINOR(loaderVersion) >= 2))
        ? VK_API_VERSION_1_2
        : loaderVersion;

    // App info
    VkApplicationInfo appInfo = { 0 };
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Fuselage Runtime"; // TODO: Replace this with a developer edited definition
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Fuselage VM";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = targetApiVersion;

    // Enumerate instance extensions
    uint32_t extCount = 0;
    if (vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL) != VK_SUCCESS || extCount == 0) {
        printf("[Instance] Failed to enumerate instance extensions.\n");
        return -1;
    }
    VkExtensionProperties* exts = malloc(sizeof(VkExtensionProperties) * extCount);
    if (!exts) {
        printf("[Instance] Out of memory for extension list.\n");
        return -1;
    }
    if (vkEnumerateInstanceExtensionProperties(NULL, &extCount, exts) != VK_SUCCESS) {
        printf("[Instance] Failed to get extension properties.\n");
        free(exts);
        return -1;
    }

    // Required extensions
    const char* requiredExts[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
        // TODO: Add more extensions here if future layers need them
    };
    const uint32_t requiredExtCount = sizeof(requiredExts) / sizeof(requiredExts[0]);

    // Check availability
    const char* enabledExts[MAX_EXTS];
    uint32_t enabledExtCount = 0;

    for (uint32_t r = 0; r < requiredExtCount; r++) {
        int found = 0;
        for (uint32_t i = 0; i < extCount; i++) {
            if (strcmp(requiredExts[r], exts[i].extensionName) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("[Instance] Missing required extension: %s\n", requiredExts[r]);
            free(exts);
            return -1;
        }
        enabledExts[enabledExtCount++] = requiredExts[r];
    }

    free(exts);

    // Create instance
    VkInstanceCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = enabledExtCount;
    createInfo.ppEnabledExtensionNames = enabledExts;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;

    VkResult result = vkCreateInstance(&createInfo, NULL, &g_vkInstance);
    if (result != VK_SUCCESS) {
        printf("[Instance] vkCreateInstance failed (VkResult=%d)\n", result);
        return -1;
    }

    printf("[Instance] Created. API %u.%u\n", 
        VK_API_VERSION_MAJOR(targetApiVersion),
        VK_API_VERSION_MINOR(targetApiVersion));

    return VK_SUCCESS;
}

// Vulkan surface
int gdmfCreateVulkanSurface(void) {
    if (!g_vkInstance) { printf("[Surface] No Vulkan instance.\n"); return -1; }
    if (!g_hInstance || !g_hWnd) { printf("[Surface] Invalid Win32 handles.\n"); return -1; }

    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = { 0 };
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = NULL;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.hinstance = g_hInstance;
    surfaceCreateInfo.hwnd = g_hWnd;

    PFN_vkCreateWin32SurfaceKHR pfnCreateWin32SurfaceKHR =
        (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(g_vkInstance, "vkCreateWin32SurfaceKHR");

    if (!pfnCreateWin32SurfaceKHR) {
        printf("[Surface] vkCreateWin32SurfaceKHR not found (extension missing?).\n");
        return -1;
    }

    VkResult result = pfnCreateWin32SurfaceKHR(g_vkInstance, &surfaceCreateInfo, NULL, &g_vkSurface);
    if (result != VK_SUCCESS) {
        printf("[Surface] CreateWin32Surface failed (VkResult=%d).\n", result);
        return result; // preserves current contract
    }

    printf("[Surface] Vulkan surface created.\n");

    return result; // VK_SUCCESS
}

// Device enumeration and evaluation
int gdmfEnumerateDevices(void) {

    if (!g_vkInstance) {
        printf("[Devices] No Vulkan instance.\n");
        return -1;
    }

    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, NULL);
    if (result != VK_SUCCESS) {
        printf("[Devices] vkEnumeratePhysicalDevices(count) failed: %d\n", result);
        return -1;
    }
    if (deviceCount == 0) {
        printf("[Devices] No Vulkan-capable devices found.\n");
        return -1;
    }

    // Allocate an array for physical devices
    VkPhysicalDevice* devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    if (!devices) {
        printf("[Devices] Out of memory (devices list)\n");
        return -1;
    }
    result = vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, devices);
    if (result != VK_SUCCESS) {
        printf("[Devices] vkEnumeratePhysicalDevices(list) failed: %d\n", result);
        free(devices);
        return -1;
    }

    // Allocate our candidate array
    g_device_candidates = (GDMFDeviceCandidate*)malloc(deviceCount * sizeof(GDMFDeviceCandidate));
    if (!g_device_candidates) {
        printf("[Devices] Out of memory (candidates)\n");
        free(devices);
        return -1;
    }
    g_device_count = deviceCount;

    for (uint32_t i = 0; i < deviceCount; i++) {
        g_device_candidates[i].device = devices[i];
        g_device_candidates[i].graphics_family = UINT32_MAX;
        g_device_candidates[i].present_family = UINT32_MAX;
        g_device_candidates[i].suitable = false;
        g_device_candidates[i].score = 0;
    }

    free(devices);

    printf("[Devices] Found %u device(s).\n", g_device_count);

    return result;
}

// Evaluation
int gdmfEvaluateDevices(void) {
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
        if (findQueueFamilies(candidate) &&
            checkDeviceExtensions(candidate) &&
            checkSurfaceSupport(candidate)) {

            candidate->suitable = true;
            candidate->score = calculateDeviceScore(candidate);
        }
    }

    return 0;
}

bool findQueueFamilies(GDMFDeviceCandidate* candidate) {
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

bool checkDeviceExtensions(GDMFDeviceCandidate* candidate) {
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

bool checkSurfaceSupport(GDMFDeviceCandidate* candidate) {
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

int calculateDeviceScore(GDMFDeviceCandidate* candidate) {
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

GDMFDeviceCandidate* gdmfSelectDevice(void) {
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

int gdmfCreateLogicalDevice(void) {
    if (!g_selectedDevice) {
        printf("[Devices] No devices selected.\n");
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
        printf("[Devices] Cound not create logical device.\n");
        return -1;
    }

    // Get queue handles
    vkGetDeviceQueue(g_vkDevice, g_selectedDevice->graphics_family, 0, &g_graphicsQueue);
    vkGetDeviceQueue(g_vkDevice, g_selectedDevice->present_family, 0, &g_presentQueue);

    printf("[Devices] Selected Device: %s (Type: %d, Score: %d)\n",
        g_selectedDevice->properties.deviceName,
        g_selectedDevice->properties.deviceType,
        g_selectedDevice->score);
    return 0;
}

int gdmfCreateSwapchain(void) {
    // Query swapchain support details
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_selectedDevice->device, g_vkSurface, &capabilities);
    printf("[Swapchain]\n");
    // Choose surface format (prefer BGRA8 SRGB)
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_selectedDevice->device, g_vkSurface, &formatCount, NULL);
    VkSurfaceFormatKHR* formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    if (!formats) {
        printf("FAILURE: No supported surface formats available.\n");
        return -1;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_selectedDevice->device, g_vkSurface, &formatCount, formats);

    VkSurfaceFormatKHR chosenFormat = formats[0]; // Default to first available
    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = formats[i];
            printf("Chosen formats: B8G8R8A8_SRGB & SRGB_NONLINEAR\n");
            break;
        }
    }
    free(formats);

    // TODO: add selection.
    // Currently using FIFO but will later create a hierarchical
    // selection process for VK_PRESENT_MODE_MAILBOX_KHR first
    // and then only falling down to FIFO if MAILBOX is unavailable.
    // VK_PRESENT_MODE_IMMEDIATE_KHR will always be a last resort
    // with a warning for degraded image stability (e.g. screen tearing)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    // Choose extent
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    }
    else {
        extent.width = 1280;  // Default window size
        extent.height = 720;

        // Clamp extent.width to be within min and max limits
        if (extent.width > capabilities.maxImageExtent.width) {
            extent.width = capabilities.maxImageExtent.width;
        }
        else if (extent.width < capabilities.minImageExtent.width) {
            extent.width = capabilities.minImageExtent.width;
        }

        if (extent.height > capabilities.maxImageExtent.height) {
            extent.height = capabilities.maxImageExtent.height;
        }
        else if (extent.height < capabilities.minImageExtent.height) {
            extent.height = capabilities.minImageExtent.height;
        }
    }

    // Choose image count (prefer one more than minimum for triple buffering)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    printf("%d images created for swapchain.\n", imageCount);

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
        printf("Concurrent mode selected.\n");
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        printf("Exclusive mode selected.\n");
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(g_vkDevice, &createInfo, NULL, &g_vkSwapchain) != VK_SUCCESS) {
        printf("Swapchain not created.\n");
        return -1;
    }

    // Store swapchain properties
    g_swapchainImageFormat = chosenFormat.format;
    g_swapchainExtent = extent;

    // Get swapchain images
    vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_swapchainImageCount, NULL);
    g_swapchainImages = malloc(g_swapchainImageCount * sizeof(VkImage));
    if (!g_swapchainImages) { 
        printf("No swapchain images available.\n");
        return -1; }
    vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_swapchainImageCount, g_swapchainImages);

    // Create image views
    g_swapchainImageViews = malloc(g_swapchainImageCount * sizeof(VkImageView));
    if (!g_swapchainImageViews) {
        free(g_swapchainImages);
        printf("Could not create image views.\n");
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
            printf("Could not create swapchain.\n");
            return -1;
        }
    }

    printf("Swapchain created.\n");
    return 0;
}

int gdmfCreateDepthBuffer(void) {
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
        printf("[Depth Buffer] Failed to create depth image.\n");
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
        printf("[Depth Buffer] Failed to allocate depth buffer.\n");
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
        printf("[Depth Buffer] Failed to create image view.\n");
        if (g_depthImage) {
            vkDestroyImage(g_vkDevice, g_depthImage, NULL);
            g_depthImage = VK_NULL_HANDLE;
        }
        if (g_depthImageMemory) {
            vkFreeMemory(g_vkDevice, g_depthImageMemory, NULL);
            g_depthImageMemory = VK_NULL_HANDLE;
        }
        return -1;
    }

    printf("[Depth Buffer] Depth buffer created.\n");

    return 0;
}

int gdmfCreateRenderPass(void) {
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
    printf("[Render Pass]\nUsing image format: %d\n", g_swapchainImageFormat);
    // TODO: make human readable image format output.

    // Depth attachment
    VkFormatProperties formatProps;
    VkFormat candidateFormats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };
    int formatCount = sizeof(candidateFormats) / sizeof(candidateFormats[0]);
    bool formatFound = false;
    
    VkAttachmentDescription depthAttachment = { 0 };
    depthAttachment.format = g_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;     // Clear depth to 1.0
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't need to keep depth
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    for (int i = 0; i < formatCount; i++) {
        vkGetPhysicalDeviceFormatProperties(g_selectedDevice->device, candidateFormats[i], &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            g_depthFormat = candidateFormats[i];
            formatFound = true;
            printf("Using depth format: %d\n", g_depthFormat);
            break;
        }
    }
    // TODO: make human readable depth format output.

    if (!formatFound) {
        printf("No suitable depth format found\n");
        return -1;
    }

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
        printf("Failed to create render pass.\n");
        return -1;
    }

    return 0;
}

int gdmfCreateFrameBuffers(void) {

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
            printf("[Frame Buffers] Failed to create frame buffers.\n");
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(g_vkDevice, g_swapchainFramebuffers[j], NULL);
            }
            free(g_swapchainFramebuffers);
            return -1;
        }
    }

    printf("[Frame Buffers] %d frame buffers created.\n", g_swapchainImageCount);

    return 0;
}

uint32_t gdmfFindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    if (!g_selectedDevice) {
        printf("[GDMF Device] No device selected for memory type search\n");
        return UINT32_MAX;
    }

    VkPhysicalDeviceMemoryProperties mem_properties = g_selectedDevice->memory_properties;

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    printf("[GDMF Device] Failed to find suitable memory type (filter: 0x%x, properties: 0x%x)\n",
        type_filter, properties);
    return UINT32_MAX;
}