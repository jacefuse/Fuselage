// Mask compositor resources; targets and draw recording have separate owners.
// SEE: (GDMF_VULKAN.md, Mask compositor setup).
#include "gdmf_vulkan_masks.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void gdmf_vulkan_masks_destroy(gdmf_vulkan_masks* masks) {
    VkDevice device = masks->device;
    if (masks->pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, masks->pipeline, NULL); }
    if (masks->pipeline_layout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, masks->pipeline_layout, NULL); }
    if (masks->descriptor_pool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, masks->descriptor_pool, NULL); }
    free(masks->descriptor_sets);
    if (masks->descriptor_layout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, masks->descriptor_layout, NULL); }
    if (masks->sampler != VK_NULL_HANDLE) { vkDestroySampler(device, masks->sampler, NULL); }
    if (masks->offscreen_render_pass != VK_NULL_HANDLE) { vkDestroyRenderPass(device, masks->offscreen_render_pass, NULL); }
    *masks = (gdmf_vulkan_masks){0};
}

bool gdmf_vulkan_masks_update_descriptor(gdmf_vulkan_masks* masks, uint32_t image_index,
    uint32_t mask_slot, VkImageView color_view, VkImageView mask_view) {
    if (!masks->descriptor_sets || image_index >= masks->image_count ||
        mask_slot >= masks->masks_per_image || color_view == VK_NULL_HANDLE || mask_view == VK_NULL_HANDLE) {
        return false;
    }
    uint32_t slot_index = image_index * masks->masks_per_image + mask_slot;
    VkDescriptorImageInfo image_infos[2] = {
        {.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .imageView = color_view, .sampler = masks->sampler},
        {.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .imageView = mask_view, .sampler = masks->sampler}
    };
    VkWriteDescriptorSet writes[2] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = masks->descriptor_sets[slot_index],
         .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1, .pImageInfo = &image_infos[0]},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = masks->descriptor_sets[slot_index],
         .dstBinding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1, .pImageInfo = &image_infos[1]}
    };
    vkUpdateDescriptorSets(masks->device, 2, writes, 0, NULL);
    return true;
}

int gdmf_vulkan_masks_create(const gdmf_vulkan_mask_config* config, gdmf_vulkan_masks* masks) {
    if (config->device == VK_NULL_HANDLE || config->presentation_render_pass == VK_NULL_HANDLE ||
        config->format == VK_FORMAT_UNDEFINED || !config->image_count || !config->masks_per_image ||
        config->image_count > UINT32_MAX / config->masks_per_image ||
        !config->vertex_shader.words || !config->fragment_shader.words ||
        !config->vertex_shader.byte_count || config->vertex_shader.byte_count % 4 ||
        !config->fragment_shader.byte_count || config->fragment_shader.byte_count % 4) { return -1; }
    uint32_t slot_count = config->image_count * config->masks_per_image;
    if (slot_count > UINT32_MAX / 2 || (size_t)slot_count > SIZE_MAX / sizeof(VkDescriptorSetLayout)) { return -1; }
    VkDevice device = config->device;
    gdmf_vulkan_masks created_masks = {.device = device, .image_count = config->image_count,
        .masks_per_image = config->masks_per_image};
    VkShaderModule vertex_module = VK_NULL_HANDLE, fragment_module = VK_NULL_HANDLE;
    VkResult result;
    VkAttachmentDescription color_attachment = {
        .format         = config->format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkAttachmentReference color_reference = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1, .pColorAttachments = &color_reference
    };
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &color_attachment,
        .subpassCount = 1, .pSubpasses = &subpass,
        .dependencyCount = 1, .pDependencies = &dependency
    };
    result = vkCreateRenderPass(device, &render_pass_info, NULL, &created_masks.offscreen_render_pass);
    if (result != VK_SUCCESS) {
        created_masks.offscreen_render_pass = VK_NULL_HANDLE;
        printf("[Masks] Offscreen render pass creation failed\n");
        goto fail;
    }
    created_masks.format = config->format;
    VkSamplerCreateInfo sampler_info = {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .borderColor  = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .maxLod       = 0.0f
    };
    result = vkCreateSampler(device, &sampler_info, NULL, &created_masks.sampler);
    if (result != VK_SUCCESS) {
        created_masks.sampler = VK_NULL_HANDLE;
        printf("[Masks] Sampler creation failed\n");
        goto fail;
    }
    VkDescriptorSetLayoutBinding bindings[2] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2, .pBindings = bindings
    };
    result = vkCreateDescriptorSetLayout(device, &descriptor_layout_info, NULL, &created_masks.descriptor_layout);
    if (result != VK_SUCCESS) {
        created_masks.descriptor_layout = VK_NULL_HANDLE;
        printf("[Masks] Descriptor set layout creation failed\n");
        goto fail;
    }

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 2 * slot_count
    };
    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = slot_count, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    result = vkCreateDescriptorPool(device, &descriptor_pool_info, NULL, &created_masks.descriptor_pool);
    if (result != VK_SUCCESS) {
        created_masks.descriptor_pool = VK_NULL_HANDLE;
        printf("[Masks] Descriptor pool creation failed\n");
        goto fail;
    }

    created_masks.descriptor_sets = calloc(slot_count, sizeof(VkDescriptorSet));
    if (!created_masks.descriptor_sets) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto fail; }
    {
        VkDescriptorSetLayout* descriptor_layouts = (VkDescriptorSetLayout*)malloc(slot_count * sizeof(VkDescriptorSetLayout));
        if (!descriptor_layouts) { result = VK_ERROR_OUT_OF_HOST_MEMORY; goto fail; }
        for (uint32_t slot_index = 0; slot_index < slot_count; slot_index++) { descriptor_layouts[slot_index] = created_masks.descriptor_layout; }
        VkDescriptorSetAllocateInfo descriptor_allocation_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = created_masks.descriptor_pool,
            .descriptorSetCount = slot_count, .pSetLayouts = descriptor_layouts
        };
        result = vkAllocateDescriptorSets(device, &descriptor_allocation_info, created_masks.descriptor_sets);
        free(descriptor_layouts);
        if (result != VK_SUCCESS) { printf("[Masks] Descriptor set allocation failed: %d\n", result); goto fail; }
    }
    VkShaderModuleCreateInfo vertex_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = config->vertex_shader.byte_count, .pCode = config->vertex_shader.words
    };
    VkShaderModuleCreateInfo fragment_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = config->fragment_shader.byte_count, .pCode = config->fragment_shader.words
    };

    result = vkCreateShaderModule(device, &vertex_shader_info, NULL, &vertex_module);
    if (result != VK_SUCCESS) { vertex_module = VK_NULL_HANDLE; goto fail; }
    result = vkCreateShaderModule(device, &fragment_shader_info, NULL, &fragment_module);
    if (result != VK_SUCCESS) { fragment_module = VK_NULL_HANDLE; goto fail; }
    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vertex_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragment_module, .pName = "main" }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };  
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blend_attachment
    };
    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2, .pDynamicStates = dynamic_states
    };
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(gdmf_vulkan_mask_push_constants)
    };
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &created_masks.descriptor_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push_range
    };
    result = vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &created_masks.pipeline_layout);
    if (result != VK_SUCCESS) {
        created_masks.pipeline_layout = VK_NULL_HANDLE;
        printf("[Masks] Pipeline layout creation failed\n");

        goto fail;
    }
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vertex_input, .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state, .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling, .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state, .layout = created_masks.pipeline_layout,
        .renderPass = config->presentation_render_pass, .subpass = 0
    };
    result = vkCreateGraphicsPipelines(device, config->pipeline_cache, 1, &pipeline_info, NULL, &created_masks.pipeline);
    vkDestroyShaderModule(device, vertex_module, NULL); vertex_module = VK_NULL_HANDLE;
    vkDestroyShaderModule(device, fragment_module, NULL); fragment_module = VK_NULL_HANDLE;
    if (result != VK_SUCCESS) {
        printf("[Masks] Composite pipeline creation failed: %d\n", result);
        goto fail;
    }

    *masks = created_masks;
    printf("[Masks] Compositor ready (%u slots)\n", slot_count);
    return 0;
fail:
    printf("[Masks] Compositor setup failed: %d\n", result);
    if (vertex_module != VK_NULL_HANDLE) { vkDestroyShaderModule(device, vertex_module, NULL); }
    if (fragment_module != VK_NULL_HANDLE) { vkDestroyShaderModule(device, fragment_module, NULL); }
    gdmf_vulkan_masks_destroy(&created_masks);
    return -1;
}


