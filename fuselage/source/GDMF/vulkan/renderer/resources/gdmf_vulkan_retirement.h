#ifndef GDMF_VULKAN_RETIREMENT_H
#define GDMF_VULKAN_RETIREMENT_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include "../../../../BASE/fuselage_sync.h"

typedef struct gdmf_vulkan_retirement_entry gdmf_vulkan_retirement_entry;

// Caller-owned queue. Device and initialized lock are borrowed; entries owned.
typedef struct {
    VkDevice device;
    FuselageMutex* lock;
    uint32_t frame_delay;
    gdmf_vulkan_retirement_entry* entries;
} gdmf_vulkan_retirement_queue;

// Setup before producers start. Supply valid device, initialized lock, and
// output. Returns 0 on success or -1 for zero/overflowing image_count.
// Do not overwrite a live queue. Drain it before changing its frame delay.
int gdmf_defer_queue_init(VkDevice device, FuselageMutex* lock, uint32_t image_count,
    gdmf_vulkan_retirement_queue* queue);

// True transfers ownership to the queue (null resources are successful no-ops).
// False leaves ownership with the caller; do not destroy resources still in use.
// Stop creating new references before retirement. Context and borrowed objects
// must remain valid; callers must not already hold the queue's lock.
// Do not transfer ownership of the same resource more than once.
bool gdmf_defer_destroy_buffer(gdmf_vulkan_retirement_queue* queue,
    VkBuffer buffer, VkDeviceMemory memory);
bool gdmf_defer_destroy_image(gdmf_vulkan_retirement_queue* queue,
    VkImage image, VkImageView image_view, VkDeviceMemory memory);
bool gdmf_defer_free_descriptor_set(gdmf_vulkan_retirement_queue* queue,
    VkDescriptorPool pool, VkDescriptorSet descriptor_set);
bool gdmf_defer_destroy_descriptor_pool(gdmf_vulkan_retirement_queue* queue, VkDescriptorPool pool);
bool gdmf_defer_destroy_sampler(gdmf_vulkan_retirement_queue* queue, VkSampler sampler);

// Drop queued set frees before directly destroying their pool after completion.
// This does not establish completion or remove a queued whole-pool destruction.
void gdmf_defer_forget_descriptor_pool(gdmf_vulkan_retirement_queue* queue, VkDescriptorPool pool);

// Render thread at the coordinator's completed-frame boundary. Uses the existing
// image_count+1 countdown; it is not an independently verified GPU completion token.
// SEE: (GDMF_VULKAN.md, Deferred destruction).
void gdmf_defer_tick(gdmf_vulkan_retirement_queue* queue);

// Stop producers and establish GPU completion with no recorded/unsubmitted
// references first. Returns true if drained; failed set frees remain for retry.
// Does not wait or release borrowed device/lock. Empty queues can be flushed again.
bool gdmf_defer_flush(gdmf_vulkan_retirement_queue* queue);

#endif // GDMF_VULKAN_RETIREMENT_H
