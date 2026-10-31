#ifndef GDMF_TEXTLAYER_GRAPHICS_H
#define GDMF_TEXTLAYER_GRAPHICS_H
#include "gdmf_textlayer_atlas.h"
#include "../../textlayer/gdmf_textlayer_state_private.h"
#include <stddef.h>

typedef struct {
    uint32_t character, color;
} gdmf_textlayer_cell;
_Static_assert(sizeof(gdmf_textlayer_cell) == 8, "Text std430 cell stride");
_Static_assert(offsetof(gdmf_textlayer_cell, color) == 4, "Text std430 color offset");
_Static_assert(TEXT_LAYER_WIDTH == 80 && TEXT_LAYER_HEIGHT == 45, "Text shader grid must match host");
_Static_assert(CHARACTER_WIDTH == 16 && CHARACTER_HEIGHT == 16, "Text shader glyph dimensions");
#define GDMF_TEXTLAYER_CELL_COUNT (TEXT_LAYER_WIDTH * TEXT_LAYER_HEIGHT)
#define GDMF_TEXTLAYER_CELL_BUFFER_SIZE (GDMF_TEXTLAYER_CELL_COUNT * sizeof(gdmf_textlayer_cell))

typedef struct {
    VkDevice device;
    VkBuffer buffer;
    VkDeviceMemory memory;
    gdmf_textlayer_cell* mapped;
} gdmf_textlayer_cell_buffer;
typedef struct {
    VkDevice device;
    VkDescriptorSetLayout layout;
} gdmf_textlayer_descriptor_layout;
typedef struct {
    VkDevice device;
    VkDescriptorSetLayout layout; // Borrowed.
    VkDescriptorPool pool;
    VkDescriptorSet* sets;
    uint32_t image_count;
} gdmf_textlayer_descriptors;
typedef struct {
    VkDevice device;
    VkPipeline pipeline;
    VkPipelineLayout layout;
} gdmf_textlayer_pipeline;

// Zeroed outputs publish only on success. Borrowed device/atlas/layout/buffers
// must outlive use. SEE: (GDMF_TEXT.md, Vulkan graphics resources)
bool gdmf_textlayer_cell_buffer_create(VkDevice device,
                                       const VkPhysicalDeviceMemoryProperties* memory_properties,
                                       gdmf_textlayer_cell_buffer* buffer);
// Complete prior slot use before writing. Short outputs remain untouched.
bool gdmf_textlayer_cells_pack(const gdmf_textlayer_state* state, gdmf_textlayer_cell* cells,
                               size_t cell_count);
bool gdmf_textlayer_descriptor_layout_create(VkDevice device, gdmf_textlayer_descriptor_layout* layout);
bool gdmf_textlayer_descriptors_create(const gdmf_textlayer_descriptor_layout* layout,
                                       const gdmf_textlayer_atlas* atlas, const VkBuffer* cell_buffers,
                                       uint32_t image_count, gdmf_textlayer_descriptors* descriptors);
VkDescriptorSet gdmf_textlayer_descriptor_get(const gdmf_textlayer_descriptors* descriptors,
                                              uint32_t image_index);
bool gdmf_textlayer_pipeline_create(VkDevice device, VkRenderPass render_pass, VkPipelineCache cache,
                                    VkDescriptorSetLayout descriptor_layout,
                                    gdmf_textlayer_pipeline* pipeline);
// Caller must stop producers, complete GPU work and discard command references.
// Release pipeline/sets before layout; sets before buffers/atlas. No hidden waits.
void gdmf_textlayer_cell_buffer_destroy(gdmf_textlayer_cell_buffer* buffer);
void gdmf_textlayer_descriptor_layout_destroy(gdmf_textlayer_descriptor_layout* layout);
void gdmf_textlayer_descriptors_destroy(gdmf_textlayer_descriptors* descriptors);
void gdmf_textlayer_pipeline_destroy(gdmf_textlayer_pipeline* pipeline);
#endif
