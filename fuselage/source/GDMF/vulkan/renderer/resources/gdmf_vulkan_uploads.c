// Serialized one-time command recording and synchronous submission.
// SEE: (GDMF_VULKAN.md, One-time upload commands).
#include "gdmf_vulkan_uploads.h"
#include <stdio.h>

static void gdmf_vulkan_uploads_release_handles(gdmf_vulkan_uploads* uploads,
    VkCommandBuffer command_buffer, VkFence fence) {
    if (fence != VK_NULL_HANDLE) { vkDestroyFence(uploads->device, fence, NULL); }
    if (command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(uploads->device, uploads->upload_pool, 1, &command_buffer);
    }
}

void gdmf_vulkan_uploads_release_completed(gdmf_vulkan_uploads* uploads) {
    fuselage_mutex_lock(uploads->upload_lock);
    gdmf_vulkan_uploads_release_handles(uploads,
        uploads->retained_command_buffer, uploads->retained_fence);
    uploads->retained_command_buffer = VK_NULL_HANDLE;
    uploads->retained_fence = VK_NULL_HANDLE;
    fuselage_mutex_unlock(uploads->upload_lock);
}

gdmf_vulkan_upload_result gdmf_execute_one_time_commands(gdmf_vulkan_uploads* uploads,
    gdmf_vulkan_command_record_callback record_commands, void* user_data) {
    if (!record_commands) { return GDMF_VULKAN_UPLOAD_FAILED; }
    fuselage_mutex_lock(uploads->upload_lock);
    if (uploads->retained_command_buffer != VK_NULL_HANDLE || uploads->retained_fence != VK_NULL_HANDLE) {
        printf("[Vulkan] Upload blocked: previous completion is unresolved\n");
        fuselage_mutex_unlock(uploads->upload_lock);
        return GDMF_VULKAN_UPLOAD_UNRESOLVED;
    }

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = uploads->upload_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer allocated_command = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(uploads->device, &allocation_info, &allocated_command);
    if (result != VK_SUCCESS) { goto fail; }
    command_buffer = allocated_command;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) { goto fail; }
    record_commands(command_buffer, user_data);
    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) { goto fail; }

    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence created_fence = VK_NULL_HANDLE;
    result = vkCreateFence(uploads->device, &fence_info, NULL, &created_fence);
    if (result != VK_SUCCESS) { goto fail; }
    fence = created_fence;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer
    };
    fuselage_mutex_lock(uploads->queue_lock);
    result = vkQueueSubmit(uploads->graphics_queue, 1, &submit_info, fence);
    fuselage_mutex_unlock(uploads->queue_lock);
    if (result == VK_SUCCESS) {
        result = vkWaitForFences(uploads->device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    if (result != VK_SUCCESS) {
        uploads->retained_command_buffer = command_buffer;
        uploads->retained_fence = fence;
        printf("[Vulkan] Upload completion unresolved: %d\n", result);
        fuselage_mutex_unlock(uploads->upload_lock);
        return GDMF_VULKAN_UPLOAD_UNRESOLVED;
    }

    gdmf_vulkan_uploads_release_handles(uploads, command_buffer, fence);
    fuselage_mutex_unlock(uploads->upload_lock);
    return GDMF_VULKAN_UPLOAD_COMPLETE;

fail:
    gdmf_vulkan_uploads_release_handles(uploads, command_buffer, fence);
    printf("[Vulkan] One-time command failed: %d\n", result);
    fuselage_mutex_unlock(uploads->upload_lock);
    return GDMF_VULKAN_UPLOAD_FAILED;
}
