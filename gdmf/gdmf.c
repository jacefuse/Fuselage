// gdmf.c - Minimal Vulkan Bootstrap for Fuselage on Windows (other systems eventually)
// See header for more details

#include "gdmf.h"

// Global device and Vk states
HWND g_hWnd = NULL;
HINSTANCE g_hInstance = NULL;
VkInstance g_vkInstance = VK_NULL_HANDLE;
VkSurfaceKHR g_vkSurface = VK_NULL_HANDLE;
GDMFDeviceCandidate* g_device_candidates = NULL;
GDMFDeviceCandidate* g_selectedDevice = NULL;
uint32_t g_device_count = 0;
VkDevice g_vkDevice = VK_NULL_HANDLE;
VkQueue g_graphicsQueue = VK_NULL_HANDLE;
VkQueue g_presentQueue = VK_NULL_HANDLE;
VkSwapchainKHR g_vkSwapchain = VK_NULL_HANDLE;
VkFormat g_swapchainImageFormat;
VkExtent2D g_swapchainExtent;
VkImage* g_swapchainImages = NULL;
VkImageView* g_swapchainImageViews = NULL;
uint32_t g_swapchainImageCount = 0;
VkImage g_depthImage = VK_NULL_HANDLE;
VkDeviceMemory g_depthImageMemory = VK_NULL_HANDLE;
VkImageView g_depthImageView = VK_NULL_HANDLE;
VkFormat g_depthFormat = VK_FORMAT_D32_SFLOAT;
VkRenderPass g_renderPass = VK_NULL_HANDLE;
VkFramebuffer* g_swapchainFramebuffers = NULL;

// Command pools and buffers
VkCommandPool g_layerCommandPools[4] = { VK_NULL_HANDLE }; // GDMF_LAYER_COUNT
VkCommandBuffer* g_layerCommandBuffers[4] = { NULL };      // [layer] -> array of command buffers

LRESULT CALLBACK GDMFWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// Main facing functions
int GDMFinit(void) {

    // Window creation segment for Windows
    // For now we're making a basic Window. This will be refactored.
    // This will be replaced with a version for every platform and
    // well be enhanced to better handle future requirements.
    g_hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = GDMFWindowProc; // Our Window callback
    wc.hInstance = g_hInstance;
    wc.lpszClassName = TEXT("GDMFWindowClass");

    if (!RegisterClass(&wc)) return -1;

    g_hWnd = CreateWindowEx(0, wc.lpszClassName, TEXT("GDMF Window"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        NULL, NULL, g_hInstance, NULL);
    if (!g_hWnd) return -1;
    
    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hWnd);
    // End Window creation segment

    printf("[GDMF Init]\n");
    
    // Vulkanic Start
    if (gdmf_create_vulkan_instance() != VK_SUCCESS) return -1;
    if (gdmf_create_vulkan_surface() != VK_SUCCESS) return -1;

    // Enumerate and evaluate devices to select the most suitable
    if (gdmf_enumerate_devices() != 0) return -1;
    if (gdmf_evaluate_devices() != 0) return -1;

    GDMFDeviceCandidate* selectedDevice = gdmf_select_device();
    if (!selectedDevice) return -1;  // No suitable device found

    // Select device
    g_selectedDevice = selectedDevice;
    if (gdmf_create_logical_device() != 0) return -1;

    // Create swapchain
    if (gdmf_create_swapchain() != 0) return -1;

    // Create render pass
    //if (gdmf_create_render_pass() != 0) return -1;
    //printf("Render pass created.\n");
    //if (gdmf_create_depth_buffer() != 0) return -1;
    //printf("Depth buffer created.\n");
    // Create framebuffers
    //if (gdmf_create_framebuffers() != 0) return -1;
    //printf("Framebuffers created.\n");

    // Create command pools and buffers
    //if (gdmf_create_command_pools() != 0) return -1;
    //printf("Command pools created.\n");
    //if (gdmf_create_command_buffers() != 0) return -1;
    //printf("Command buffers created.\n");

    // Create synchronization objects
    //if (gdmf_create_sync_objects() != 0) return -1;
    //printf("Synchronization objects created.\n");

    // Enable sync profiling for debugging (can be disabled later)
    //gdmf_enable_sync_profiling(false);

    // Print initial layer status
    //gdmf_print_layer_status();

    //SetupCharacterMaps();

    return 0;
}

// Cleanup happens in the reverse order of Init
void GDMFshutdown(void) {
    /*
    ShutdownCharacterMaps();

    printf("[GDMF Sync] Final performance stats before shutdown:\n");
    gdmf_print_sync_stats();

    // Destroy synchronization objects
    gdmf_destroy_sync_objects();

    // Destroy command pools and buffers
    gdmf_destroy_command_pools();

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

    */// Clean up swapchain image views
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

/*/ Synchronization-aware render loop function
int GDMFrenderFrame(void) {
    static uint32_t frameCount = 0;

    // Get current frame index for synchronization
    uint32_t frameIndex = gdmf_get_current_frame_index();

    // Wait for this frame to be ready (fence signaled)
    if (gdmf_wait_for_frame(frameIndex) != 0) {
        printf("[GDMF] Failed to wait for frame %d\n", frameIndex);
        return -1;
    }

    // Acquire next swapchain image
    uint32_t imageIndex;
    int acquireResult = gdmf_acquire_next_image(frameIndex, &imageIndex);
    if (acquireResult < 0) {
        printf("[GDMF] Failed to acquire swapchain image\n");
        return -1;
    }
    else if (acquireResult > 0) {
        // Swapchain needs recreation
        printf("[GDMF] Swapchain recreation needed - not implemented yet\n");
        return 1;
    }

    // Submit frame with proper synchronization
    if (gdmf_submit_frame(frameIndex, imageIndex) != 0) {
        printf("[GDMF] Failed to submit frame %d\n", frameIndex);
        return -1;
    }

    // Present frame
    int presentResult = gdmf_present_frame(frameIndex, imageIndex);
    if (presentResult < 0) {
        printf("[GDMF] Failed to present frame\n");
        return -1;
    }
    else if (presentResult > 0) {
        // Swapchain suboptimal
        printf("[GDMF] Swapchain suboptimal - recreation recommended\n");
    }

    // Signal frame completion and advance to next frame
    gdmf_signal_frame_complete(frameIndex);
    gdmf_render_complete_frame();
    frameCount++;

    // Print stats every 120 frames (approximately every 2 seconds at 60fps)
    if (frameCount % 120 == 0 && gdmf_is_sync_profiling_enabled()) {
        gdmf_print_sync_stats();
    }

    return 0;
}//*/