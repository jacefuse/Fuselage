#include "gdmf.h"
#include "gdmf_vulkan_command.h"

// Layer control and debugging
GDMFLayerControl g_layerControls[GDMF_LAYER_COUNT] = {
    { true,  true,  "Text Layer",   0 },  // TEXT - enabled by default for debugging
    { false, false, "Sprite Layer", 0 },  // SPRITE - disabled until implemented
    { false, false, "Tile Layer",   0 },  // TILE - disabled until implemented  
    { false, false, "Pixie Layer",  0 }   // PIXIE - disabled until implemented
};

// Create command pools for each layer
int gdmfCreateCommnadPools(void) {
    printf("[GDMF Command]\nCreating command pools for %d layers...\n", GDMF_LAYER_COUNT);

    for (int layer = 0; layer < GDMF_LAYER_COUNT; layer++) {
        VkCommandPoolCreateInfo poolInfo = { 0 };
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow individual command buffer reset
        poolInfo.queueFamilyIndex = g_selectedDevice->graphics_family;

        if (vkCreateCommandPool(g_vkDevice, &poolInfo, NULL, &g_layerCommandPools[layer]) != VK_SUCCESS) {
            printf("Failed to create command pool for %s\n", g_layerControls[layer].name);
            // Clean up previously created pools
            for (int cleanup = 0; cleanup < layer; cleanup++) {
                vkDestroyCommandPool(g_vkDevice, g_layerCommandPools[cleanup], NULL);
                g_layerCommandPools[cleanup] = VK_NULL_HANDLE;
            }
            return -1;
        }

        printf("Created command pool for %s\n", g_layerControls[layer].name);
    }

    return 0;
}

// Create command buffers for each layer (one per swapchain image)
int gdmfCreateCommandBuffers(void) {
    printf("Creating command buffers (%d per layer)...\n", g_swapchainImageCount);

    for (int layer = 0; layer < GDMF_LAYER_COUNT; layer++) {
        // Allocate array for this layer's command buffers
        g_layerCommandBuffers[layer] = malloc(g_swapchainImageCount * sizeof(VkCommandBuffer));
        if (!g_layerCommandBuffers[layer]) {
            printf("Failed to allocate command buffer array for %s\n", g_layerControls[layer].name);
            // Clean up previously allocated arrays
            for (int cleanup = 0; cleanup < layer; cleanup++) {
                free(g_layerCommandBuffers[cleanup]);
                g_layerCommandBuffers[cleanup] = NULL;
            }
            return -1;
        }

        // Allocate command buffers from the pool
        VkCommandBufferAllocateInfo allocInfo = { 0 };
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = g_layerCommandPools[layer];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = g_swapchainImageCount;

        if (vkAllocateCommandBuffers(g_vkDevice, &allocInfo, g_layerCommandBuffers[layer]) != VK_SUCCESS) {
            printf("Failed to allocate command buffers for %s\n", g_layerControls[layer].name);
            // Clean up this layer's array and previous layers
            free(g_layerCommandBuffers[layer]);
            g_layerCommandBuffers[layer] = NULL;
            for (int cleanup = 0; cleanup < layer; cleanup++) {
                free(g_layerCommandBuffers[cleanup]);
                g_layerCommandBuffers[cleanup] = NULL;
            }
            return -1;
        }

        printf("Created %d command buffers for %s\n",
            g_swapchainImageCount, g_layerControls[layer].name);
    }

    return 0;
}

// Cleanup command pools and buffers
void gdmfDestroyCommandPools(void) {
    printf("[GDMF Command]\nDestroying command pools and buffers...\n");

    // Free command buffer arrays (buffers are freed automatically when pools are destroyed)
    for (int layer = 0; layer < GDMF_LAYER_COUNT; layer++) {
        if (g_layerCommandBuffers[layer]) {
            free(g_layerCommandBuffers[layer]);
            g_layerCommandBuffers[layer] = NULL;
        }
    }

    // Destroy command pools
    for (int layer = 0; layer < GDMF_LAYER_COUNT; layer++) {
        if (g_layerCommandPools[layer] != VK_NULL_HANDLE) {
            vkDestroyCommandPool(g_vkDevice, g_layerCommandPools[layer], NULL);
            g_layerCommandPools[layer] = VK_NULL_HANDLE;
            printf("Destroyed command pool for %s\n", g_layerControls[layer].name);
        }
    }
}

// Layer control functions
void gdmfSetLayerEnabled(GDMFLayer layer, bool enabled) {
    if (layer >= 0 && layer < GDMF_LAYER_COUNT) {
        bool was_enabled = g_layerControls[layer].enabled;
        g_layerControls[layer].enabled = enabled;
        if (was_enabled != enabled) {
            printf("[GDMF Command] %s %s\n",
                g_layerControls[layer].name,
                enabled ? "ENABLED" : "DISABLED");
        }
    }
}

void gdmfSetLayerVisible(GDMFLayer layer, bool visible) {
    if (layer >= 0 && layer < GDMF_LAYER_COUNT) {
        bool was_visible = g_layerControls[layer].visible;
        g_layerControls[layer].visible = visible;
        if (was_visible != visible) {
            printf("[GDMF Command] %s %s\n",
                g_layerControls[layer].name,
                visible ? "VISIBLE" : "HIDDEN");
        }
    }
}

bool gdmfIsLayerEnabled(GDMFLayer layer) {
    if (layer >= 0 && layer < GDMF_LAYER_COUNT) {
        return g_layerControls[layer].enabled;
    }
    return false;
}

const char* gdmfGetLayerName(GDMFLayer layer) {
    if (layer >= 0 && layer < GDMF_LAYER_COUNT) {
        return g_layerControls[layer].name;
    }
    return "Invalid Layer";
}

// Command buffer access
VkCommandBuffer gdmfGetLayerCommandBuffer(GDMFLayer layer, uint32_t frame_index) {
    if (layer >= 0 && layer < GDMF_LAYER_COUNT &&
        frame_index < g_swapchainImageCount &&
        g_layerCommandBuffers[layer] != NULL) {
        return g_layerCommandBuffers[layer][frame_index];
    }
    return VK_NULL_HANDLE;
}

// Basic frame management (to be expanded with synchronization)
int gdmfBeginFrame(uint32_t* image_index) {
    // For now, just get the next swapchain image
    // TODO: Add semaphore synchronization in gdmf_vulkan_sync.c
    VkResult result = vkAcquireNextImageKHR(g_vkDevice, g_vkSwapchain, UINT64_MAX,
        VK_NULL_HANDLE, VK_NULL_HANDLE, image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs to be recreated (window resize, etc.)
        printf("[GDMF Command] Swapchain out of date - recreation needed\n");
        return -1;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("[GDMF Command] Failed to acquire swapchain image\n");
        return -1;
    }

    return 0;
}

int gdmfSubmitLayerCommands(GDMFLayer layer, uint32_t frame_index) {
    // Check if layer should be processed
    if (!gdmfIsLayerEnabled(layer) || !gdmfIsLayerEnabled(layer)) {
        return 0; // Skip disabled/invisible layers
    }

    VkCommandBuffer commandBuffer = gdmfGetLayerCommandBuffer(layer, frame_index);
    if (commandBuffer == VK_NULL_HANDLE) {
        return -1;
    }

    // For now, create a simple submit (will be expanded with proper synchronization)
    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit to graphics queue
    if (vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        printf("[GDMF Command] Failed to submit %s commands\n", gdmfGetLayerName(layer));
        return -1;
    }

    return 0;
}

int gdmfEndFrame(uint32_t image_index) {
    // Present the image
    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &g_vkSwapchain;
    presentInfo.pImageIndices = &image_index;

    VkResult result = vkQueuePresentKHR(g_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        printf("[GDMF Command] Swapchain suboptimal - recreation recommended\n");
        return 1; // Indicate swapchain should be recreated
    }
    else if (result != VK_SUCCESS) {
        printf("[GDMF Command] Failed to present swapchain image\n");
        return -1;
    }

    return 0;
}

// Debug and profiling functions
void gdmfPrintLayerStatus(void) {
    printf("\n[GDMF Command] Layer Status:\n");
    printf("================================\n");
    for (int layer = 0; layer < GDMF_LAYER_COUNT; layer++) {
        printf("%-12s: %s, %s\n",
            g_layerControls[layer].name,
            g_layerControls[layer].enabled ? "ENABLED " : "DISABLED",
            g_layerControls[layer].visible ? "VISIBLE" : "HIDDEN ");
    }
    printf("================================\n\n");
}

uint64_t gdmfGetLayerFrameTime(GDMFLayer layer) {
    if (layer >= 0 && layer < GDMF_LAYER_COUNT) {
        return g_layerControls[layer].frame_time;
    }
    return 0;
}

// One-time command execution for initialization tasks like atlas uploads
int gdmfExecuteOneTimeCommands(GDMFCommandRecordFunc record_func, void* user_data) {
    if (!record_func) {
        printf("[GDMF Command] Invalid record function for one-time command\n");
        return -1;
    }

    // Use the text layer's command pool for one-time operations
    // (Could be made configurable if other layers need this frequently)
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = g_layerCommandPools[GDMF_LAYER_TEXT];
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(g_vkDevice, &alloc_info, &command_buffer) != VK_SUCCESS) {
        printf("[GDMF Command] Failed to allocate one-time command buffer\n");
        return -1;
    }

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        printf("[GDMF Command] Failed to begin one-time command buffer\n");
        vkFreeCommandBuffers(g_vkDevice, g_layerCommandPools[GDMF_LAYER_TEXT], 1, &command_buffer);
        return -1;
    }

    // Record user commands
    record_func(command_buffer, user_data);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        printf("[GDMF Command] Failed to end one-time command buffer\n");
        vkFreeCommandBuffers(g_vkDevice, g_layerCommandPools[GDMF_LAYER_TEXT], 1, &command_buffer);
        return -1;
    }

    // Submit and wait for completion
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    if (vkQueueSubmit(g_graphicsQueue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        printf("[GDMF Command] Failed to submit one-time command buffer\n");
        vkFreeCommandBuffers(g_vkDevice, g_layerCommandPools[GDMF_LAYER_TEXT], 1, &command_buffer);
        return -1;
    }

    // Wait for completion
    vkQueueWaitIdle(g_graphicsQueue);

    // Clean up
    vkFreeCommandBuffers(g_vkDevice, g_layerCommandPools[GDMF_LAYER_TEXT], 1, &command_buffer);

    return 0;
}