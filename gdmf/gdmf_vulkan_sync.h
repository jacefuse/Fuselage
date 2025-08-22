#pragma once

#define GDMF_MAX_FRAMES_IN_FLIGHT 2

// Frame synchronization state
typedef struct {
    VkSemaphore imageAvailableSemaphore;  // GPU-GPU: signals when swapchain image is ready
    VkSemaphore renderFinishedSemaphore;  // GPU-GPU: signals when rendering is complete
    VkFence inFlightFence;                // CPU-GPU: signals when frame is complete
    bool fenceSignaled;                   // Track fence state for reset logic
    uint64_t frameStartTime;              // Profiling: when frame began
    uint64_t frameEndTime;                // Profiling: when frame completed
    uint32_t frameNumber;                 // Frame counter for debugging
} GDMFFrameSync;

// Frame timing and profiling
typedef struct {
    uint64_t totalFrames;                 // Total frames rendered
    uint64_t totalFrameTime;              // Cumulative frame time (microseconds)
    uint64_t averageFrameTime;            // Rolling average frame time
    uint64_t minFrameTime;                // Minimum frame time recorded
    uint64_t maxFrameTime;                // Maximum frame time recorded
    uint64_t lastUpdateTime;              // Last time stats were updated
    bool profilingEnabled;                // Enable/disable timing collection
} GDMFSyncStats;

// Synchronization system management
int gdmfCreateSyncObjects(void);
void gdmfDestroySyncObjects(void);

// Frame synchronization functions
int gdmfWaitForFrame(uint32_t frame_index);
int gdmfAcquireNextImage(uint32_t frame_index, uint32_t* image_index);
int gdmfSubmitFrame(uint32_t frame_index, uint32_t image_index);
int gdmfPresentFrame(uint32_t frame_index, uint32_t image_index);

// Frame management utilities
uint32_t gdmfGetCurrentFrameIndex(void);
bool gdmfFrameReadyCheck(uint32_t frame_index);
void gdmfFrameCompleteSignal(uint32_t frame_index);

// Profiling and debugging
void gdmfEnableSyncProfiling(bool enabled);
bool gdmfSyncProfilingEnabledCheck(void);
uint64_t gdmfGetFrameTime(uint32_t frame_index);
uint64_t gdmfGetAverageFrameTime(void);
void gdmfPrintSyncStats(void);
void gdmfResetSyncStats(void);

// Internal timing utilities
uint64_t gdmfGetTimestampMicroseconds(void);
void gdmfUpdateFrameTiming(uint32_t frame_index, uint64_t start_time, uint64_t end_time);

// Frame-in-flight management

extern GDMFFrameSync g_frameSync[GDMF_MAX_FRAMES_IN_FLIGHT];
extern GDMFSyncStats g_syncStats;
extern uint32_t g_currentFrameIndex;