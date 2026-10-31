#ifndef GDMF_SPRITES_GRAPHICS_H
#define GDMF_SPRITES_GRAPHICS_H

#include "gdmf_sprites_atlas.h"
#include "../renderer/palettes/gdmf_vulkan_palettes.h"

typedef struct {
    float position[2], uv[2];
    float bitmap_id, palette, opacity, show_zero, raw_grayscale;
} gdmf_sprite_vertex;

typedef struct {
    VkDevice device;
    VkDescriptorSetLayout layout;
    VkDescriptorPool pool;
    VkDescriptorSet* sets;
    uint32_t image_count;
} gdmf_sprites_descriptors;

typedef struct {
    VkDevice device;
    VkPipeline pipeline;
    VkPipelineLayout layout;
} gdmf_sprites_pipeline;

typedef struct {
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    gdmf_sprite_vertex* mapped;
    uint32_t capacity;
} gdmf_sprites_vertex_buffer;

// Caller-owned, initially zero outputs; publish only complete results.
// Atlas handles and all per-image palette buffers are borrowed and must outlive use.
// palette_buffers has image_count entries, each covering GDMF_PALETTE_BUFFER_SIZE bytes.
// SEE: (GDMF_SPRITES.md, Vulkan graphics resources)
bool gdmf_sprites_descriptors_create(VkDevice device, const gdmf_sprites_atlas* atlas,
                                     const VkBuffer* palette_buffers, uint32_t image_count,
                                     gdmf_sprites_descriptors* descriptors);
VkDescriptorSet gdmf_sprites_descriptor_get(const gdmf_sprites_descriptors* descriptors,
                                            uint32_t image_index);
bool gdmf_sprites_pipeline_create(VkDevice device, VkRenderPass render_pass, VkPipelineCache cache,
                                  VkDescriptorSetLayout descriptor_layout, gdmf_sprites_pipeline* pipeline);

// Prior slot GPU use must be complete and recorded references discarded before
// replacing storage. Failed growth keeps old storage and contents. Capacity is vertices.
// New storage is mapped/coherent; caller writes the complete new frame's vertices.
bool gdmf_sprites_vertex_buffer_ensure(VkDevice device,
                                       const VkPhysicalDeviceMemoryProperties* memory_properties,
                                       uint32_t required_vertices, gdmf_sprites_vertex_buffer* buffer);

// Quiesced cleanup, no waits/retirement. Release pipeline users before descriptor
// layout/pool teardown, and descriptor users before atlas/palette teardown.
void gdmf_sprites_descriptors_destroy(gdmf_sprites_descriptors* descriptors);
void gdmf_sprites_pipeline_destroy(gdmf_sprites_pipeline* pipeline);
void gdmf_sprites_vertex_buffer_destroy(gdmf_sprites_vertex_buffer* buffer);

#endif
