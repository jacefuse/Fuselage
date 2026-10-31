// Command pools and per-image frame synchronization.
// SEE: (GDMF_VULKAN.md, Commands and frame synchronization).
#include "gdmf_vulkan_commands.h"
#include <stdio.h>
#include <stdlib.h>

void gdmf_vulkan_command_pools_destroy(VkDevice device, gdmf_vulkan_command_pools* pools) {
    if (pools->render_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, pools->render_pool, NULL);
    }
    if (pools->upload_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, pools->upload_pool, NULL);
    }
    *pools = (gdmf_vulkan_command_pools){0};
}

int gdmf_vulkan_command_pools_create(VkDevice device, uint32_t graphics_family,
    gdmf_vulkan_command_pools* pools) {
    gdmf_vulkan_command_pools created_pools = {0};
    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family
    };
    VkResult result = vkCreateCommandPool(device, &create_info, NULL, &created_pools.render_pool);
    if (result != VK_SUCCESS) {
        printf("[Vulkan] Render command pool creation failed: %d\n", result);
        return -1;
    }

    create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    result = vkCreateCommandPool(device, &create_info, NULL, &created_pools.upload_pool);
    if (result != VK_SUCCESS) {
        created_pools.upload_pool = VK_NULL_HANDLE;
        gdmf_vulkan_command_pools_destroy(device, &created_pools);
        printf("[Vulkan] Upload command pool creation failed: %d\n", result);
        return -1;
    }
    *pools = created_pools;
    printf("[Vulkan] Command pools created\n");
    return 0;
}

void gdmf_vulkan_frame_commands_destroy(VkDevice device, gdmf_vulkan_frame_commands* frames) {
    if (frames->command_buffer_count > 0) {
        vkFreeCommandBuffers(device, frames->command_pool, frames->command_buffer_count,
            frames->command_buffers);
    }
    free(frames->command_buffers);
    for (uint32_t image_index = 0; image_index < frames->image_count; image_index++) {
        if (frames->acquire_semaphores && frames->acquire_semaphores[image_index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frames->acquire_semaphores[image_index], NULL);
        }
        if (frames->render_finished_semaphores && frames->render_finished_semaphores[image_index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frames->render_finished_semaphores[image_index], NULL);
        }
        if (frames->in_flight_fences && frames->in_flight_fences[image_index] != VK_NULL_HANDLE) {
            vkDestroyFence(device, frames->in_flight_fences[image_index], NULL);
        }
    }
    if (frames->spare_acquire_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, frames->spare_acquire_semaphore, NULL);
    }
    free(frames->acquire_semaphores);
    free(frames->render_finished_semaphores);
    free(frames->in_flight_fences);
    *frames = (gdmf_vulkan_frame_commands){0};
}

int gdmf_vulkan_frame_commands_create(VkDevice device, VkCommandPool render_pool,
    uint32_t image_count, gdmf_vulkan_frame_commands* frames) {
    if (image_count == 0) {
        printf("[Vulkan] Cannot create frame commands for zero images\n");
        return -1;
    }
    gdmf_vulkan_frame_commands created_frames = {
        .command_pool = render_pool,
        .image_count = image_count
    };
    VkResult result = VK_ERROR_OUT_OF_HOST_MEMORY;
    created_frames.command_buffers = calloc(image_count, sizeof(VkCommandBuffer));
    created_frames.acquire_semaphores = calloc(image_count, sizeof(VkSemaphore));
    created_frames.render_finished_semaphores = calloc(image_count, sizeof(VkSemaphore));
    created_frames.in_flight_fences = calloc(image_count, sizeof(VkFence));
    if (!created_frames.command_buffers || !created_frames.acquire_semaphores ||
        !created_frames.render_finished_semaphores || !created_frames.in_flight_fences) {
        goto fail;
    }

    VkCommandBufferAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = render_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = image_count
    };
    result = vkAllocateCommandBuffers(device, &allocation_info, created_frames.command_buffers);
    if (result != VK_SUCCESS) { goto fail; }
    created_frames.command_buffer_count = image_count;

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    VkSemaphore semaphore = VK_NULL_HANDLE;
    result = vkCreateSemaphore(device, &semaphore_info, NULL, &semaphore);
    if (result != VK_SUCCESS) { goto fail; }
    created_frames.spare_acquire_semaphore = semaphore;

    for (uint32_t image_index = 0; image_index < image_count; image_index++) {
        result = vkCreateSemaphore(device, &semaphore_info, NULL, &semaphore);
        if (result != VK_SUCCESS) { goto fail; }
        created_frames.acquire_semaphores[image_index] = semaphore;
        result = vkCreateSemaphore(device, &semaphore_info, NULL, &semaphore);
        if (result != VK_SUCCESS) { goto fail; }
        created_frames.render_finished_semaphores[image_index] = semaphore;
        VkFence fence = VK_NULL_HANDLE;
        result = vkCreateFence(device, &fence_info, NULL, &fence);
        if (result != VK_SUCCESS) { goto fail; }
        created_frames.in_flight_fences[image_index] = fence;
    }

    *frames = created_frames;
    printf("[Vulkan] Per-image commands ready (%u images)\n", image_count);
    return 0;

fail:
    gdmf_vulkan_frame_commands_destroy(device, &created_frames);
    printf("[Vulkan] Per-image command setup failed: %d\n", result);
    return -1;
}
