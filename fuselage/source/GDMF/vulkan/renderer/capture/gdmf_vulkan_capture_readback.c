// Frame readback from an explicitly owned presentation-layout image.
// SEE: (GDMF_VULKAN.md, Capture readback).
#include "gdmf_vulkan_capture_readback.h"
#include "gdmf_vulkan_capture_ppm.h"
#include "../resources/gdmf_vulkan_memory.h"
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const gdmf_vulkan_capture_source* source;
    VkBuffer destination;
} gdmf_vulkan_capture_copy;

static void gdmf_vulkan_capture_record_copy(VkCommandBuffer command_buffer, void* user_data) {
    const gdmf_vulkan_capture_copy* copy = user_data;
    VkImageMemoryBarrier to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = copy->source->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &to_transfer);
    VkBufferImageCopy region = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {copy->source->extent.width, copy->source->extent.height, 1}
    };
    vkCmdCopyImageToBuffer(command_buffer, copy->source->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        copy->destination, 1, &region);
    VkImageMemoryBarrier to_present = to_transfer;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &to_present);
}

static void gdmf_vulkan_capture_release_storage(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, buffer, NULL); }
    if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, NULL); }
}

void gdmf_vulkan_capture_readback_release_completed(gdmf_vulkan_capture_readback* readback) {
    gdmf_vulkan_capture_release_storage(readback->device, readback->retained_buffer, readback->retained_memory);
    readback->retained_buffer = VK_NULL_HANDLE;
    readback->retained_memory = VK_NULL_HANDLE;
}

gdmf_vulkan_capture_readback_result gdmf_vulkan_capture_readback_write(
    gdmf_vulkan_capture_readback* readback, const gdmf_vulkan_capture_source* source,
    const char* path) {
    if (readback->retained_buffer != VK_NULL_HANDLE || readback->retained_memory != VK_NULL_HANDLE) {
        return GDMF_CAPTURE_READBACK_UNRESOLVED;
    }
    if (!path || !*path || !source || source->image == VK_NULL_HANDLE ||
        !source->extent.width || !source->extent.height ||
        !source->viewport.extent.width || !source->viewport.extent.height ||
        !(source->image_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
        readback->uploads->device != readback->device) { return GDMF_CAPTURE_READBACK_FAILED; }
    switch (source->format) {
    case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB:
        break;
    default:
        return GDMF_CAPTURE_READBACK_FAILED;
    }
    size_t bytes = source->extent.width;
    if (source->extent.height > SIZE_MAX / bytes) { return GDMF_CAPTURE_READBACK_FAILED; }
    bytes *= source->extent.height;
    if (bytes > SIZE_MAX / 4) { return GDMF_CAPTURE_READBACK_FAILED; }
    bytes *= 4;

    fuselage_mutex_lock(readback->uploads->queue_lock);
    VkResult result = vkDeviceWaitIdle(readback->device);
    fuselage_mutex_unlock(readback->uploads->queue_lock);
    if (result != VK_SUCCESS) {
        printf("[Capture] Device idle wait failed: %d\n", result);
        return GDMF_CAPTURE_READBACK_FAILED;
    }

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer created_buffer = VK_NULL_HANDLE;
    result = vkCreateBuffer(readback->device, &buffer_info, NULL, &created_buffer);
    if (result != VK_SUCCESS) { goto fail; }
    buffer = created_buffer;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(readback->device, buffer, &requirements);
    uint32_t memory_type = gdmf_find_memory_type(&readback->memory_properties, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == UINT32_MAX) { result = VK_ERROR_OUT_OF_DEVICE_MEMORY; goto fail; }
    VkMemoryAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size, .memoryTypeIndex = memory_type
    };
    VkDeviceMemory allocated_memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(readback->device, &allocation_info, NULL, &allocated_memory);
    if (result != VK_SUCCESS) { goto fail; }
    memory = allocated_memory;
    result = vkBindBufferMemory(readback->device, buffer, memory, 0);
    if (result != VK_SUCCESS) { goto fail; }

    gdmf_vulkan_capture_copy copy = {.source = source, .destination = buffer};
    gdmf_vulkan_upload_result upload_result =
        gdmf_execute_one_time_commands(readback->uploads, gdmf_vulkan_capture_record_copy, &copy);
    if (upload_result == GDMF_VULKAN_UPLOAD_UNRESOLVED) {
        readback->retained_buffer = buffer;
        readback->retained_memory = memory;
        printf("[Capture] Readback completion unresolved; storage retained\n");
        return GDMF_CAPTURE_READBACK_UNRESOLVED;
    }
    if (upload_result != GDMF_VULKAN_UPLOAD_COMPLETE) {
        gdmf_vulkan_capture_release_storage(readback->device, buffer, memory);
        return GDMF_CAPTURE_READBACK_FAILED;
    }

    void* pixels = NULL;
    result = vkMapMemory(readback->device, memory, 0, bytes, 0, &pixels);
    if (result != VK_SUCCESS) { goto fail; }
    gdmf_vulkan_capture_image image = {
        .pixels = pixels, .byte_count = bytes, .format = source->format,
        .image_extent = source->extent, .viewport = source->viewport, .canvas_extent = source->canvas_extent
    };
    bool saved = gdmf_vulkan_capture_write_ppm(path, &image);
    vkUnmapMemory(readback->device, memory);
    gdmf_vulkan_capture_release_storage(readback->device, buffer, memory);
    printf("[Capture] %s %s\n", saved ? "Wrote" : "Could not write", path);
    return saved ? GDMF_CAPTURE_READBACK_SAVED : GDMF_CAPTURE_READBACK_FAILED;

fail:
    gdmf_vulkan_capture_release_storage(readback->device, buffer, memory);
    printf("[Capture] Readback failed: %d\n", result);
    return GDMF_CAPTURE_READBACK_FAILED;
}
