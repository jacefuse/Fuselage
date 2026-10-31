// Presentation render pass and its per-image framebuffers.
// SEE: (GDMF_VULKAN.md, Render pass and framebuffers).
#include "gdmf_vulkan_render_pass.h"
#include <stdio.h>
#include <stdlib.h>

int gdmf_vulkan_render_pass_create(VkDevice device, VkFormat format, VkRenderPass* render_pass) {
    VkAttachmentDescription color_attachment = {
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference color_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference
    };
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VkRenderPass created_render_pass = VK_NULL_HANDLE;
    VkResult result = vkCreateRenderPass(device, &create_info, NULL, &created_render_pass);
    if (result != VK_SUCCESS) {
        printf("[Vulkan] Render pass creation failed: %d\n", result);
        return -1;
    }
    *render_pass = created_render_pass;
    printf("[Vulkan] Render pass created\n");
    return 0;
}

void gdmf_vulkan_render_pass_destroy(VkDevice device, VkRenderPass* render_pass) {
    if (*render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, *render_pass, NULL);
        *render_pass = VK_NULL_HANDLE;
    }
}

void gdmf_vulkan_framebuffers_destroy(VkDevice device, gdmf_vulkan_framebuffers* framebuffers) {
    if (framebuffers->handles) {
        for (uint32_t framebuffer_index = 0; framebuffer_index < framebuffers->count; framebuffer_index++) {
            vkDestroyFramebuffer(device, framebuffers->handles[framebuffer_index], NULL);
        }
        free(framebuffers->handles);
    }
    *framebuffers = (gdmf_vulkan_framebuffers){0};
}

int gdmf_vulkan_framebuffers_create(VkDevice device, VkRenderPass render_pass,
    const gdmf_vulkan_swapchain* swapchain, gdmf_vulkan_framebuffers* framebuffers) {
    gdmf_vulkan_framebuffers created_framebuffers = {0};
    created_framebuffers.handles = malloc(swapchain->image_count * sizeof(VkFramebuffer));
    if (!created_framebuffers.handles) {
        printf("[Vulkan] Framebuffer array allocation failed\n");
        return -1;
    }

    for (uint32_t image_index = 0; image_index < swapchain->image_count; image_index++) {
        VkFramebufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &swapchain->image_views[image_index],
            .width = swapchain->extent.width,
            .height = swapchain->extent.height,
            .layers = 1
        };
        VkResult result = vkCreateFramebuffer(device, &create_info, NULL,
            &created_framebuffers.handles[image_index]);
        if (result != VK_SUCCESS) {
            printf("[Vulkan] Framebuffer creation failed (index %u): %d\n", image_index, result);
            gdmf_vulkan_framebuffers_destroy(device, &created_framebuffers);
            return -1;
        }
        created_framebuffers.count++;
    }
    *framebuffers = created_framebuffers;
    return 0;
}
