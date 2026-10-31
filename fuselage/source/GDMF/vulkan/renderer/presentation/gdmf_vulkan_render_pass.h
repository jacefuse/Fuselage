#ifndef GDMF_VULKAN_RENDER_PASS_H
#define GDMF_VULKAN_RENDER_PASS_H

#include "gdmf_vulkan_swapchain.h"

// Caller-owned array and framebuffer handles. Image views are borrowed.
typedef struct {
    uint32_t count;
    VkFramebuffer* handles;
} gdmf_vulkan_framebuffers;

// Main/render thread during setup. Supply valid device/format/output values.
// Returns 0 on success or -1 on failure; output is written only on success.
// Do not overwrite a live render-pass handle.
int gdmf_vulkan_render_pass_create(VkDevice device, VkFormat format, VkRenderPass* render_pass);

// Finish GPU work and release dependent framebuffers first. Does not wait.
// Supply a valid pointer to a null or owned handle; clears the handle.
void gdmf_vulkan_render_pass_destroy(VkDevice device, VkRenderPass* render_pass);

// Supply a compatible render pass, a created swapchain, and valid output.
// Returns 0 on success or -1 on failure. Partial creation is cleaned up;
// output is written only on success. Do not overwrite a live collection.
int gdmf_vulkan_framebuffers_create(VkDevice device, VkRenderPass render_pass,
    const gdmf_vulkan_swapchain* swapchain, gdmf_vulkan_framebuffers* framebuffers);

// Finish GPU work first. Release before the borrowed image views/render pass.
// Supply a zeroed or successfully created collection; clears it. Does not wait.
void gdmf_vulkan_framebuffers_destroy(VkDevice device, gdmf_vulkan_framebuffers* framebuffers);

#endif // GDMF_VULKAN_RENDER_PASS_H
