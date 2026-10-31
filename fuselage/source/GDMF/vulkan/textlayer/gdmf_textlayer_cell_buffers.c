// Per-image mapped cell storage. SEE: (GDMF_TEXT.md, Vulkan graphics resources)
#include "gdmf_textlayer_graphics.h"
#include <string.h>

void gdmf_textlayer_cell_buffer_destroy(gdmf_textlayer_cell_buffer* buffer) {
    if (!buffer || !buffer->device) {
        return;
    }
    if (buffer->mapped && buffer->memory) {
        vkUnmapMemory(buffer->device, buffer->memory);
    }
    if (buffer->buffer) {
        vkDestroyBuffer(buffer->device, buffer->buffer, NULL);
    }
    if (buffer->memory) {
        vkFreeMemory(buffer->device, buffer->memory, NULL);
    }
    memset(buffer, 0, sizeof(*buffer));
}

bool gdmf_textlayer_cell_buffer_create(VkDevice device,
                                       const VkPhysicalDeviceMemoryProperties* memory_properties,
                                       gdmf_textlayer_cell_buffer* buffer) {
    if (!device || !memory_properties || !buffer || buffer->device || buffer->buffer || buffer->memory ||
        buffer->mapped) {
        return false;
    }
    gdmf_textlayer_cell_buffer pending = {.device = device};
    VkBufferCreateInfo creation = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = GDMF_TEXTLAYER_CELL_BUFFER_SIZE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer handle = VK_NULL_HANDLE;
    if (vkCreateBuffer(device, &creation, NULL, &handle) != VK_SUCCESS) {
        return false;
    }
    pending.buffer = handle;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, handle, &requirements);
    if (gdmf_allocate_host_visible_prefer_device_local(device, memory_properties, &requirements,
                                                       &pending.memory) != VK_SUCCESS) {
        goto fail;
    }
    if (vkBindBufferMemory(device, handle, pending.memory, 0) != VK_SUCCESS) {
        goto fail;
    }
    void* mapped = NULL;
    if (vkMapMemory(device, pending.memory, 0, creation.size, 0, &mapped) != VK_SUCCESS) {
        goto fail;
    }
    pending.mapped = mapped;
    *buffer = pending;
    return true;
fail:
    gdmf_textlayer_cell_buffer_destroy(&pending);
    return false;
}

bool gdmf_textlayer_cells_pack(const gdmf_textlayer_state* state, gdmf_textlayer_cell* cells,
                               size_t cell_count) {
    if (!state || !cells || cell_count < GDMF_TEXTLAYER_CELL_COUNT) {
        return false;
    }
    for (unsigned row = 0; row < TEXT_LAYER_HEIGHT; ++row) {
        unsigned source_row = TEXT_LAYER_HEIGHT - 1 - row;
        for (unsigned column = 0; column < TEXT_LAYER_WIDTH; ++column) {
            size_t index = row * TEXT_LAYER_WIDTH + column;
            cells[index].character = state->characters[column][source_row];
            cells[index].color = PackColorsRGBA8(state->colors[column][source_row]);
        }
    }
    return true;
}
