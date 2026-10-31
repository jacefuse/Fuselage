#ifndef GDMF_TILES_FRAME_H
#define GDMF_TILES_FRAME_H
#include "gdmf_tiles_graphics.h"

typedef struct {
    const TileMap* map;
    const gdmf_tiles_atlas* atlas;
    float canvas_width, canvas_height;
} gdmf_tiles_geometry_context;
typedef struct {
    float scale, tile_width, tile_height, cosine, sine;
    double offset_x, offset_y;
    bool turned;
    int start_x, start_y, columns, rows;
    uint32_t required_vertices;
} gdmf_tiles_geometry_plan;

// Checked candidate bounds/counts; no allocation or host mutation.
bool gdmf_tiles_geometry_plan_create(const gdmf_tiles_geometry_context* context,
                                     gdmf_tiles_geometry_plan* plan);
bool gdmf_tiles_geometry_build(const gdmf_tiles_geometry_context* context,
                               const gdmf_tiles_geometry_plan* plan, gdmf_tile_vertex* vertices,
                               uint32_t capacity, uint32_t* output_count);

// Detached recording values. Resource owners must retain the captured handles
// through command completion, including logical layer release between phases.
typedef struct {
    VkBuffer vertices;
    VkDescriptorSet descriptor;
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkRect2D viewport, scissor;
    uint32_t vertex_count, image_index;
    uint8_t priority;
    bool drawable;
} gdmf_tiles_draw;
typedef struct {
    gdmf_tiles_vertex_buffer vertices;
    uint32_t vertex_count;
    float canvas_width, canvas_height;
    VkImage geometry_atlas;
    uint8_t layer;
    bool geometry_valid;
    gdmf_tiles_draw draw;
} gdmf_tiles_frame;
typedef struct {
    VkDevice device;
    const VkPhysicalDeviceMemoryProperties* memory_properties;
    const gdmf_tiles_atlas* atlas;
    const gdmf_tiles_descriptors* descriptors;
    const gdmf_tiles_pipeline* pipeline;
    float canvas_width, canvas_height;
    VkRect2D viewport;
    bool rebuild;
} gdmf_tiles_frame_context;

// Prior slot GPU use completed and command references discarded. Host state is
// stable throughout prepare. Caller marks rebuild after geometry/readiness edits.
// False clears publication and invalidates cached geometry; allocation retained.
// SEE: (GDMF_TILES.md, Prepared frames)
bool gdmf_tiles_frame_prepare(const gdmf_tiles_frame_context* context, uint32_t image_index,
                              gdmf_tiles_frame* frame);
void gdmf_tiles_frame_invalidate(gdmf_tiles_frame* frame);
void gdmf_tiles_frame_destroy(gdmf_tiles_frame* frame);
// Copies of draws are indexed by Tile layer. Record inside a compatible pass;
// no live host state, canvas or renderer-global access.
void gdmf_tiles_draw_record_priority(const gdmf_tiles_draw draws[MAX_TILE_LAYERS],
                                     VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority);
#endif
