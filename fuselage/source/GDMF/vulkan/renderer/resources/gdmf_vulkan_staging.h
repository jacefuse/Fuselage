#ifndef GDMF_VULKAN_STAGING_H
#define GDMF_VULKAN_STAGING_H

#include <vulkan/vulkan.h>

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
    VkDeviceSize capacity;
    VkDeviceSize used;
    VkDeviceSize wanted;
} gdmf_vulkan_staging_slot;

// Caller-owned slots with copied memory properties and a borrowed device.
typedef struct {
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory_properties;
    uint32_t image_count;
    gdmf_vulkan_staging_slot* slots;
} gdmf_vulkan_staging;

// Setup on the main/render thread with matching device/properties and valid
// output. Returns 0 on success or -1 on failure, including zero images.
// Allocates bookkeeping only. Output changes only on success; do not overwrite
// a live collection. The device must remain valid throughout its lifetime.
int gdmf_frame_staging_create(VkDevice device,
    const VkPhysicalDeviceMemoryProperties* memory_properties,
    uint32_t image_count, gdmf_vulkan_staging* staging);

// Render thread, after this image's prior GPU work completes and before any
// allocations for its next frame. May grow storage; invalid indices are ignored.
void gdmf_frame_staging_reset(gdmf_vulkan_staging* staging, uint32_t image_index);

// During preparation, after reset. Returns mapped storage and buffer/offset
// outputs, or NULL for invalid input, overflow, or unavailable capacity/memory.
// Outputs change only on success. First use may allocate; insufficient capacity
// records demand for growth at reset. Keep failed work pending for retry.
// SEE: (GDMF_VULKAN.md, Frame staging).
void* gdmf_frame_staging_alloc(gdmf_vulkan_staging* staging, uint32_t image_index,
    VkDeviceSize bytes, VkBuffer* buffer, VkDeviceSize* offset);

// Stop users and finish GPU work first. Supply a zeroed or successful result.
// Releases storage, clears the result, and does not wait.
void gdmf_frame_staging_destroy(gdmf_vulkan_staging* staging);

#endif // GDMF_VULKAN_STAGING_H
