#ifndef GDMF_SPRITES_FRAME_H
#define GDMF_SPRITES_FRAME_H

#include "gdmf_sprites_graphics.h"

typedef struct {
    uint32_t first_vertex, vertex_count;
} gdmf_sprite_slice;

// One image slot owns its vertex storage and borrows the captured draw handles.
// SEE: (GDMF_SPRITES.md, Prepared frames)
typedef struct {
    gdmf_sprites_vertex_buffer vertices;
    gdmf_sprite_slice slices[256];
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSet descriptor;
    VkRect2D viewport;
    uint32_t image_index, sprite_count;
    bool prepared;
} gdmf_sprites_frame;

typedef struct {
    VkDevice device;
    const VkPhysicalDeviceMemoryProperties* memory_properties;
    const gdmf_sprites_atlas* atlas;
    const gdmf_sprites_descriptors* descriptors;
    const gdmf_sprites_pipeline* pipeline;
    float canvas_width, canvas_height;
    VkRect2D viewport;
} gdmf_sprites_frame_context;

// False for invalid projection; output remains unchanged. Triangle order is
// TL, BL, TR, BL, BR, TR. Does not inspect visibility or bitmap registration.
bool gdmf_sprites_frame_quad(const Sprite* sprite, float canvas_width, float canvas_height,
                             gdmf_sprite_vertex output[6]);

// Caller has completed this slot's prior GPU use and discarded recorded commands.
// Host state stays serialized through return. Uploads and collisions run before
// this call. False invalidates publication but retains owned vertex storage.
bool gdmf_sprites_frame_prepare(const gdmf_sprites_frame_context* context, uint32_t image_index,
                                gdmf_sprites_frame* frame);
void gdmf_sprites_frame_invalidate(gdmf_sprites_frame* frame);
// Record inside a compatible render pass; reads only this prepared snapshot.
void gdmf_sprites_frame_record(const gdmf_sprites_frame* frame, VkCommandBuffer command_buffer,
                               uint32_t image_index, uint8_t priority);
// Same completion/discard requirements as vertex-storage teardown.
void gdmf_sprites_frame_destroy(gdmf_sprites_frame* frame);

#endif
