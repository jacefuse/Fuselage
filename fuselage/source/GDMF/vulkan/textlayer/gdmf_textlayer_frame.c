// Prepared draw snapshots. SEE: (GDMF_TEXT.md, Prepared frames and lifecycle)
#include "gdmf_textlayer_frame.h"

bool gdmf_textlayer_frame_prepare(const gdmf_textlayer_state* state, const gdmf_textlayer_cell_buffer* buffer,
                                  const gdmf_textlayer_descriptors* descriptors,
                                  const gdmf_textlayer_pipeline* pipeline, uint32_t image_index,
                                  VkRect2D viewport, gdmf_textlayer_draw* draw) {
    if (!draw) {
        return false;
    }
    *draw = (gdmf_textlayer_draw){0};
    if (!state || !state->active || !buffer || !buffer->device || !buffer->buffer || !buffer->memory ||
        !buffer->mapped || !descriptors || descriptors->device != buffer->device || !pipeline ||
        pipeline->device != buffer->device || !pipeline->pipeline || !pipeline->layout ||
        !viewport.extent.width || !viewport.extent.height) {
        return false;
    }
    VkDescriptorSet descriptor = gdmf_textlayer_descriptor_get(descriptors, image_index);
    if (!descriptor || !gdmf_textlayer_cells_pack(state, buffer->mapped, GDMF_TEXTLAYER_CELL_COUNT)) {
        return false;
    }
    *draw =
        (gdmf_textlayer_draw){pipeline->pipeline, pipeline->layout, descriptor, viewport, image_index, true};
    return true;
}
void gdmf_textlayer_draw_record(gdmf_textlayer_draw* prepared, VkCommandBuffer command,
                                uint32_t image_index) {
    if (!prepared) {
        return;
    }
    gdmf_textlayer_draw draw = *prepared;
    *prepared = (gdmf_textlayer_draw){0};
    if (!command || !draw.drawable || draw.image_index != image_index || !draw.pipeline || !draw.layout ||
        !draw.descriptor || !draw.viewport.extent.width || !draw.viewport.extent.height) {
        return;
    }
    VkViewport viewport = {.x = (float)draw.viewport.offset.x,
                           .y = (float)draw.viewport.offset.y,
                           .width = (float)draw.viewport.extent.width,
                           .height = (float)draw.viewport.extent.height,
                           .minDepth = 0,
                           .maxDepth = 1};
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pipeline);
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &draw.viewport);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.layout, 0, 1, &draw.descriptor, 0,
                            NULL);
    vkCmdDraw(command, 6, 1, 0, 0);
}
