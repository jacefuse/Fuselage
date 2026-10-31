#ifndef GDMF_VULKAN_PALETTES_H
#define GDMF_VULKAN_PALETTES_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

#define GDMF_PALETTE_BUFFER_SIZE (256 * 16 * sizeof(uint32_t))

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
    uint32_t uploaded_generation;
    bool uploaded;
} gdmf_vulkan_palette_slot;

// Caller-owned per-image storage; mappings remain valid until destruction.
typedef struct {
    uint32_t image_count;
    gdmf_vulkan_palette_slot* slots;
} gdmf_vulkan_palettes;

// Main/render thread during setup. Supply matching device/memory properties
// and a valid output pointer. Returns 0 on success
// or -1 on failure, including zero images. Cleans up partial resources and
// writes output only on success. Do not overwrite a live collection.
int gdmf_vulkan_palettes_create(VkDevice device,
    const VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t image_count,
    gdmf_vulkan_palettes* palettes);

// After this image's prior GPU work completes, under the application update
// lock, refresh from Colors if needed. Invalid/unmapped slots are ignored.
void gdmf_vulkan_palettes_prepare(gdmf_vulkan_palettes* palettes, uint32_t image_index);

// Borrowed buffer, or VK_NULL_HANDLE for an unavailable/invalid image slot.
VkBuffer gdmf_vulkan_palettes_get_buffer(const gdmf_vulkan_palettes* palettes,
    uint32_t image_index);

// Stop users and finish GPU work first. Supply a zeroed or successful result.
// Clears the result. Does not wait; borrowed descriptor users must be retired.
void gdmf_vulkan_palettes_destroy(VkDevice device, gdmf_vulkan_palettes* palettes);

#endif // GDMF_VULKAN_PALETTES_H
