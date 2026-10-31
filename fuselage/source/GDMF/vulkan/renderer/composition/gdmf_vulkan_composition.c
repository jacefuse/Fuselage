// Snapshot mask state during preparation; record priority passes from the snapshot.
// SEE: (GDMF_VULKAN.md, Viewport and composition recording).
#include "gdmf_vulkan_composition.h"
#include "gdmf_vulkan_geometry.h"
#include "gdmf_vulkan_layers.h"

bool gdmf_vulkan_composition_prepare(VkExtent2D extent, VkRect2D viewport,
    uint32_t mask_capacity, gdmf_vulkan_composition_plan* plan) {
    if (!extent.width || !extent.height || !viewport.extent.width || !viewport.extent.height ||
        mask_capacity > GDMF_VULKAN_PRIORITY_COUNT) { return false; }
    gdmf_vulkan_composition_plan prepared = {.extent = extent, .viewport = viewport};
    for (int priority = GDMF_VULKAN_PRIORITY_COUNT - 1; priority >= 0; priority--) {
        int pixie_id = gdmf_pixies_priority_mask_id((uint8_t)priority);
        if (pixie_id < 0) { continue; }
        if (prepared.mask_count == mask_capacity) { prepared.capacity_fallbacks++; continue; }
        gdmf_vulkan_composition_mask* mask = &prepared.masks[prepared.mask_count++];
        mask->priority = (uint8_t)priority;
        float canvas_rect[4];
        bool inverted = false;
        if (!gdmf_pixies_get_mask_info(pixie_id, &mask->mask_view, canvas_rect, &inverted) ||
            mask->mask_view == VK_NULL_HANDLE ||
            !gdmf_vulkan_mask_rect(extent, viewport, canvas_rect, mask->push.normalized_rect)) { continue; }
        mask->push.inverted = inverted ? 1u : 0u;
        mask->valid = true;
    }
    *plan = prepared;
    return true;
}

static void gdmf_vulkan_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority) {
    gdmf_pixies_record_priority(command_buffer, image_index, priority);
    gdmf_tiles_record_priority(command_buffer, image_index, priority);
    gdmf_sprites_record_priority(command_buffer, image_index, priority);
}

bool gdmf_vulkan_composition_record(const gdmf_vulkan_composition_frame* frame,
    const gdmf_vulkan_composition_plan* plan, gdmf_vulkan_masks* masks,
    gdmf_vulkan_mask_targets* targets, gdmf_vulkan_composition_result* result) {
    if (frame->command_buffer == VK_NULL_HANDLE || frame->render_pass == VK_NULL_HANDLE ||
        frame->framebuffer == VK_NULL_HANDLE || !plan->extent.width || !plan->extent.height ||
        !plan->viewport.extent.width || !plan->viewport.extent.height ||
        plan->mask_count > GDMF_VULKAN_PRIORITY_COUNT) { return false; }
    int mask_slot_at_priority[GDMF_VULKAN_PRIORITY_COUNT];
    for (uint32_t priority = 0; priority < GDMF_VULKAN_PRIORITY_COUNT; priority++) { mask_slot_at_priority[priority] = -1; }
    gdmf_vulkan_composition_result recorded = {.mask_fallbacks = plan->capacity_fallbacks};
    bool mask_resources_ready = masks && targets && masks->pipeline != VK_NULL_HANDLE &&
        masks->descriptor_sets && masks->offscreen_render_pass != VK_NULL_HANDLE &&
        masks->device == targets->config.device && masks->format == targets->config.format &&
        masks->offscreen_render_pass == targets->config.render_pass &&
        masks->image_count == targets->config.image_count && masks->masks_per_image == targets->config.masks_per_image &&
        frame->image_index < masks->image_count &&
        plan->extent.width == targets->config.extent.width && plan->extent.height == targets->config.extent.height;

    for (uint32_t mask_slot = 0; mask_slot < plan->mask_count; mask_slot++) {
        const gdmf_vulkan_composition_mask* mask = &plan->masks[mask_slot];
        const gdmf_vulkan_mask_target* target = NULL;
        if (mask_resources_ready && mask->valid) {
            target = gdmf_vulkan_mask_targets_ensure(targets, frame->image_index, mask_slot);
        }
        if (!target || !gdmf_vulkan_masks_update_descriptor(masks, frame->image_index,
                mask_slot, target->image_view, mask->mask_view)) {
            recorded.mask_fallbacks++;
            continue;
        }
        VkClearValue transparent_clear = {.color = {.float32 = {0, 0, 0, 0}}};
        VkRenderPassBeginInfo offscreen_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = masks->offscreen_render_pass, .framebuffer = target->framebuffer,
            .renderArea = {.offset = {0, 0}, .extent = plan->extent},
            .clearValueCount = 1, .pClearValues = &transparent_clear
        };
        vkCmdBeginRenderPass(frame->command_buffer, &offscreen_begin, VK_SUBPASS_CONTENTS_INLINE);
        gdmf_vulkan_record_priority(frame->command_buffer, frame->image_index, mask->priority);
        vkCmdEndRenderPass(frame->command_buffer);
        VkImageMemoryBarrier sample_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = target->image, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
        };
        vkCmdPipelineBarrier(frame->command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &sample_barrier);
        mask_slot_at_priority[mask->priority] = (int)mask_slot;
        recorded.masks_recorded++;
    }

    VkClearValue clear = {.color = frame->clear_color};
    VkRenderPassBeginInfo presentation_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = frame->render_pass, .framebuffer = frame->framebuffer,
        .renderArea = {.offset = {0, 0}, .extent = plan->extent},
        .clearValueCount = 1, .pClearValues = &clear
    };
    vkCmdBeginRenderPass(frame->command_buffer, &presentation_begin, VK_SUBPASS_CONTENTS_INLINE);
    for (int priority = GDMF_VULKAN_PRIORITY_COUNT - 1; priority >= 0; priority--) {
        int mask_slot = mask_slot_at_priority[priority];
        if (mask_slot < 0) {
            gdmf_vulkan_record_priority(frame->command_buffer, frame->image_index, (uint8_t)priority);
            continue;
        }
        VkViewport viewport = {0, 0, (float)plan->extent.width, (float)plan->extent.height, 0, 1};
        VkRect2D scissor = {.offset = {0, 0}, .extent = plan->extent};
        uint32_t descriptor_index = frame->image_index * masks->masks_per_image + (uint32_t)mask_slot;
        vkCmdSetViewport(frame->command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(frame->command_buffer, 0, 1, &scissor);
        vkCmdBindPipeline(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, masks->pipeline);
        vkCmdBindDescriptorSets(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, masks->pipeline_layout,
            0, 1, &masks->descriptor_sets[descriptor_index], 0, NULL);
        vkCmdPushConstants(frame->command_buffer, masks->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(gdmf_vulkan_mask_push_constants), &plan->masks[mask_slot].push);
        vkCmdDraw(frame->command_buffer, 3, 1, 0, 0);
    }
    gdmf_textlayer_record(frame->command_buffer, frame->image_index);
    vkCmdEndRenderPass(frame->command_buffer);
    *result = recorded;
    return true;
}
