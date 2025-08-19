#pragma once

//#include "gdmf_internal.h"

// Layer types for command pool organization
typedef enum GDMFLayer {
    GDMF_LAYER_TEXT = 0,    // Text rendering layer (debug/system text)
    GDMF_LAYER_SPRITE = 1,  // Sprite rendering layer (characters, objects)
    GDMF_LAYER_TILE = 2,    // Tile rendering layer (backgrounds, world)
    GDMF_LAYER_PIXIE = 3,   // Pixie rendering layer (effects, compute)
    GDMF_LAYER_COUNT = 4    // Total number of layers
} GDMFLayer;

// Layer control structure for debugging
typedef struct {
    bool enabled;           // Is this layer active?
    bool visible;          // Should this layer be rendered?
    const char* name;      // Layer name for debugging
    uint64_t frame_time;   // Timing for performance profiling
} GDMFLayerControl;

// Command pool and buffer management
int gdmfCreateCommnadPools(void);
int gdmfCreateCommandBuffers(void);
void gdmfDestroyCommandPools(void);

// Layer control functions
void gdmfSetLayerEnabled(GDMFLayer layer, bool enabled);
void gdmfSetLayerVisible(GDMFLayer layer, bool visible);
bool gdmfIsLayerEnabled(GDMFLayer layer);
bool gdmfIsLayerEnabled(GDMFLayer layer);
const char* gdmfGetLayerName(GDMFLayer layer);

// Command buffer access
VkCommandBuffer gdmfGetLayerCommandBuffer(GDMFLayer layer, uint32_t frame_index);

// Legacy frame functions (deprecated - use GDMFrenderFrame instead)
int gdmfBeginFrame(uint32_t* image_index);
int gdmfSubmitLayerCommands(GDMFLayer layer, uint32_t frame_index);
int gdmfEndFrame(uint32_t image_index);

// Debug/profiling functions
void gdmfPrintLayerStatus(void);
uint64_t gdmfGetLayerFrameTime(GDMFLayer layer);

// One-time command execution for initialization tasks
typedef void (*GDMFCommandRecordFunc)(VkCommandBuffer cmd_buffer, void* user_data);
int gdmfExecuteOneTimeCommands(GDMFCommandRecordFunc record_func, void* user_data);
