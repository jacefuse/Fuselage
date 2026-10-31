// Graphics state shared by both Pixie modes. SEE: (GDMF_PIXIES.md, Frame preparation)
#include "gdmf_pixies_graphics.h"
#include "../../shaders/pixie_vert.h"
#include "../../shaders/pixie_frag.h"
#include "../../shaders/pixie_live_vert.h"
#include "../../shaders/pixie_live_frag.h"
#include <string.h>

void gdmf_pixies_pipeline_destroy(VkDevice device, gdmf_pixies_pipeline* pipeline) {
    if (!device || !pipeline)
        return;
    if (pipeline->pipeline)
        vkDestroyPipeline(device, pipeline->pipeline, NULL);
    if (pipeline->layout)
        vkDestroyPipelineLayout(device, pipeline->layout, NULL);
    memset(pipeline, 0, sizeof *pipeline);
}

bool gdmf_pixies_pipeline_create(VkDevice device, VkRenderPass render_pass, VkPipelineCache cache,
                                 VkDescriptorSetLayout descriptor_layout, bool live,
                                 gdmf_pixies_pipeline* pipeline) {
    if (!device || !render_pass || !pipeline || (!live && !descriptor_layout))
        return false;
    gdmf_pixies_pipeline pending = {0};
    VkShaderModuleCreateInfo vertex_creation = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = live ? gdmf_pixie_live_vertex_spirv_byte_count : gdmf_pixie_vertex_spirv_byte_count,
        .pCode = live ? gdmf_pixie_live_vertex_spirv : gdmf_pixie_vertex_spirv};
    VkShaderModuleCreateInfo fragment_creation = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = live ? gdmf_pixie_live_fragment_spirv_byte_count : gdmf_pixie_fragment_spirv_byte_count,
        .pCode = live ? gdmf_pixie_live_fragment_spirv : gdmf_pixie_fragment_spirv};
    VkShaderModule vertex_module, fragment_module;
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
         .pName = "main"}};

    VkVertexInputBindingDescription binding = {.binding = 0,
                                               .stride = live ? sizeof(gdmf_pixies_live_vertex)
                                                              : sizeof(gdmf_pixies_texture_vertex),
                                               .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[2] = {
        {.location = 0,
         .binding = 0,
         .format = VK_FORMAT_R32G32_SFLOAT,
         .offset = (uint32_t)offsetof(gdmf_pixies_texture_vertex, position)},
        {.location = 1,
         .binding = 0,
         .format = live ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R32G32_SFLOAT,
         .offset = (uint32_t)(live ? offsetof(gdmf_pixies_live_vertex, color)
                                   : offsetof(gdmf_pixies_texture_vertex, uv))}};
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attributes};

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states};
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f};

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment};

    VkPipelineLayoutCreateInfo layout_creation = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                  .setLayoutCount = live ? 0u : 1u,
                                                  .pSetLayouts = live ? NULL : &descriptor_layout};
    if (vkCreatePipelineLayout(device, &layout_creation, NULL, &pending.layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertex_module, NULL);
        vkDestroyShaderModule(device, fragment_module, NULL);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_creation = {.sType =
                                                          VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                      .stageCount = 2,
                                                      .pStages = stages,
                                                      .pVertexInputState = &vertex_input,
                                                      .pInputAssemblyState = &input_assembly,
                                                      .pViewportState = &viewport_state,
                                                      .pRasterizationState = &rasterizer,
                                                      .pMultisampleState = &multisampling,
                                                      .pColorBlendState = &color_blending,
                                                      .pDynamicState = &dynamic_state,
                                                      .layout = pending.layout,
                                                      .renderPass = render_pass,
                                                      .subpass = 0};
    VkResult result =
        vkCreateGraphicsPipelines(device, cache, 1, &pipeline_creation, NULL, &pending.pipeline);

    vkDestroyShaderModule(device, vertex_module, NULL);
    vkDestroyShaderModule(device, fragment_module, NULL);

    if (result != VK_SUCCESS) {
        if (pending.pipeline)
            vkDestroyPipeline(device, pending.pipeline, NULL);
        vkDestroyPipelineLayout(device, pending.layout, NULL);
        pending.layout = VK_NULL_HANDLE;
        return false;
    }

    *pipeline = pending;
    return true;
}
