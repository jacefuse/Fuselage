// Acquire, prepare, record, capture, submit, and present one renderer frame.
// SEE: (GDMF_VULKAN.md, Renderer coordination).
#include "../gdmf_vulkan_renderer_private.h"
#include "../../pixies/gdmf_pixies_backend_internal.h"
#include "../../sprites/gdmf_sprites_backend_internal.h"
#include "../../tiles/gdmf_tiles_backend_internal.h"
#include "../../../textlayer/gdmf_textlayer_backend_internal.h"
#include "../composition/gdmf_vulkan_geometry.h"
#include "../../../compute/gdmf_compute.h"

bool gdmf_renderer_acquire(gdmf_vulkan_renderer* renderer) {
    if (!gdmf_pixies_backend_healthy() || !gdmf_sprites_backend_healthy() || !gdmf_tiles_backend_healthy() || !gdmf_textlayer_backend_healthy()) { renderer->stopped = true; return false; }
    if (!renderer->initialized || renderer->stopped || !renderer->frames_ready) { return false; }
    if (renderer->acquired || renderer->frame_pending) { return true; }
    if (GDMF_IsMinimized()) { return false; }
    if (GDMF_ResizeOccurred()) { renderer->needs_recreation = true; }
    if (renderer->needs_recreation) { return false; }
    uint32_t image_index = 0;
    VkSemaphore acquire_semaphore = renderer->frames.spare_acquire_semaphore;
    VkResult result = vkAcquireNextImageKHR(renderer->logical.device, renderer->swapchain.handle,
        UINT64_MAX, acquire_semaphore, VK_NULL_HANDLE, &image_index);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        gdmf_renderer_note_error(renderer, "Acquire image", result);
        return false;
    }
    if (image_index >= renderer->frames.image_count || image_index >= renderer->swapchain.image_count) {
        printf("[Vulkan] Acquired image index is outside frame resources\n");
        renderer->stopped = true;
        renderer->submit_unresolved = true;
        return false;
    }
    if (result == VK_SUBOPTIMAL_KHR) { renderer->needs_recreation = true; }
    renderer->frames.spare_acquire_semaphore = renderer->frames.acquire_semaphores[image_index];
    renderer->frames.acquire_semaphores[image_index] = acquire_semaphore;
    renderer->image_index = image_index;
    renderer->acquire_wait_pending = true;
    result = vkWaitForFences(renderer->logical.device, 1,
        &renderer->frames.in_flight_fences[image_index], VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        gdmf_renderer_note_error(renderer, "Wait for image fence", result);
        return false;
    }
    renderer->completion_observed = renderer->submitted_images[image_index] != 0;
    renderer->submitted_images[image_index] = 0;
    renderer->acquired = true;
    return true;
}

bool gdmf_renderer_prepare(gdmf_vulkan_renderer* renderer) {
    if (!gdmf_pixies_backend_healthy() || !gdmf_sprites_backend_healthy() || !gdmf_tiles_backend_healthy() || !gdmf_textlayer_backend_healthy()) { renderer->stopped = true; return false; }
    if (!renderer->initialized || renderer->stopped) { return false; }
    if (renderer->frame_pending) { return true; }
    if (!renderer->acquired) {
        if (GDMF_IsMinimized()) { return false; }
        if (GDMF_ResizeOccurred()) { renderer->needs_recreation = true; }
        if (renderer->needs_recreation || !renderer->frames_ready) {
            if (gdmf_renderer_recreate(renderer) == 0) { renderer->needs_recreation = false; }
            return false;
        }
        if (!gdmf_renderer_acquire(renderer)) { return false; }
    }
    renderer->acquired = false;
    if (renderer->completion_observed) {
        gdmf_defer_tick(&renderer->retirement);
        renderer->completion_observed = false;
    }
    int numerator = GDMF_GetAspectRatioNum(), denominator = GDMF_GetAspectRatioDen();
    if (numerator <= 0 || denominator <= 0 || !gdmf_vulkan_viewport_rect(renderer->swapchain.extent,
        (uint32_t)numerator, (uint32_t)denominator, &renderer->viewport)) {
        renderer->needs_recreation = true;
        return false;
    }
    VkCommandBuffer command_buffer = renderer->frames.command_buffers[renderer->image_index];
    VkResult result = vkResetCommandBuffer(command_buffer, 0);
    if (result != VK_SUCCESS) {
        gdmf_renderer_note_error(renderer, "Reset frame command buffer", result); return false;
    }
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        gdmf_renderer_note_error(renderer, "Begin frame command buffer", result); return false;
    }
    gdmf_frame_staging_reset(&renderer->staging, renderer->image_index);
    gdmf_vulkan_palettes_prepare(&renderer->palettes, renderer->image_index);
    gdmf_compute_prepare(renderer->image_index);
    gdmf_sprites_prepare(command_buffer, renderer->image_index);
    if (!gdmf_sprites_backend_healthy()) { renderer->stopped = true; return false; }
    gdmf_tiles_prepare(command_buffer, renderer->image_index);
    if (!gdmf_tiles_backend_healthy()) { renderer->stopped = true; return false; }
    gdmf_pixies_prepare(command_buffer, renderer->image_index);
    if (!gdmf_pixies_backend_healthy()) { renderer->stopped = true; return false; }
    gdmf_textlayer_prepare(renderer->image_index);
    if (!gdmf_textlayer_backend_healthy()) { renderer->stopped = true; return false; }
    if (!gdmf_vulkan_composition_prepare(renderer->swapchain.extent, renderer->viewport,
        MAX_TILE_LAYERS, &renderer->composition)) {
        renderer->needs_recreation = true; return false;
    }
    gdmf_renderer_ensure_masks(renderer);
    renderer->frame_clear_color = (VkClearColorValue){.float32 = {
        renderer->clear_color.r / 255.0f, renderer->clear_color.g / 255.0f,
        renderer->clear_color.b / 255.0f, 1.0f
    }};
    int width = GDMF_GetCanvasWidth(), height = GDMF_GetCanvasHeight();
    renderer->frame_canvas = width > 0 && height > 0
        ? (VkExtent2D){(uint32_t)width, (uint32_t)height} : renderer->swapchain.extent;
    renderer->frame_pending = true;
    return true;
}

static bool gdmf_renderer_capture(gdmf_vulkan_renderer* renderer) {
    gdmf_vulkan_capture_request request;
    if (!gdmf_vulkan_capture_request_take(&renderer->requests, &request)) { return true; }
    gdmf_vulkan_capture_source source = {
        .image = renderer->swapchain.images[renderer->image_index],
        .format = renderer->swapchain.format, .extent = renderer->swapchain.extent,
        .image_usage = renderer->swapchain.image_usage, .viewport = renderer->viewport,
        .canvas_extent = renderer->frame_canvas
    };
    gdmf_vulkan_capture_readback_result result =
        gdmf_vulkan_capture_readback_write(&renderer->readback, &source, request.path);
    if (result == GDMF_CAPTURE_READBACK_UNRESOLVED) {
        renderer->stopped = true;
        return false;
    }
    gdmf_vulkan_capture_request_finish(&renderer->requests);
    return true;
}

bool gdmf_renderer_submit(gdmf_vulkan_renderer* renderer) {
    if (!gdmf_pixies_backend_healthy() || !gdmf_sprites_backend_healthy() || !gdmf_tiles_backend_healthy() || !gdmf_textlayer_backend_healthy()) { renderer->stopped = true; return false; }
    if (!renderer->frame_pending || renderer->stopped) { return false; }
    renderer->frame_pending = false;
    uint32_t image_index = renderer->image_index;
    VkCommandBuffer command_buffer = renderer->frames.command_buffers[image_index];
    gdmf_vulkan_composition_frame frame = {
        .command_buffer = command_buffer, .image_index = image_index,
        .render_pass = renderer->render_pass, .framebuffer = renderer->framebuffers.handles[image_index],
        .clear_color = renderer->frame_clear_color
    };
    gdmf_vulkan_composition_result composition_result;
    if (!gdmf_vulkan_composition_record(&frame, &renderer->composition,
        &renderer->masks, &renderer->targets, &composition_result)) {
        renderer->needs_recreation = true; return false;
    }
    if (composition_result.mask_fallbacks && composition_result.mask_fallbacks != renderer->mask_fallbacks) {
        printf("[Masks] %u priorities rendered without masks\n", composition_result.mask_fallbacks);
    }
    renderer->mask_fallbacks = composition_result.mask_fallbacks;
    VkResult result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        gdmf_renderer_note_error(renderer, "End frame command buffer", result); return false;
    }
    result = vkResetFences(renderer->logical.device, 1, &renderer->frames.in_flight_fences[image_index]);
    if (result != VK_SUCCESS) {
        gdmf_renderer_note_error(renderer, "Reset frame fence", result); return false;
    }
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &renderer->frames.acquire_semaphores[image_index],
        .pWaitDstStageMask = &wait_stage, .commandBufferCount = 1, .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &renderer->frames.render_finished_semaphores[image_index]
    };
    fuselage_mutex_lock(&renderer->queue_lock);
    result = vkQueueSubmit(renderer->logical.graphics_queue, 1, &submit_info,
        renderer->frames.in_flight_fences[image_index]);
    fuselage_mutex_unlock(&renderer->queue_lock);
    if (result != VK_SUCCESS) {
        renderer->submit_unresolved = renderer->stopped = true;
        gdmf_renderer_note_error(renderer, "Submit frame", result);
        return false;
    }
    renderer->acquire_wait_pending = false;
    renderer->submitted_images[image_index] = 1;
    if (!gdmf_renderer_capture(renderer)) { return false; }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &renderer->frames.render_finished_semaphores[image_index],
        .swapchainCount = 1, .pSwapchains = &renderer->swapchain.handle, .pImageIndices = &image_index
    };
    fuselage_mutex_lock(&renderer->queue_lock);
    result = vkQueuePresentKHR(renderer->logical.present_queue, &present_info);
    fuselage_mutex_unlock(&renderer->queue_lock);
    if (result == VK_SUBOPTIMAL_KHR) { renderer->needs_recreation = true; return true; }
    if (result != VK_SUCCESS) {
        gdmf_renderer_note_error(renderer, "Present frame", result); return false;
    }
    return true;
}
