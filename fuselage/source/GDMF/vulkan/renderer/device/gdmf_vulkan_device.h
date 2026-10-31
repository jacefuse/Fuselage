#ifndef GDMF_VULKAN_DEVICE_H
#define GDMF_VULKAN_DEVICE_H

#include "gdmf_vulkan_physical_device.h"

// Caller-owned handles. Queues belong to device; pipeline_cache may be null.
typedef struct {
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkPipelineCache pipeline_cache;
} gdmf_vulkan_logical_device;

// Main/render thread during initialization. Supply a successful physical-device
// selection and valid output pointer. Returns 0 on success or -1 on failure;
// output is written only on success. Pipeline-cache failure is nonfatal.
// Establish renderer synchronization before concurrent queue/resource work.
int gdmf_vulkan_device_create(const gdmf_vulkan_physical_device_selection* selection,
    gdmf_vulkan_logical_device* logical_device);

// Stop users, finish GPU work, and destroy dependent resources first.
// Releases the cache and device and clears handles. Does not wait for idle.
// Supply a valid pointer containing zeroed handles or a successful result.
void gdmf_vulkan_device_destroy(gdmf_vulkan_logical_device* logical_device);

#endif // GDMF_VULKAN_DEVICE_H
