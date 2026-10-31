#ifndef GDMF_TILES_GRAPHICS_H
#define GDMF_TILES_GRAPHICS_H
#include "gdmf_tiles_atlas.h"
#include "../renderer/palettes/gdmf_vulkan_palettes.h"

typedef struct {
    float position[2], uv[2];
    // Opacity stays in 0..255 units; the Tile fragment shader normalizes it.
    float bitmap_id, palette, opacity, show_zero;
} gdmf_tile_vertex;

typedef struct {
    VkDevice device;
    VkDescriptorSetLayout layout;
} gdmf_tiles_descriptor_layout;
typedef struct {
    VkDevice device;
    VkDescriptorSetLayout layout; // Borrowed shared layout.
    VkDescriptorPool pool;
    VkDescriptorSet* sets;
    uint32_t image_count;
} gdmf_tiles_descriptors;
typedef struct {
    VkDevice device;
    VkPipeline pipeline;
    VkPipelineLayout layout;
} gdmf_tiles_pipeline;
typedef struct {
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    gdmf_tile_vertex* mapped;
    uint32_t capacity;
} gdmf_tiles_vertex_buffer;

// Zeroed caller-owned outputs, published only on success. One shared layout;
// one descriptor collection per Tile layer. Borrowed palettes/atlas/layout must
// outlive use. SEE: (GDMF_TILES.md, Vulkan graphics resources)
bool gdmf_tiles_descriptor_layout_create(VkDevice device, gdmf_tiles_descriptor_layout* layout);
bool gdmf_tiles_descriptors_create(const gdmf_tiles_descriptor_layout* layout, const gdmf_tiles_atlas* atlas,
                                   const VkBuffer* palette_buffers, uint32_t image_count,
                                   gdmf_tiles_descriptors* descriptors);
VkDescriptorSet gdmf_tiles_descriptor_get(const gdmf_tiles_descriptors* descriptors, uint32_t image_index);
bool gdmf_tiles_pipeline_create(VkDevice device, VkRenderPass render_pass, VkPipelineCache cache,
                                VkDescriptorSetLayout descriptor_layout, gdmf_tiles_pipeline* pipeline);
// Complete prior slot GPU work and discard command references before reuse/growth.
// Failed growth retains the old allocation/mapping/contents. Capacity is vertices.
// Initial capacity is at least 4096 vertices; caller budgets larger requests.
bool gdmf_tiles_vertex_buffer_ensure(VkDevice device,
                                     const VkPhysicalDeviceMemoryProperties* memory_properties,
                                     uint32_t required_vertices, gdmf_tiles_vertex_buffer* buffer);
// Quiesced cleanup only. Release pipeline/descriptor users before shared layout;
// descriptor users before borrowed atlas/palettes. No waits or retirement.
void gdmf_tiles_descriptor_layout_destroy(gdmf_tiles_descriptor_layout* layout);
void gdmf_tiles_descriptors_destroy(gdmf_tiles_descriptors* descriptors);
void gdmf_tiles_pipeline_destroy(gdmf_tiles_pipeline* pipeline);
void gdmf_tiles_vertex_buffer_destroy(gdmf_tiles_vertex_buffer* buffer);
#endif
