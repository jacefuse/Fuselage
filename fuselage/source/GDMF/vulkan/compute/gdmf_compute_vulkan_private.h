#ifndef GDMF_COMPUTE_VULKAN_PRIVATE_H
#define GDMF_COMPUTE_VULKAN_PRIVATE_H

#include "../renderer/gdmf_vulkan_internal.h"
#include "../../compute/gdmf_compute.h"

#define GDMF_VULKAN_COMPUTE_SETS_PER_FRAME 64u

// Owned by resource setup; borrowed read-only by dispatch recording.
typedef struct {
    VkDevice device;
    VkDescriptorSetLayout kind_layouts[GDMF_KERNEL_BIND_KIND_COUNT];
    VkPipelineLayout pipeline_layouts[GDMF_KERNEL_COUNT];
    VkPipeline pipelines[GDMF_KERNEL_COUNT];
    VkDescriptorPool* frame_pools;
    uint32_t frame_pool_count;
    uint32_t buffer_alignment;
    uint32_t prepared_image;
    bool built;
    bool frame_ready;
} gdmf_vulkan_compute;

const gdmf_vulkan_compute* gdmf_compute_get_state(void);

#endif
