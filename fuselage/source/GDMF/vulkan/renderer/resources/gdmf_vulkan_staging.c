// Per-image storage for transfers and compute inputs during frame preparation.
// SEE: (GDMF_VULKAN.md, Frame staging).
#include "gdmf_vulkan_staging.h"
#include "gdmf_vulkan_memory.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GDMF_FRAME_STAGING_ALIGNMENT 16
#define GDMF_FRAME_STAGING_MINIMUM 65536

static void gdmf_frame_staging_release(VkDevice device, gdmf_vulkan_staging_slot* slot) {
    if (slot->mapped) { vkUnmapMemory(device, slot->memory); }
    if (slot->buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, slot->buffer, NULL); }
    if (slot->memory != VK_NULL_HANDLE) { vkFreeMemory(device, slot->memory, NULL); }
    *slot = (gdmf_vulkan_staging_slot){0};
}

void gdmf_frame_staging_destroy(gdmf_vulkan_staging* staging) {
    if (staging->slots) {
        for (uint32_t image_index = 0; image_index < staging->image_count; image_index++) {
            gdmf_frame_staging_release(staging->device, &staging->slots[image_index]);
        }
        free(staging->slots);
    }
    *staging = (gdmf_vulkan_staging){0};
}

int gdmf_frame_staging_create(VkDevice device,
    const VkPhysicalDeviceMemoryProperties* memory_properties,
    uint32_t image_count, gdmf_vulkan_staging* staging) {
    if (image_count == 0) { return -1; }
    gdmf_vulkan_staging_slot* slots = calloc(image_count, sizeof(gdmf_vulkan_staging_slot));
    if (!slots) { return -1; }
    *staging = (gdmf_vulkan_staging){
        .device = device, .memory_properties = *memory_properties,
        .image_count = image_count, .slots = slots
    };
    return 0;
}

// Only at reset, or first use when no storage has yet been handed out.
static bool gdmf_frame_staging_ensure(gdmf_vulkan_staging* staging,
    gdmf_vulkan_staging_slot* slot, VkDeviceSize bytes) {
    if (slot->buffer != VK_NULL_HANDLE && slot->capacity >= bytes) { return true; }
    VkDeviceSize capacity = bytes < GDMF_FRAME_STAGING_MINIMUM ? GDMF_FRAME_STAGING_MINIMUM : bytes;
    if (capacity > SIZE_MAX) { return false; }
    gdmf_vulkan_staging_slot replacement = {0};
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = capacity,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(staging->device, &buffer_info, NULL, &buffer);
    if (result != VK_SUCCESS) { goto fail; }
    replacement.buffer = buffer;

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(staging->device, buffer, &requirements);
    uint32_t memory_type = gdmf_find_memory_type(&staging->memory_properties, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == UINT32_MAX) { result = VK_ERROR_OUT_OF_DEVICE_MEMORY; goto fail; }
    VkMemoryAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_type
    };
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(staging->device, &allocation_info, NULL, &memory);
    if (result != VK_SUCCESS) { goto fail; }
    replacement.memory = memory;
    result = vkBindBufferMemory(staging->device, buffer, memory, 0);
    if (result != VK_SUCCESS) { goto fail; }
    void* mapped = NULL;
    result = vkMapMemory(staging->device, memory, 0, capacity, 0, &mapped);
    if (result != VK_SUCCESS) { goto fail; }
    replacement.mapped = mapped;
    replacement.capacity = capacity;
    replacement.used = slot->used;
    replacement.wanted = slot->wanted;
    gdmf_frame_staging_release(staging->device, slot);
    *slot = replacement;
    return true;

fail:
    gdmf_frame_staging_release(staging->device, &replacement);
    printf("[Vulkan] Frame staging allocation failed: %d\n", result);
    return false;
}

void gdmf_frame_staging_reset(gdmf_vulkan_staging* staging, uint32_t image_index) {
    if (!staging->slots || image_index >= staging->image_count) { return; }
    gdmf_vulkan_staging_slot* slot = &staging->slots[image_index];
    if (slot->wanted > slot->capacity) {
        gdmf_frame_staging_ensure(staging, slot, slot->wanted);
    }
    slot->used = 0;
    slot->wanted = 0;
}

void* gdmf_frame_staging_alloc(gdmf_vulkan_staging* staging, uint32_t image_index,
    VkDeviceSize bytes, VkBuffer* buffer, VkDeviceSize* offset) {
    if (!staging->slots || image_index >= staging->image_count || !bytes || !buffer || !offset) { return NULL; }
    gdmf_vulkan_staging_slot* slot = &staging->slots[image_index];
    VkDeviceSize alignment_mask = GDMF_FRAME_STAGING_ALIGNMENT - 1;
    if (slot->used > UINT64_MAX - alignment_mask) { return NULL; }
    VkDeviceSize aligned_offset = (slot->used + alignment_mask) & ~alignment_mask;
    if (bytes > UINT64_MAX - aligned_offset) { return NULL; }
    VkDeviceSize end_offset = aligned_offset + bytes;
    VkDeviceSize demand = end_offset - slot->used;
    if (slot->wanted > UINT64_MAX - demand) { return NULL; }
    slot->wanted += demand;

    if (slot->buffer == VK_NULL_HANDLE && !gdmf_frame_staging_ensure(staging, slot, end_offset)) { return NULL; }
    if (end_offset > slot->capacity || !slot->mapped) { return NULL; }
    slot->used = end_offset;
    *buffer = slot->buffer;
    *offset = aligned_offset;
    return (char*)slot->mapped + (size_t)aligned_offset;
}
