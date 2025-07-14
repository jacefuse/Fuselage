#pragma once

#include <windows.h>
#include <tchar.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan.h>
#include <vulkan_win32.h>

// Devices
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

// Global state declarations (extern - defined in gdmf.c)
extern HWND g_hWnd;
extern HINSTANCE g_hInstance;
extern VkInstance g_vkInstance;
extern VkSurfaceKHR g_vkSurface;
extern GDMFDeviceCandidate* g_device_candidates;
extern GDMFDeviceCandidate* g_selectedDevice;
extern uint32_t g_device_count;
extern VkDevice g_vkDevice;
extern VkQueue g_graphicsQueue;
extern VkQueue g_presentQueue;
extern VkSwapchainKHR g_vkSwapchain;
extern VkFormat g_swapchainImageFormat;
extern VkExtent2D g_swapchainExtent;
extern VkImage* g_swapchainImages;
extern VkImageView* g_swapchainImageViews;
extern uint32_t g_swapchainImageCount;
extern VkImage g_depthImage;
extern VkDeviceMemory g_depthImageMemory;
extern VkImageView g_depthImageView;
extern VkFormat g_depthFormat;
extern VkRenderPass g_renderPass;
extern VkFramebuffer* g_swapchainFramebuffers;

// Command pools and buffers (one per layer)
extern VkCommandPool g_layerCommandPools[4];           // GDMF_LAYER_COUNT
extern VkCommandBuffer* g_layerCommandBuffers[4];      // [layer] -> array of command buffers per frame