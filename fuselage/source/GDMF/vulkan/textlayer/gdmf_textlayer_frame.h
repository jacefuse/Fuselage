#ifndef GDMF_TEXTLAYER_FRAME_H
#define GDMF_TEXTLAYER_FRAME_H
#include "gdmf_textlayer_graphics.h"
typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSet descriptor;
    VkRect2D viewport;
    uint32_t image_index;
    bool drawable;
} gdmf_textlayer_draw;
// Caller has completed prior image-slot use and discarded its old commands.
// Failure clears the output. Capture all recording inputs while host state is locked.
bool gdmf_textlayer_frame_prepare(const gdmf_textlayer_state* state, const gdmf_textlayer_cell_buffer* buffer,
                                  const gdmf_textlayer_descriptors* descriptors,
                                  const gdmf_textlayer_pipeline* pipeline, uint32_t image_index,
                                  VkRect2D viewport, gdmf_textlayer_draw* draw);
// Consumes one prepared draw, including an invalid/mismatched recording attempt.
void gdmf_textlayer_draw_record(gdmf_textlayer_draw* draw, VkCommandBuffer command, uint32_t image_index);
#endif
