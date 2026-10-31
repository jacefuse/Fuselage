#ifndef GDMF_PIXIES_GRAPHICS_H
#define GDMF_PIXIES_GRAPHICS_H

#include "gdmf_pixies_images.h"
#include "../../pixies/gdmf_pixies_state_private.h"

typedef struct {
    float position[2], uv[2];
} gdmf_pixies_texture_vertex;
typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
} gdmf_pixies_pipeline;
typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
    size_t capacity;
} gdmf_pixies_vertex_buffer;

// Caller-owned outputs, zeroed before first use. Outputs change only on success.
// live selects position/color instead of position/UV and removes descriptor sets.
bool gdmf_pixies_pipeline_create(VkDevice device, VkRenderPass render_pass, VkPipelineCache cache,
                                 VkDescriptorSetLayout descriptor_layout, bool live,
                                 gdmf_pixies_pipeline* pipeline);
// Requires completed prior image-index work and no recorded references to the
// buffer being replaced. Failed growth retains the old allocation and contents.
bool gdmf_pixies_vertex_buffer_ensure(VkDevice device, const VkPhysicalDeviceMemoryProperties* properties,
                                      size_t bytes, size_t headroom, gdmf_pixies_vertex_buffer* buffer);
// Quiesced cleanup only; no implicit waits or retirement.
void gdmf_pixies_pipeline_destroy(VkDevice device, gdmf_pixies_pipeline* pipeline);
void gdmf_pixies_vertex_buffer_destroy(VkDevice device, gdmf_pixies_vertex_buffer* buffer);

#endif
