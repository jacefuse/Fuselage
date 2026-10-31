#ifndef GDMF_VULKAN_CAPTURE_READBACK_H
#define GDMF_VULKAN_CAPTURE_READBACK_H

#include "../resources/gdmf_vulkan_uploads.h"

typedef struct {
    VkImage image;
    VkFormat format;
    VkExtent2D extent;
    VkImageUsageFlags image_usage;
    VkRect2D viewport;
    VkExtent2D canvas_extent;
} gdmf_vulkan_capture_source;

// Caller-owned context with borrowed device/upload service and copied properties.
// Initialize retained handles to null. Keep all borrowed objects valid through
// normal work and recovery; do not copy this context to create a second owner.
typedef struct {
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory_properties;
    gdmf_vulkan_uploads* uploads;
    VkBuffer retained_buffer;
    VkDeviceMemory retained_memory;
} gdmf_vulkan_capture_readback;

typedef enum {
    GDMF_CAPTURE_READBACK_SAVED = 0,
    GDMF_CAPTURE_READBACK_FAILED = -1,
    GDMF_CAPTURE_READBACK_UNRESOLVED = -2
} gdmf_vulkan_capture_readback_result;

// Render thread only, outside upload/queue locks. Source must be application-
// owned in PRESENT_SRC_KHR layout, with no active presentation or competing use.
// An idle wait is not a substitute for this ownership precondition.
// Returns SAVED only after file output succeeds, FAILED after a resolved failure,
// or UNRESOLVED with readback storage retained. Preserve the source image and
// its parent swapchain/resources until unresolved work is recovered.
// SEE: (GDMF_VULKAN.md, Capture readback).
gdmf_vulkan_capture_readback_result gdmf_vulkan_capture_readback_write(
    gdmf_vulkan_capture_readback* readback, const gdmf_vulkan_capture_source* source,
    const char* path);

// Stop producers and establish completion externally. Release retained upload
// commands first, then call this to release their readback buffer and memory.
// Performs no wait. Same render-thread consumer; borrowed objects remain valid.
void gdmf_vulkan_capture_readback_release_completed(gdmf_vulkan_capture_readback* readback);

#endif // GDMF_VULKAN_CAPTURE_READBACK_H
