// Compute resource setup and lifetime. SEE: (GDMF_COMPUTE.md, Vulkan resources)

#include "gdmf_compute_vulkan_private.h"
#include "../../compute/gdmf_compute.h"
#include "../../shaders/decode_raw_comp.h"
#include "../../shaders/decode_rle_comp.h"
#include "../../shaders/decode_masktile_comp.h"

#include <stdlib.h>
#include <string.h>
static bool gdmf_compute_kernel_code(GdmfKernelId id, const uint32_t** code, size_t* byte_count) {
    switch (id) {
    case GDMF_KERNEL_DECODE_RAW:
        *code  = gdmf_decode_raw_spirv;
        *byte_count = gdmf_decode_raw_spirv_byte_count;
        return true;
    case GDMF_KERNEL_DECODE_RLE:
        *code  = gdmf_decode_rle_spirv;
        *byte_count = gdmf_decode_rle_spirv_byte_count;
        return true;
    case GDMF_KERNEL_DECODE_MASKTILE:
        *code  = gdmf_decode_masktile_spirv;
        *byte_count = gdmf_decode_masktile_spirv_byte_count;
        return true;
    case GDMF_KERNEL_COUNT:
        break;
    }
    return false;
}

static gdmf_vulkan_compute gdmf_compute_state;

static void gdmf_compute_destroy_frame_pools(gdmf_vulkan_compute* compute, VkDevice device) {
    if (compute->frame_pools) {
        for (uint32_t image_index = 0; image_index < compute->frame_pool_count; image_index++) {
            if (compute->frame_pools[image_index] != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, compute->frame_pools[image_index], NULL);
            }
        }
        free(compute->frame_pools);
    }
    compute->frame_pools      = NULL;
    compute->frame_pool_count = 0;
}

static bool gdmf_compute_create_kind_layouts(gdmf_vulkan_compute* compute, VkDevice device) {
    VkDescriptorSetLayoutBinding bindings[3] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
    };
    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings    = bindings,
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorSetLayout(device, &create_info, NULL, &layout);
    if (result != VK_SUCCESS) {
        printf("[Compute] vkCreateDescriptorSetLayout failed: %d\n", result);
        return false;
    }
    compute->kind_layouts[GDMF_KERNEL_BIND_BYTES_PALETTE_IMAGE] = layout;
    VkDescriptorSetLayoutBinding mask_bindings[4] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
    };
    VkDescriptorSetLayoutCreateInfo mask_layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 4,
        .pBindings    = mask_bindings,
    };
    result = vkCreateDescriptorSetLayout(device, &mask_layout_info, NULL, &layout);
    if (result != VK_SUCCESS) {
        printf("[Compute] vkCreateDescriptorSetLayout (mask) failed: %d\n", result);
        return false;
    }
    compute->kind_layouts[GDMF_KERNEL_BIND_BYTES_PROBS_PREV_IMAGE] = layout;
    return true;
}

static bool gdmf_compute_create_kernel(gdmf_vulkan_compute* compute, VkDevice device, const GdmfKernelDescription* description) {
    const uint32_t* code; size_t bytes;
    if (!gdmf_compute_kernel_code(description->id, &code, &bytes)) {
        printf("[Compute] No Vulkan code for kernel %s\n", description->name);
        return false;
    }

    VkPushConstantRange push = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = description->push_bytes,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &compute->kind_layouts[description->kind],
        .pushConstantRangeCount = description->push_bytes ? 1u : 0u,
        .pPushConstantRanges    = &push,
    };
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout);
    if (result != VK_SUCCESS) {
        printf("[Compute] vkCreatePipelineLayout(%s) failed: %d\n", description->name, result);
        return false;
    }

    compute->pipeline_layouts[description->id] = pipeline_layout;
    VkShaderModuleCreateInfo shader_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = bytes,
        .pCode    = code,
    };
    VkShaderModule shader;
    result = vkCreateShaderModule(device, &shader_info, NULL, &shader);
    if (result != VK_SUCCESS) {
        printf("[Compute] vkCreateShaderModule(%s) failed: %d\n", description->name, result);
        return false;
    }

    VkComputePipelineCreateInfo create_info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName  = "main",
        },
        .layout = compute->pipeline_layouts[description->id],
    };
    // Pipeline creation may return a live output even on failure; cleanup owns it.
    result = vkCreateComputePipelines(device, gdmf_get_pipeline_cache(), 1, &create_info, NULL, &compute->pipelines[description->id]);
    vkDestroyShaderModule(device, shader, NULL);
    if (result != VK_SUCCESS) {
        printf("[Compute] vkCreateComputePipelines(%s) failed: %d\n", description->name, result);
        return false;
    }
    return true;
}

static bool gdmf_compute_create_frame_pools(gdmf_vulkan_compute* compute, VkDevice device, uint32_t count) {
    compute->frame_pools = calloc(count, sizeof(VkDescriptorPool));
    if (!compute->frame_pools) { return false; }
    compute->frame_pool_count = count;

    VkDescriptorPoolSize sizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = GDMF_VULKAN_COMPUTE_SETS_PER_FRAME * 2 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  .descriptorCount = GDMF_VULKAN_COMPUTE_SETS_PER_FRAME * 2 },
    };
    VkDescriptorPoolCreateInfo create_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = GDMF_VULKAN_COMPUTE_SETS_PER_FRAME,
        .poolSizeCount = 2,
        .pPoolSizes    = sizes,
    };
    for (uint32_t image_index = 0; image_index < count; image_index++) {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkResult result = vkCreateDescriptorPool(device, &create_info, NULL, &pool);
        if (result != VK_SUCCESS) {
            printf("[Compute] vkCreateDescriptorPool failed: %d\n", result);
            return false;
        }
        compute->frame_pools[image_index] = pool;
    }
    return true;
}

bool gdmf_compute_backend_init(void) {
    gdmf_vulkan_compute* compute = &gdmf_compute_state;
    if (compute->built) { return true; }

    VkDevice device = gdmf_get_device();
    if (device == VK_NULL_HANDLE) { return false; }
    uint32_t image_count = gdmf_get_swapchain_image_count();
    if (image_count == 0) { return false; }
    compute->device = device;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gdmf_get_physical_device(), &properties);
    compute->buffer_alignment = (uint32_t)properties.limits.minStorageBufferOffsetAlignment;
    if (compute->buffer_alignment == 0) { compute->buffer_alignment = 1; }

    bool created = gdmf_compute_create_kind_layouts(compute, device);
    for (int kernel_index = 0; created && kernel_index < (int)GDMF_KERNEL_COUNT; kernel_index++) {
        created = gdmf_compute_create_kernel(compute, device, gdmf_kernel_desc((GdmfKernelId)kernel_index));
    }
    if (created) { created = gdmf_compute_create_frame_pools(compute, device, image_count); }

    if (!created) {
        gdmf_compute_backend_shutdown();
        return false;
    }
    compute->built = true;
    return true;
}

void gdmf_compute_backend_shutdown(void) {
    gdmf_vulkan_compute* compute = &gdmf_compute_state;
    VkDevice device = compute->device;
    if (device == VK_NULL_HANDLE) { return; }
    // Caller has quiesced submissions and discarded unsubmitted references.

    gdmf_compute_destroy_frame_pools(compute, device);
    for (int kernel_index = 0; kernel_index < (int)GDMF_KERNEL_COUNT; kernel_index++) {
        if (compute->pipelines[kernel_index] != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, compute->pipelines[kernel_index], NULL);
            compute->pipelines[kernel_index] = VK_NULL_HANDLE;
        }
        if (compute->pipeline_layouts[kernel_index] != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, compute->pipeline_layouts[kernel_index], NULL);
            compute->pipeline_layouts[kernel_index] = VK_NULL_HANDLE;
        }
    }
    for (int kind = 0; kind < (int)GDMF_KERNEL_BIND_KIND_COUNT; kind++) {
        if (compute->kind_layouts[kind] != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, compute->kind_layouts[kind], NULL);
            compute->kind_layouts[kind] = VK_NULL_HANDLE;
        }
    }
    memset(compute, 0, sizeof(*compute));
}

bool gdmf_compute_backend_begin_frame(uint32_t image_index) {
    gdmf_vulkan_compute* compute = &gdmf_compute_state;
    compute->frame_ready = false;
    if (!compute->built || image_index >= compute->frame_pool_count) { return false; }
    VkResult result = vkResetDescriptorPool(compute->device, compute->frame_pools[image_index], 0);
    if (result != VK_SUCCESS) {
        printf("[Compute] vkResetDescriptorPool failed: %d\n", result);
        return false;
    }
    compute->prepared_image = image_index;
    compute->frame_ready = true;
    return true;
}

uint32_t gdmf_compute_buffer_alignment(void) {
    return gdmf_compute_state.buffer_alignment ? gdmf_compute_state.buffer_alignment : 256;
}

const gdmf_vulkan_compute* gdmf_compute_get_state(void) { return &gdmf_compute_state; }

