// Record from immutable frame data. SEE: (GDMF_PIXIES.md, Frame preparation)
#include "gdmf_pixies_frame.h"
#include <string.h>

void gdmf_pixies_frame_record_priority(const gdmf_pixies_frame* frame, VkCommandBuffer command_buffer,
                                       uint8_t priority) {
    if (!frame || !frame->valid || !command_buffer)
        return;
    int bound_mode = -1;
    for (int id = 0; id < MAX_PIXIES; id++) {
        const gdmf_pixies_draw_snapshot* draw = &frame->draws[id];
        if (!draw->drawn || draw->priority != priority)
            continue;
        if (draw->mode != PIXIE_MODE_TEXTURE && draw->mode != PIXIE_MODE_LIVE)
            continue;
        const gdmf_pixies_pipeline* pipeline =
            draw->mode == PIXIE_MODE_TEXTURE ? &frame->texture_pipeline : &frame->live_pipeline;
        VkBuffer buffer =
            draw->mode == PIXIE_MODE_TEXTURE ? frame->texture_vertices.buffer : frame->live_vertices.buffer;
        if (!buffer || !pipeline->pipeline || !pipeline->layout)
            continue;
        if (bound_mode != (int)draw->mode) {
            VkViewport viewport = {.x = (float)frame->viewport.offset.x,
                                   .y = (float)frame->viewport.offset.y,
                                   .width = (float)frame->viewport.extent.width,
                                   .height = (float)frame->viewport.extent.height,
                                   .minDepth = 0,
                                   .maxDepth = 1};
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer, &offset);
            bound_mode = (int)draw->mode;
        }
        vkCmdSetScissor(command_buffer, 0, 1, &draw->scissor);
        if (draw->mode == PIXIE_MODE_TEXTURE) {
            if (!draw->descriptor)
                continue;
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 1,
                                    &draw->descriptor, 0, NULL);
        }
        vkCmdDraw(command_buffer, draw->vertex_count, 1, draw->first_vertex, 0);
    }
    // Other layers must not inherit the last Pixie's clipping rectangle.
    vkCmdSetScissor(command_buffer, 0, 1, &frame->viewport);
}
int gdmf_pixies_frame_mask_id(const gdmf_pixies_frame* frame, uint8_t priority) {
    return frame && frame->valid ? frame->masks[priority].id : -1;
}
bool gdmf_pixies_frame_mask_info(const gdmf_pixies_frame* frame, int id, VkImageView* view, float rect[4],
                                 bool* inverted) {
    if (!frame || !frame->valid || id < 0)
        return false;
    for (unsigned priority = 0; priority < 256; priority++) {
        const gdmf_pixies_mask_snapshot* mask = &frame->masks[priority];
        if (mask->id != id)
            continue;
        if (view)
            *view = mask->view;
        if (rect)
            memcpy(rect, mask->rect, sizeof mask->rect);
        if (inverted)
            *inverted = mask->inverted;
        return true;
    }
    return false;
}
void gdmf_pixies_frame_destroy(VkDevice device, gdmf_pixies_frame* frame) {
    if (!device || !frame)
        return;
    gdmf_pixies_vertex_buffer_destroy(device, &frame->texture_vertices);
    gdmf_pixies_vertex_buffer_destroy(device, &frame->live_vertices);
    memset(frame, 0, sizeof *frame);
}
