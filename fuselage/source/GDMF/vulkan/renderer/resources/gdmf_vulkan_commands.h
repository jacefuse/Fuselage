#ifndef GDMF_VULKAN_COMMANDS_H
#define GDMF_VULKAN_COMMANDS_H

#include <vulkan/vulkan.h>

// Caller-owned pools. Frames use render_pool; serialized one-time uploads use
// upload_pool. Both belong to the selected graphics queue family.
typedef struct {
    VkCommandPool render_pool;
    VkCommandPool upload_pool;
} gdmf_vulkan_command_pools;

// Caller-owned per-image commands and synchronization. command_pool is borrowed.
typedef struct {
    VkCommandPool command_pool;
    uint32_t image_count;
    uint32_t command_buffer_count;
    VkCommandBuffer* command_buffers;
    VkSemaphore* acquire_semaphores;
    VkSemaphore* render_finished_semaphores;
    VkFence* in_flight_fences;
    VkSemaphore spare_acquire_semaphore;
} gdmf_vulkan_frame_commands;

// Setup on the main/render thread. Supply valid device, graphics-family index,
// and output pointer. Returns 0 on success or -1 on failure. Partial resources
// are cleaned up; output changes only on success. Do not overwrite live pools.
int gdmf_vulkan_command_pools_create(VkDevice device, uint32_t graphics_family,
    gdmf_vulkan_command_pools* pools);

// Stop users and finish GPU work first. Release frame commands before pools.
// Supply a zeroed or successful result; clears it. Does not wait.
void gdmf_vulkan_command_pools_destroy(VkDevice device, gdmf_vulkan_command_pools* pools);

// Supply a valid render pool, nonzero image_count, and output pointer.
// Returns 0 on success or -1 on failure, with partial cleanup and unchanged
// output on failure. Do not overwrite a live collection. Does not own staging.
int gdmf_vulkan_frame_commands_create(VkDevice device, VkCommandPool render_pool,
    uint32_t image_count, gdmf_vulkan_frame_commands* frames);

// Stop users and finish GPU/presentation work first. The borrowed pool and
// device must remain valid. Supply a zeroed or successful result; clears it.
// Does not wait or release per-image staging resources.
void gdmf_vulkan_frame_commands_destroy(VkDevice device, gdmf_vulkan_frame_commands* frames);

#endif // GDMF_VULKAN_COMMANDS_H
