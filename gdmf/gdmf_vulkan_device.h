#pragma once

#include "gdmf_internal.h"

// Public device management functions
VkInstance gdmf_get_instance(void);
VkSurfaceKHR gdmf_get_surface(void);

int gdmf_enumerate_devices(void);
int gdmf_evaluate_devices(void);
GDMFDeviceCandidate* gdmf_select_device(void);

// Device creation functions (called from gdmf.c)
int gdmf_create_vulkan_instance(void);
int gdmf_create_vulkan_surface(void);
int gdmf_create_logical_device(void);
int gdmf_create_swapchain(void);
int gdmf_create_render_pass(void);
int gdmf_create_depth_buffer(void);
int gdmf_create_framebuffers(void);

// Synchronization system (integration with gdmf_vulkan_sync.c)
//int gdmf_create_sync_objects(void);
//void gdmf_destroy_sync_objects(void);

// Internal helper functions
bool find_queue_families(GDMFDeviceCandidate* candidate);
bool check_device_extensions(GDMFDeviceCandidate* candidate);
bool check_surface_support(GDMFDeviceCandidate* candidate);
int calculate_device_score(GDMFDeviceCandidate* candidate);
uint32_t gdmf_find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);
