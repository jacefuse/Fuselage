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

// Windows Window

int g_windowWidth = 0;
int g_windowHeight = 0;
volatile BOOL g_windowResized = FALSE;
volatile BOOL g_inResizeMove = FALSE;

// Command pools and buffers
VkCommandPool g_layerCommandPools[4] = { VK_NULL_HANDLE }; // GDMF_LAYER_COUNT
VkCommandBuffer* g_layerCommandBuffers[4] = { NULL };      // [layer] -> array of command buffers

LRESULT CALLBACK GDMFWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool createWindowsWindow(void);

// Main facing functions
int GDMFinit(void) {
    // Window creation segment for Windows
    // For now we're making a basic Window. This will be refactored.
    // This will be replaced with a version for every platform and
    // well be enhanced to better handle future requirements.
    printf("Fuselage version %s\n", FUSELAGE_GDMF_VERSION);
    printf("[GDMF INIT]\n");

    if (createWindowsWindow() != 0) {
        printf("[Window creation]\nFailed to create Window.\n");
        return -1;
    }
    
    // Vulkanic Start
    if (gdmfCreateVulkanInstance() != VK_SUCCESS) return -1;
    if (gdmfCreateVulkanSurface() != VK_SUCCESS) return -1;

    // Enumerate and evaluate devices to select the most suitable
    if (gdmfEnumerateDevices() != 0) return -1;
    if (gdmfEvaluateDevices() != 0) return -1;

    GDMFDeviceCandidate* selectedDevice = gdmfSelectDevice();
    if (!selectedDevice) return -1;  // No suitable device found

    // Select device
    g_selectedDevice = selectedDevice;
    if (gdmfCreateLogicalDevice() != 0) return -1;

    // Create swapchain
    if (gdmfCreateSwapchain() != 0) return -1;

    // Create render pass
    if (gdmfCreateRenderPass() != 0) return -1;

    if (gdmfCreateDepthBuffer() != 0) return -1;

    // Create framebuffers
    if (gdmfCreateFrameBuffers() != 0) return -1;

    // Create command pools and buffers
    if (gdmf_create_command_pools() != 0) return -1;

    if (gdmf_create_command_buffers() != 0) return -1;

    // Create synchronization objects
    if (gdmf_create_sync_objects() != 0) return -1;

    // Enable sync profiling for debugging (can be disabled later)
    gdmf_enable_sync_profiling(false);

    // Print initial layer status
    gdmf_print_layer_status();

    //SetupCharacterMaps(); // Now handled in ensure_text_pipeline();

    return 0;
}

// Cleanup happens in the reverse order of Init
void GDMFshutdown(void) {
    printf("[GDMF SHUTDOWN]\n");

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
        printf("Frame buffers freed.\n");
    }

    // Destroy depth buffer
    if (g_depthImageView) {
        vkDestroyImageView(g_vkDevice, g_depthImageView, NULL);
        g_depthImageView = VK_NULL_HANDLE;
        printf("Depth Image View destroyed. | ");
    }
    if (g_depthImageMemory) {
        vkFreeMemory(g_vkDevice, g_depthImageMemory, NULL);
        g_depthImageMemory = VK_NULL_HANDLE;
        printf("Depth Image Memory destroyed. | ");
    }    
    if (g_depthImage) {
        vkDestroyImage(g_vkDevice, g_depthImage, NULL);
        g_depthImage = VK_NULL_HANDLE;
        printf("Depth Image destroyed.\n");
    }

    // Destroy render pass
    if (g_renderPass) {
        vkDestroyRenderPass(g_vkDevice, g_renderPass, NULL);
        g_renderPass = VK_NULL_HANDLE;
        printf("Render pass destroyed.\n");
    }

    // Clean up swapchain image views
    if (g_swapchainImageViews) {
        for (uint32_t i = 0; i < g_swapchainImageCount; i++) {
            vkDestroyImageView(g_vkDevice, g_swapchainImageViews[i], NULL);
        }
        free(g_swapchainImageViews);
        g_swapchainImageViews = NULL;
        printf("Swapchain Image Views destroyed. | ");
    }

    // Clean up swapchain images array (the images themselves are owned by swapchain)
    if (g_swapchainImages) {
        free(g_swapchainImages);
        g_swapchainImages = NULL;
        printf("Swapchain Images destroyed. | ");
    }

    // Destroy Swapchain
    if (g_vkSwapchain) {
        vkDestroySwapchainKHR(g_vkDevice, g_vkSwapchain, NULL);
        g_vkSwapchain = VK_NULL_HANDLE;
        printf("Swapchain destroyed.\n");
    }

    // Destroy Device
    if (g_vkDevice) {
        vkDestroyDevice(g_vkDevice, NULL);
        g_vkDevice = VK_NULL_HANDLE;
        printf("Device freed. | ");
    }

    // Release Candidates
    if (g_device_candidates) {
        free(g_device_candidates);
        g_device_candidates = NULL;
        g_device_count = 0;
        printf("Device Candidates freed.\n");
    }

    // Poof
    if (g_vkSurface) {
        PFN_vkDestroySurfaceKHR pfnDestroySurfaceKHR =
            (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(g_vkInstance, "vkDestroySurfaceKHR");
        if (pfnDestroySurfaceKHR)
            pfnDestroySurfaceKHR(g_vkInstance, g_vkSurface, NULL);
        printf("Surface destroyed. | ");
    }

    // Gone
    if (g_vkInstance) {
        vkDestroyInstance(g_vkInstance, NULL);
        printf("Instance destroyed. | ");
    }
    // Close the window
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        printf("Window closed.\n");
    }
    UnregisterClass(TEXT("GDMFWindowClass"), g_hInstance);

    printf("[GDMF OVER]\n");

    return;
}

bool createWindowsWindow(void) {
    // Star Windows Creation Segment
    g_hInstance = GetModuleHandle(NULL);

    WNDCLASS wc = { 0 };
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GDMFWindowProc; // Our Window callback
    wc.hInstance = g_hInstance;
    wc.lpszClassName = TEXT("GDMFWindowClass");

    if (SetThreadDpiAwarenessContext) {
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    else {
        // Fallback (older Windows 10)
        SetProcessDPIAware();
    }

    if (!RegisterClass(&wc)) return -1;

    g_hWnd = CreateWindowEx(0, wc.lpszClassName, TEXT("GDMF Window"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        NULL, NULL, g_hInstance, NULL);
    if (!g_hWnd) {
        printf("Failed to create window.\n");
        return -1;
    }
    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hWnd);
    printf("Window created.\n");

    return 0;
    // End Window creation segment
}

LRESULT CALLBACK GDMFWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        // New client size in pixels
        int w = (int)LOWORD(lParam);
        int h = (int)HIWORD(lParam);
        if (w > 0 && h > 0) {
            g_windowWidth = w;
            g_windowHeight = h;

            // If the user is actively resizing, defer heavy work;
            // otherwise signal the renderer to rebuild swapchain.
            if (!g_inResizeMove) {
                g_windowResized = TRUE;
            }
        }
        return 0;
    }

    case WM_ENTERSIZEMOVE:
        g_inResizeMove = TRUE;
        return 0;

    case WM_EXITSIZEMOVE:
        g_inResizeMove = FALSE;
        // One final resize event after the drag/recreate swapchain once.
        g_windowResized = TRUE;
        return 0;

    case WM_DPICHANGED: {
        // lParam points to a suggested new window rect to keep the same client size at new DPI
        if (lParam) {
            RECT* const suggested = (RECT*)lParam;
            SetWindowPos(hWnd, NULL,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }

    case WM_GETMINMAXINFO: {
        // Optional: set a sane minimum client size so UI/text never collapses
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 360;
        return 0;
    }

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

int GDMFrenderFrame(void) {
    static uint32_t frameCount = 0;

    int result = gdmf_render_complete_frame();

    if (result == 0) {
        frameCount++;
        // Print stats every 120 frames 
        if (frameCount % 120 == 0 && gdmf_is_sync_profiling_enabled()) {
            gdmf_print_sync_stats();
        }
    }

    return result;
}

