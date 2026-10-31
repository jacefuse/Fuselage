// Per-image descriptors borrow atlas and per-image cell buffers.
// SEE: (GDMF_TEXT.md, Vulkan graphics resources)
#include "gdmf_textlayer_graphics.h"
#include <stdlib.h>
#include <string.h>

void gdmf_textlayer_descriptors_destroy(gdmf_textlayer_descriptors* descriptors) {
    if (!descriptors || !descriptors->device) {
        return;
    }
    if (descriptors->pool) {
        vkDestroyDescriptorPool(descriptors->device, descriptors->pool, NULL);
    }
    free(descriptors->sets);
    memset(descriptors, 0, sizeof(*descriptors));
}

bool gdmf_textlayer_descriptors_create(const gdmf_textlayer_descriptor_layout* shared_layout,
                                       const gdmf_textlayer_atlas* atlas, const VkBuffer* cell_buffers,
                                       uint32_t image_count, gdmf_textlayer_descriptors* descriptors) {
    if (!shared_layout || !shared_layout->device || !shared_layout->layout || !atlas ||
        atlas->device != shared_layout->device || !atlas->image || !atlas->memory || !atlas->view ||
        !atlas->sampler || !atlas->ready || !cell_buffers || !image_count || !descriptors ||
        descriptors->device || descriptors->pool || descriptors->layout || descriptors->sets ||
        descriptors->image_count || (size_t)image_count > SIZE_MAX / sizeof(VkDescriptorSet)) {
        return false;
    }
    VkDevice device = shared_layout->device;
    // Validate every borrowed buffer before creating or publishing any sets.
    for (uint32_t image_index = 0; image_index < image_count; ++image_index) {
        if (!cell_buffers[image_index]) {
            return false;
        }
    }
    gdmf_textlayer_descriptors pending = {.device = device, .image_count = image_count};
    pending.sets = calloc(image_count, sizeof(*pending.sets));
    if (!pending.sets) {
        return false;
    }
    VkDescriptorSetLayout layout = shared_layout->layout;
    pending.layout = layout;
    VkDescriptorPoolSize sizes[2] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_count},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, image_count},
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = image_count,
        .poolSizeCount = 2,
        .pPoolSizes = sizes,
    };
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &pool_info, NULL, &pool) != VK_SUCCESS) {
        goto fail;
    }
    pending.pool = pool;
    VkDescriptorSetAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkDescriptorImageInfo image_info = {
        .sampler = atlas->sampler,
        .imageView = atlas->view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    for (uint32_t image_index = 0; image_index < image_count; ++image_index) {
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device, &allocation, &set) != VK_SUCCESS) {
            goto fail;
        }
        pending.sets[image_index] = set;
        VkDescriptorBufferInfo buffer_info = {
            .buffer = cell_buffers[image_index],
            .offset = 0,
            .range = GDMF_TEXTLAYER_CELL_BUFFER_SIZE,
        };
        VkWriteDescriptorSet writes[2] = {
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = set,
             .dstBinding = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .pImageInfo = &image_info},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = set,
             .dstBinding = 1,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             .pBufferInfo = &buffer_info},
        };
        vkUpdateDescriptorSets(device, 2, writes, 0, NULL);
    }
    *descriptors = pending;
    return true;
fail:
    gdmf_textlayer_descriptors_destroy(&pending);
    return false;
}

VkDescriptorSet gdmf_textlayer_descriptor_get(const gdmf_textlayer_descriptors* descriptors,
                                              uint32_t image_index) {
    return descriptors && descriptors->sets && image_index < descriptors->image_count
               ? descriptors->sets[image_index]
               : VK_NULL_HANDLE;
}

void gdmf_textlayer_descriptor_layout_destroy(gdmf_textlayer_descriptor_layout* layout) {
    if (!layout || !layout->device) {
        return;
    }
    if (layout->layout) {
        vkDestroyDescriptorSetLayout(layout->device, layout->layout, NULL);
    }
    memset(layout, 0, sizeof(*layout));
}
bool gdmf_textlayer_descriptor_layout_create(VkDevice device, gdmf_textlayer_descriptor_layout* output) {
    if (!device || !output || output->device || output->layout) {
        return false;
    }
    VkDescriptorSetLayoutBinding bindings[2] = {
        {.binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings,
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &layout_info, NULL, &layout) != VK_SUCCESS) {
        return false;
    }
    *output = (gdmf_textlayer_descriptor_layout){device, layout};
    return true;
}
