// Dispatch recording. SEE: (GDMF_COMPUTE.md, Dispatch contract)
#include "gdmf_compute_vulkan_private.h"

bool gdmf_compute_record(VkCommandBuffer command_buffer, uint32_t image_index, const GdmfComputeDispatch* dispatch) {
    const gdmf_vulkan_compute* compute = gdmf_compute_get_state();
    if (!dispatch || !command_buffer || !compute->frame_ready || image_index != compute->prepared_image) { return false; }
    if (!compute->built || image_index >= compute->frame_pool_count) { return false; }
    const GdmfKernelDescription* description = gdmf_kernel_desc(dispatch->kernel);
    if (!description || dispatch->push_bytes != description->push_bytes) { return false; }
    if (dispatch->bytes.offset % compute->buffer_alignment || dispatch->palette.offset % compute->buffer_alignment) {
        printf("[Compute] %s: buffer offset not aligned to %u\n", description->name, compute->buffer_alignment);
        return false;
    }
    if (dispatch->groups_x == 0 || dispatch->groups_y == 0) { return true; }
    if (!dispatch->push || !dispatch->bytes.handle || !dispatch->palette.handle ||
        !dispatch->bytes.size || !dispatch->palette.size ||
        !dispatch->image.image || !dispatch->image.view ||
        !dispatch->image.width || !dispatch->image.height) { return false; }
    if (description->kind == GDMF_KERNEL_BIND_BYTES_PROBS_PREV_IMAGE &&
        (!dispatch->previous.image || !dispatch->previous.view ||
         dispatch->previous.image == dispatch->image.image ||
         dispatch->previous.width != dispatch->image.width ||
         dispatch->previous.height != dispatch->image.height)) { return false; }

    VkDevice device = compute->device;

    VkDescriptorSetAllocateInfo allocation_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = compute->frame_pools[image_index],
        .descriptorSetCount = 1,
        .pSetLayouts        = &compute->kind_layouts[description->kind],
    };
    VkDescriptorSet descriptor_set;
    if (vkAllocateDescriptorSets(device, &allocation_info, &descriptor_set) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorBufferInfo bytes_info = {
        .buffer = (VkBuffer)dispatch->bytes.handle,   .offset = dispatch->bytes.offset,   .range = dispatch->bytes.size };
    VkDescriptorBufferInfo palette_info = {
        .buffer = (VkBuffer)dispatch->palette.handle, .offset = dispatch->palette.offset, .range = dispatch->palette.size };
    VkDescriptorImageInfo image_info = {
        .imageView = (VkImageView)dispatch->image.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
    const bool two_images = (description->kind == GDMF_KERNEL_BIND_BYTES_PROBS_PREV_IMAGE);
    VkDescriptorImageInfo previous_info = {
        .imageView = (VkImageView)dispatch->previous.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

    VkWriteDescriptorSet writes[4] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 0,
          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &bytes_info },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 1,
          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &palette_info },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set,
          .dstBinding = 2,
          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = two_images ? &previous_info : &image_info },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 3,
          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &image_info },
    };
    vkUpdateDescriptorSets(device, two_images ? 4u : 3u, writes, 0, NULL);

    VkImageSubresourceRange subresource_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 };
    // Order prior sampling, uploads, and compute before this image access.
    VkImageMemoryBarrier input_barriers[2] = {
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = (VkImage)dispatch->image.image,
            .subresourceRange    = subresource_range,
        },
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = (VkImage)dispatch->previous.image,
            .subresourceRange    = subresource_range,
        },
    };
    vkCmdPipelineBarrier(command_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, NULL, 0, NULL, two_images ? 2u : 1u, input_barriers);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipelines[description->id]);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline_layouts[description->id],
                            0, 1, &descriptor_set, 0, NULL);
    if (description->push_bytes) {
        vkCmdPushConstants(command_buffer, compute->pipeline_layouts[description->id], VK_SHADER_STAGE_COMPUTE_BIT,
                           0, description->push_bytes, dispatch->push);
    }
    vkCmdDispatch(command_buffer, dispatch->groups_x, dispatch->groups_y, 1);
    // Restore the sampling layout for both roles in a mask image pair.
    VkImageMemoryBarrier output_barriers[2] = {
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = (VkImage)dispatch->image.image,
            .subresourceRange    = subresource_range,
        },
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = (VkImage)dispatch->previous.image,
            .subresourceRange    = subresource_range,
        },
    };
    vkCmdPipelineBarrier(command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, two_images ? 2u : 1u, output_barriers);

    return true;
}

