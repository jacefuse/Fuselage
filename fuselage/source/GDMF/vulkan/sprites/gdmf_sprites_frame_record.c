#include "gdmf_sprites_frame.h"

void gdmf_sprites_frame_record(const gdmf_sprites_frame* frame, VkCommandBuffer command_buffer,
                               uint32_t image_index, uint8_t priority) {
    if (!frame || !frame->prepared || !command_buffer || frame->image_index != image_index ||
        !frame->slices[priority].vertex_count) {
        return;
    }
    VkViewport viewport = {.x = (float)frame->viewport.offset.x,
                           .y = (float)frame->viewport.offset.y,
                           .width = (float)frame->viewport.extent.width,
                           .height = (float)frame->viewport.extent.height,
                           .minDepth = 0.0f,
                           .maxDepth = 1.0f};
    VkDeviceSize offset = 0;
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frame->pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &frame->viewport);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &frame->vertices.buffer, &offset);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frame->layout, 0, 1,
                            &frame->descriptor, 0, NULL);
    vkCmdDraw(command_buffer, frame->slices[priority].vertex_count, 1, frame->slices[priority].first_vertex,
              0);
}
