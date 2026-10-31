// Priority recording from retained draw values. SEE: (GDMF_TILES.md, Prepared frames)
#include "gdmf_tiles_frame.h"

void gdmf_tiles_draw_record_priority(const gdmf_tiles_draw draws[MAX_TILE_LAYERS],
                                     VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority) {
    if (!draws || !command_buffer) {
        return;
    }
    for (int layer = MAX_TILE_LAYERS - 1; layer >= 0; --layer) {
        const gdmf_tiles_draw* draw = &draws[layer];
        if (!draw->drawable || draw->image_index != image_index || draw->priority != priority ||
            !draw->vertices || !draw->descriptor || !draw->pipeline || !draw->layout || !draw->vertex_count ||
            !draw->scissor.extent.width || !draw->scissor.extent.height) {
            continue;
        }
        VkViewport viewport = {.x = (float)draw->viewport.offset.x,
                               .y = (float)draw->viewport.offset.y,
                               .width = (float)draw->viewport.extent.width,
                               .height = (float)draw->viewport.extent.height,
                               .minDepth = 0,
                               .maxDepth = 1};
        VkDeviceSize offset = 0;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->pipeline);
        vkCmdSetScissor(command_buffer, 0, 1, &draw->scissor);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->layout, 0, 1,
                                &draw->descriptor, 0, NULL);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &draw->vertices, &offset);
        vkCmdDraw(command_buffer, draw->vertex_count, 1, 0, 0);
    }
}
