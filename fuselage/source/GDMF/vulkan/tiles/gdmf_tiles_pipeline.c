// Tile graphics state. SEE: (GDMF_TILES.md, Vulkan graphics resources)
#include "gdmf_tiles_graphics.h"
#include "../../shaders/tile_vert.h"
#include "../../shaders/tile_frag.h"
#include <stddef.h>
#include <string.h>

void gdmf_tiles_pipeline_destroy(gdmf_tiles_pipeline* pipeline) {
    if (!pipeline || !pipeline->device) {
        return;
    }
    if (pipeline->pipeline) {
        vkDestroyPipeline(pipeline->device, pipeline->pipeline, NULL);
    }
    if (pipeline->layout) {
        vkDestroyPipelineLayout(pipeline->device, pipeline->layout, NULL);
    }
    memset(pipeline, 0, sizeof(*pipeline));
}

bool gdmf_tiles_pipeline_create(VkDevice device, VkRenderPass render_pass, VkPipelineCache cache,
                                VkDescriptorSetLayout descriptor_layout, gdmf_tiles_pipeline* pipeline) {
    if (!device || !render_pass || !descriptor_layout || !pipeline || pipeline->device ||
        pipeline->pipeline || pipeline->layout) {
        return false;
    }
    gdmf_tiles_pipeline pending = {.device = device};
    VkShaderModuleCreateInfo vertex_creation = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = gdmf_tile_vertex_spirv_byte_count,
        .pCode = gdmf_tile_vertex_spirv,
    };
    VkShaderModuleCreateInfo fragment_creation = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = gdmf_tile_fragment_spirv_byte_count,
        .pCode = gdmf_tile_fragment_spirv,
    };
    VkShaderModule vertex_module = VK_NULL_HANDLE, fragment_module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &vertex_creation, NULL, &vertex_module) != VK_SUCCESS) {
        return false;
    }
    if (vkCreateShaderModule(device, &fragment_creation, NULL, &fragment_module) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertex_module, NULL);
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = vertex_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = fragment_module,
         .pName = "main"},
    };
    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(gdmf_tile_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attributes[6] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(gdmf_tile_vertex, position)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(gdmf_tile_vertex, uv)},
        {2, 0, VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(gdmf_tile_vertex, bitmap_id)},
        {3, 0, VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(gdmf_tile_vertex, palette)},
        {4, 0, VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(gdmf_tile_vertex, opacity)},
        {5, 0, VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(gdmf_tile_vertex, show_zero)},
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 6,
        .pVertexAttributeDescriptions = attributes,
    };
    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };
    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_layout,
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult result = vkCreatePipelineLayout(device, &layout_info, NULL, &layout);
    if (result == VK_SUCCESS) {
        pending.layout = layout;
        VkGraphicsPipelineCreateInfo creation = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &raster,
            .pMultisampleState = &multisample,
            .pColorBlendState = &blend,
            .pDynamicState = &dynamic,
            .layout = layout,
            .renderPass = render_pass,
            .subpass = 0,
        };
        result = vkCreateGraphicsPipelines(device, cache, 1, &creation, NULL, &pending.pipeline);
    }
    vkDestroyShaderModule(device, vertex_module, NULL);
    vkDestroyShaderModule(device, fragment_module, NULL);
    if (result != VK_SUCCESS) {
        // Graphics creation can return a valid partial pipeline on failure.
        gdmf_tiles_pipeline_destroy(&pending);
        return false;
    }
    *pipeline = pending;
    return true;
}
