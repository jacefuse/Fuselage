#ifndef GDMF_PIXIES_FRAME_H
#define GDMF_PIXIES_FRAME_H

#include "gdmf_pixies_graphics.h"
#include "../renderer/resources/gdmf_vulkan_staging.h"

typedef struct {
    bool drawn;
    PixieMode mode;
    uint8_t priority;
    VkDescriptorSet descriptor;
    uint32_t first_vertex, vertex_count;
    VkRect2D scissor;
} gdmf_pixies_draw_snapshot;
typedef struct {
    int id;
    VkImageView view;
    float rect[4];
    bool inverted;
} gdmf_pixies_mask_snapshot;
typedef struct {
    gdmf_pixies_vertex_buffer texture_vertices, live_vertices;
    gdmf_pixies_pipeline texture_pipeline, live_pipeline; // Borrowed handles.
    gdmf_pixies_draw_snapshot draws[MAX_PIXIES];
    gdmf_pixies_mask_snapshot masks[256];
    VkRect2D viewport;
    uint32_t image_index, unavailable_images;
    size_t recorded_decodes;
    bool valid;
} gdmf_pixies_frame;
typedef struct {
    gdmf_pixies_image_context* image_context;
    gdmf_pixies_image* images; // MAX_PIXIES entries, owned by the backend.
    const gdmf_pixies_pipeline* texture_pipeline;
    const gdmf_pixies_pipeline* live_pipeline;
    uint32_t image_count;
    float canvas_width, canvas_height;
    VkRect2D viewport;
    gdmf_vulkan_staging* decode_staging;
} gdmf_pixies_frame_context;

// Geometry helpers require finite display values and positive canvas dimensions.
void gdmf_pixies_frame_quad(const gdmf_pixies_state* state, float canvas_width, float canvas_height,
                            gdmf_pixies_texture_vertex vertices[6]);
bool gdmf_pixies_frame_scissor(const gdmf_pixies_state* state, float canvas_width, float canvas_height,
                               VkRect2D viewport, VkRect2D* scissor);
// Serialize with state changes. Prior work for this frame slot must be complete,
// and any discarded command references removed before buffer growth. False means
// invalid input. True publishes a snapshot; unavailable resources omit affected
// draws and are reported by unavailable_images/buffer-backed drawn flags.
bool gdmf_pixies_frame_prepare(const gdmf_pixies_frame_context* context, gdmf_pixies_frame* frame,
                               VkCommandBuffer command_buffer, uint32_t image_index);
// Inside the render pass; reads only the frozen snapshot and borrowed handles.
void gdmf_pixies_frame_record_priority(const gdmf_pixies_frame* frame, VkCommandBuffer command_buffer,
                                       uint8_t priority);
int gdmf_pixies_frame_mask_id(const gdmf_pixies_frame* frame, uint8_t priority);
bool gdmf_pixies_frame_mask_info(const gdmf_pixies_frame* frame, int id, VkImageView* view, float rect[4],
                                 bool* inverted);
// Quiesced cleanup of owned vertex buffers; images/pipelines belong to caller.
void gdmf_pixies_frame_destroy(VkDevice device, gdmf_pixies_frame* frame);

#endif
