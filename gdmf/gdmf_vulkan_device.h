#pragma once

//#include "gdmf_internal.h"

#define MAX_EXTS 8

// Public device management functions
VkInstance gdmfGetInstance(void);
VkSurfaceKHR gdmfGetSurface(void);
GDMFDeviceCandidate* gdmfSelectDevice(void);
uint32_t gdmfFindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

// Device creation functions (called from gdmf.c)
int gdmfCreateVulkanInstance(void);
int gdmfCreateVulkanSurface(void);
int gdmfCreateLogicalDevice(void);
int gdmfCreateSwapchain(void);
int gdmfCreateRenderPass(void);
int gdmfCreateDepthBuffer(void);
int gdmfCreateFrameBuffers(void);
int gdmfEnumerateDevices(void);
int gdmfEvaluateDevices(void);
