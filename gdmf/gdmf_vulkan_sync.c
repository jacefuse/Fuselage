#include "gdmf.h"
#include "gdmf_vulkan_sync.h"

// Global synchronization state
GDMFFrameSync g_frameSync[GDMF_MAX_FRAMES_IN_FLIGHT];
uint32_t g_currentFrameIndex = 0;
GDMFSyncStats g_syncStats = { 0 };

// High-precision timing
uint64_t gdmfGetTimestampMicroseconds(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000) / frequency.QuadPart;
}

// Synchronization object management
int gdmfCreateSyncObjects(void) {
    printf("[GDMF Sync] Creating synchronization objects for %d frames in flight...\n", GDMF_MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < GDMF_MAX_FRAMES_IN_FLIGHT; i++) {
        // Create semaphores
        VkSemaphoreCreateInfo semaphoreInfo = { 0 };
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(g_vkDevice, &semaphoreInfo, NULL, &g_frameSync[i].imageAvailableSemaphore) != VK_SUCCESS) {
            printf("[GDMF Sync] Failed to create imageAvailable semaphore for frame %d\n", i);
            // Clean up previously created objects
            for (uint32_t cleanup = 0; cleanup < i; cleanup++) {
                vkDestroySemaphore(g_vkDevice, g_frameSync[cleanup].imageAvailableSemaphore, NULL);
                vkDestroySemaphore(g_vkDevice, g_frameSync[cleanup].renderFinishedSemaphore, NULL);
                vkDestroyFence(g_vkDevice, g_frameSync[cleanup].inFlightFence, NULL);
            }
            return -1;
        }

        if (vkCreateSemaphore(g_vkDevice, &semaphoreInfo, NULL, &g_frameSync[i].renderFinishedSemaphore) != VK_SUCCESS) {
            printf("[GDMF Sync] Failed to create renderFinished semaphore for frame %d\n", i);
            // Clean up this frame's imageAvailable and previous frames
            vkDestroySemaphore(g_vkDevice, g_frameSync[i].imageAvailableSemaphore, NULL);
            for (uint32_t cleanup = 0; cleanup < i; cleanup++) {
                vkDestroySemaphore(g_vkDevice, g_frameSync[cleanup].imageAvailableSemaphore, NULL);
                vkDestroySemaphore(g_vkDevice, g_frameSync[cleanup].renderFinishedSemaphore, NULL);
                vkDestroyFence(g_vkDevice, g_frameSync[cleanup].inFlightFence, NULL);
            }
            return -1;
        }

        // Create fence (starts signaled so first frame doesn't wait)
        VkFenceCreateInfo fenceInfo = { 0 };
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(g_vkDevice, &fenceInfo, NULL, &g_frameSync[i].inFlightFence) != VK_SUCCESS) {
            printf("[GDMF Sync] Failed to create fence for frame %d\n", i);
            // Clean up this frame's semaphores and previous frames
            vkDestroySemaphore(g_vkDevice, g_frameSync[i].imageAvailableSemaphore, NULL);
            vkDestroySemaphore(g_vkDevice, g_frameSync[i].renderFinishedSemaphore, NULL);
            for (uint32_t cleanup = 0; cleanup < i; cleanup++) {
                vkDestroySemaphore(g_vkDevice, g_frameSync[cleanup].imageAvailableSemaphore, NULL);
                vkDestroySemaphore(g_vkDevice, g_frameSync[cleanup].renderFinishedSemaphore, NULL);
                vkDestroyFence(g_vkDevice, g_frameSync[cleanup].inFlightFence, NULL);
            }
            return -1;
        }

        // Initialize frame state
        g_frameSync[i].fenceSignaled = true;  // Starts signaled
        g_frameSync[i].frameStartTime = 0;
        g_frameSync[i].frameEndTime = 0;
        g_frameSync[i].frameNumber = 0;

        printf("[GDMF Sync] Created sync objects for frame %d\n", i);
    }

    // Initialize profiling stats
    g_syncStats.totalFrames = 0;
    g_syncStats.totalFrameTime = 0;
    g_syncStats.averageFrameTime = 0;
    g_syncStats.minFrameTime = UINT64_MAX;
    g_syncStats.maxFrameTime = 0;
    g_syncStats.lastUpdateTime = gdmfGetTimestampMicroseconds();
    g_syncStats.profilingEnabled = false;  // Disabled by default

    printf("[GDMF Sync] Synchronization system initialized\n");
    return 0;
}

void gdmfDestroySyncObjects(void) {
    printf("[GDMF Sync] Destroying synchronization objects...\n");

    // Wait for all operations to complete before cleanup
    vkDeviceWaitIdle(g_vkDevice);

    for (uint32_t i = 0; i < GDMF_MAX_FRAMES_IN_FLIGHT; i++) {
        if (g_frameSync[i].imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_vkDevice, g_frameSync[i].imageAvailableSemaphore, NULL);
            g_frameSync[i].imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (g_frameSync[i].renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_vkDevice, g_frameSync[i].renderFinishedSemaphore, NULL);
            g_frameSync[i].renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (g_frameSync[i].inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(g_vkDevice, g_frameSync[i].inFlightFence, NULL);
            g_frameSync[i].inFlightFence = VK_NULL_HANDLE;
        }
        printf("[GDMF Sync] Destroyed sync objects for frame %d\n", i);
    }

    // Reset global state
    g_currentFrameIndex = 0;
    memset(&g_syncStats, 0, sizeof(g_syncStats));
}

// Frame synchronization functions
int gdmfWaitForFrame(uint32_t frame_index) {
    if (frame_index >= GDMF_MAX_FRAMES_IN_FLIGHT) {
        printf("[GDMF Sync] Invalid frame index: %d\n", frame_index);
        return -1;
    }

    GDMFFrameSync* frame = &g_frameSync[frame_index];

    // Wait for fence only if it's not already signaled
    if (!frame->fenceSignaled) {
        VkResult result = vkWaitForFences(g_vkDevice, 1, &frame->inFlightFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            printf("[GDMF Sync] Failed to wait for fence on frame %d\n", frame_index);
            return -1;
        }
    }

    // Reset fence for next use
    vkResetFences(g_vkDevice, 1, &frame->inFlightFence);
    frame->fenceSignaled = false;

    // Record frame start time for profiling
    if (g_syncStats.profilingEnabled) {
        frame->frameStartTime = gdmfGetTimestampMicroseconds();
    }

    return 0;
}

int gdmfAcquireNextImage(uint32_t frame_index, uint32_t* image_index) {
    if (frame_index >= GDMF_MAX_FRAMES_IN_FLIGHT || !image_index) {
        return -1;
    }

    GDMFFrameSync* frame = &g_frameSync[frame_index];

    VkResult result = vkAcquireNextImageKHR(g_vkDevice, g_vkSwapchain, UINT64_MAX,
        frame->imageAvailableSemaphore, VK_NULL_HANDLE, image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        printf("[GDMF Sync] Swapchain out of date during image acquisition\n");
        return 1;  // Signal swapchain recreation needed
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("[GDMF Sync] Failed to acquire swapchain image\n");
        return -1;
    }

    if (result == VK_SUBOPTIMAL_KHR) {
        printf("[GDMF Sync] Suboptimal swapchain detected\n");
    }

    return 0;
}

int gdmfSubmitFrame(uint32_t frame_index, uint32_t image_index) {
    if (frame_index >= GDMF_MAX_FRAMES_IN_FLIGHT) {
        return -1;
    }

    GDMFFrameSync* frame = &g_frameSync[frame_index];

    // Collect command buffers from enabled/visible layers
    VkCommandBuffer commandBuffers[GDMF_LAYER_COUNT];
    uint32_t commandBufferCount = 0;

    // This is where the integration with the layer system happens
    for (int layer = 0; layer < GDMF_LAYER_COUNT; layer++) {
        if (gdmfIsLayerEnabled(layer) && gdmfIsLayerEnabled(layer)) {
            VkCommandBuffer cmdBuffer = gdmfGetLayerCommandBuffer(layer, image_index);
            if (cmdBuffer != VK_NULL_HANDLE) {
                commandBuffers[commandBufferCount++] = cmdBuffer;
            }
        }
    }

    if (commandBufferCount == 0) {
        // No layers to render, but we still need to signal semaphores
        printf("[GDMF Sync] No layers enabled for rendering\n");
    }

    // Set up synchronization for submission
    VkSemaphore waitSemaphores[] = { frame->imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { frame->renderFinishedSemaphore };

    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = commandBufferCount;
    submitInfo.pCommandBuffers = commandBuffers;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Submit to graphics queue with fence
    if (vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, frame->inFlightFence) != VK_SUCCESS) {
        printf("[GDMF Sync] Failed to submit frame %d\n", frame_index);
        return -1;
    }

    return 0;
}

int gdmfPresentFrame(uint32_t frame_index, uint32_t image_index) {
    if (frame_index >= GDMF_MAX_FRAMES_IN_FLIGHT) {
        return -1;
    }

    GDMFFrameSync* frame = &g_frameSync[frame_index];

    VkSemaphore signalSemaphores[] = { frame->renderFinishedSemaphore };

    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &g_vkSwapchain;
    presentInfo.pImageIndices = &image_index;

    VkResult result = vkQueuePresentKHR(g_presentQueue, &presentInfo);

    // Update frame timing
    if (g_syncStats.profilingEnabled) {
        frame->frameEndTime = gdmfGetTimestampMicroseconds();
        gdmfUpdateFrameTiming(frame_index, frame->frameStartTime, frame->frameEndTime);
    }

    // Increment frame number
    frame->frameNumber++;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        printf("[GDMF Sync] Swapchain needs recreation after present\n");
        return 1;  // Signal swapchain recreation needed
    }
    else if (result != VK_SUCCESS) {
        printf("[GDMF Sync] Failed to present frame\n");
        return -1;
    }

    return 0;
}

// Frame management utilities
uint32_t gdmfGetCurrentFrameIndex(void) {
    return g_currentFrameIndex;
}

bool gdmfFrameReadyCheck(uint32_t frame_index) {
    if (frame_index >= GDMF_MAX_FRAMES_IN_FLIGHT) {
        return false;
    }

    // Check if fence is signaled (frame complete)
    VkResult result = vkGetFenceStatus(g_vkDevice, g_frameSync[frame_index].inFlightFence);
    bool ready = (result == VK_SUCCESS);
    g_frameSync[frame_index].fenceSignaled = ready;
    return ready;
}

void gdmfFrameCompleteSignal(uint32_t frame_index) {
    if (frame_index < GDMF_MAX_FRAMES_IN_FLIGHT) {
        g_frameSync[frame_index].fenceSignaled = true;
        // Advance to next frame
        g_currentFrameIndex = (frame_index + 1) % GDMF_MAX_FRAMES_IN_FLIGHT;
    }
}

// Profiling and debugging
void gdmfEnableSyncProfiling(bool enabled) {
    bool was_enabled = g_syncStats.profilingEnabled;
    g_syncStats.profilingEnabled = enabled;
    if (was_enabled != enabled) {
        printf("[GDMF Sync] Frame profiling %s\n", enabled ? "ENABLED" : "DISABLED");
        if (enabled) {
            gdmfResetSyncStats();
        }
    }
}

bool gdmfSyncProfilingEnabledCheck(void) {
    return g_syncStats.profilingEnabled;
}

uint64_t gdmfGetFrameTime(uint32_t frame_index) {
    if (frame_index >= GDMF_MAX_FRAMES_IN_FLIGHT) {
        return 0;
    }

    GDMFFrameSync* frame = &g_frameSync[frame_index];
    if (frame->frameEndTime > frame->frameStartTime) {
        return frame->frameEndTime - frame->frameStartTime;
    }
    return 0;
}

uint64_t gdmfGetAverageFrameTime(void) {
    return g_syncStats.averageFrameTime;
}

void gdmfUpdateFrameTiming(uint32_t frame_index, uint64_t start_time, uint64_t end_time) {
    if (!g_syncStats.profilingEnabled || end_time <= start_time) {
        return;
    }

    uint64_t frame_time = end_time - start_time;

    // Update statistics
    g_syncStats.totalFrames++;
    g_syncStats.totalFrameTime += frame_time;

    if (frame_time < g_syncStats.minFrameTime) {
        g_syncStats.minFrameTime = frame_time;
    }
    if (frame_time > g_syncStats.maxFrameTime) {
        g_syncStats.maxFrameTime = frame_time;
    }

    // Calculate rolling average (last 60 frames)
    const uint64_t AVERAGE_WINDOW = 60;
    if (g_syncStats.totalFrames <= AVERAGE_WINDOW) {
        g_syncStats.averageFrameTime = g_syncStats.totalFrameTime / g_syncStats.totalFrames;
    }
    else {
        // Simple moving average approximation
        g_syncStats.averageFrameTime = (g_syncStats.averageFrameTime * 59 + frame_time) / 60;
    }
}

void gdmfPrintSyncStats(void) {
    if (!g_syncStats.profilingEnabled || g_syncStats.totalFrames == 0) {
        printf("[GDMF Sync] No profiling data available\n");
        return;
    }

    printf("\n[GDMF Sync] Performance Statistics:\n");
    printf("Total Frames:    %llu\n", g_syncStats.totalFrames);
    printf("Average Frame:   %.2f ms (%.1f FPS)\n",
        g_syncStats.averageFrameTime / 1000.0,
        1000000.0 / g_syncStats.averageFrameTime);
    printf("Min Frame Time:  %.2f ms\n", g_syncStats.minFrameTime / 1000.0);
    printf("Max Frame Time:  %.2f ms\n", g_syncStats.maxFrameTime / 1000.0);
    printf("Frame Variance:  %.2f ms\n",
        (g_syncStats.maxFrameTime - g_syncStats.minFrameTime) / 1000.0);

    // Show current frame states
    printf("\nFrame States:\n");
    for (uint32_t i = 0; i < GDMF_MAX_FRAMES_IN_FLIGHT; i++) {
        printf("Frame %d: %s (Frame #%d)\n", i,
            g_frameSync[i].fenceSignaled ? "READY  " : "BUSY   ",
            g_frameSync[i].frameNumber);
    }
    printf("Current Frame Index: %d\n", g_currentFrameIndex);
}

void gdmfResetSyncStats(void) {
    g_syncStats.totalFrames = 0;
    g_syncStats.totalFrameTime = 0;
    g_syncStats.averageFrameTime = 0;
    g_syncStats.minFrameTime = UINT64_MAX;
    g_syncStats.maxFrameTime = 0;
    g_syncStats.lastUpdateTime = gdmfGetTimestampMicroseconds();

    printf("[GDMF Sync] Performance statistics reset\n");
}