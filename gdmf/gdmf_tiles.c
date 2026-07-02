/* GDMF Tiles - BUTTOCKS tile layer implementation. Version 0.2.2026062603 */
#include "gdmf_tiles.h"
#include "gdmf_textlayer.h"
#include "gdmf_vulkan_internal.h"
#include "colors.h"
#include "shaders/tile_vert.h"
#include "shaders/tile_frag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


// Internal constants

// Tiles store raw palette indices (0-15), same as the sprite atlas.
#define TILE_ATLAS_FORMAT VK_FORMAT_R8_UINT

// Palette lookup binds GDMF's shared per-swapchain-image buffer (see
// gdmf_get_palette_buffer/GDMF_PALETTE_BUFFER_SIZE in
// gdmf_vulkan_internal.h) instead of a private one per layer -- GDMF
// uploads Colors[256][16] once per frame; every consumer (and, previously,
// every one of up to MAX_TILE_LAYERS tile layers separately) reads the
// same buffer.

// Tile positions are expressed against the same fixed reference canvas as
// sprites and the text layer, so all three layers scale identically on resize.
#define TILE_REFERENCE_CANVAS_WIDTH  1280.0f
#define TILE_REFERENCE_CANVAS_HEIGHT  720.0f

// Initial vertex buffer capacity in tiles (grown on demand).
// Each tile is two triangles = 6 vertices.
#define TILE_INITIAL_VERTEX_CAPACITY 4096

// Internal types

// One vertex in the tile vertex buffer. All transforms are resolved on the
// CPU; the vertex shader is a pure pass-through.
typedef struct {
    float pos[2];        // pre-computed NDC position
    float uv[2];         // 0-1 texture coords, flip already applied
    float tileTypeID;    // atlas array layer index
    float palette;       // palette index (0-255)
    float transparency;  // 0-255
    float showzero;      // 0 or 1
} TileVertex;

// Per-layer Vulkan atlas (one VkImage 2D array per active tile layer).
typedef struct {
    VkImage        image;
    VkDeviceMemory memory;
    VkImageView    view;
    VkSampler      sampler;
    uint16_t       tileWidth;   // tile dimensions this atlas was created for
    uint16_t       tileHeight;
    uint16_t       tileCount;   // number of array layers allocated
} TileAtlas;

// Per-layer, per-swapchain-image GPU resources. No palette buffer here --
// GDMF owns one shared, per-image palette buffer for every consumer (see
// gdmf_get_palette_buffer); this used to be duplicated per layer.
typedef struct {
    VkBuffer        vertexBuffer;
    VkDeviceMemory  vertexMemory;
    uint32_t        vertexCapacity;  // in TileVertex units
    VkDescriptorSet descriptorSet;
} TileFrameResources;

// Static state
// CPU-side tile type table (palette, transparency, showzero per type).
static TileType      tileTypes[MAX_TILE_LAYERS][MAX_TILES];

// CPU-side bitmap mirror for each tile type (unpacked: 1 byte per pixel,
// value 0-15). Kept in step with the GPU atlas by UploadTileBitmap.
// Used for future CPU-side collision sampling via the interactions library.
static unsigned char tileBitmapData[MAX_TILE_LAYERS][MAX_TILES][MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
static bool          tileBitmapValid[MAX_TILE_LAYERS][MAX_TILES];

// Per-layer test pattern bitmap, built at InitTileLayer for that layer's tile
// dimensions. Stored packed (4-bit), max size per layer.
static unsigned char tileTestPattern[MAX_TILE_LAYERS][MAX_TILE_WIDTH * MAX_TILE_HEIGHT / 2];

// Per-layer map state (large -- ~4MB per layer due to location[][] array).
static TileMap tilemaps[MAX_TILE_LAYERS];

// Vulkan per-layer atlas.
static TileAtlas g_tile_atlas[MAX_TILE_LAYERS];

// Per-layer per-frame GPU resources: [layer][imageIndex].
static TileFrameResources* g_tile_frames[MAX_TILE_LAYERS];
static uint32_t            g_tile_frame_count;

// Per-layer per-frame draw vertex count (set in prepare, used in record).
static uint32_t g_tile_draw_vertex_count[MAX_TILE_LAYERS];

// Dirty bitmask per layer: one bit per swapchain image index (0-7). Set all
// bits on any geometry change so every in-flight image gets rebuilt. In
// prepare(), only the bit for the current imageIndex is checked and cleared,
// so every slot is guaranteed a rebuild regardless of presentation order.
// Bits for indices that never appear (e.g. bits 2-7 on a 2-image swapchain)
// remain set but are never checked -- harmless.
static uint8_t g_tile_dirty_images[MAX_TILE_LAYERS];

// Shared pipeline (same vertex layout and shaders for all layers; only the
// descriptor set -- which atlas is bound -- differs per layer).
static VkDescriptorSetLayout g_tile_descriptor_set_layout = VK_NULL_HANDLE;
static VkDescriptorPool      g_tile_descriptor_pool       = VK_NULL_HANDLE;
static VkPipeline            g_tile_vk_pipeline           = VK_NULL_HANDLE;
static VkPipelineLayout      g_tile_vk_layout             = VK_NULL_HANDLE;
static bool                  g_tile_pipeline_ready        = false;

static bool g_tile_atlas_view_active[MAX_TILE_LAYERS];


// Forward declarations
static void mark_tile_layer_dirty(uint8_t l);
static void build_tile_test_pattern(uint8_t layer, uint16_t tileWidth, uint16_t tileHeight);
static int  create_tile_atlas(uint8_t layer);
static void destroy_tile_atlas(uint8_t layer);
static int  ensure_tile_atlas_view_and_sampler(uint8_t layer);
static void record_tile_atlas_init_layout(VkCommandBuffer cmd, void* user_data);
static void record_tile_bitmap_upload(VkCommandBuffer cmd, void* user_data);
static int  ensure_tile_descriptor_set_layout(void);
static int  ensure_tile_descriptor_sets(uint32_t frameCount);
static int  ensure_tile_vertex_buffer(TileFrameResources* frame, uint32_t required_vertices);
static int  ensure_tile_pipeline(void);
static void cleanup_tile_render_resources(void);

// Validation helpers
static bool TileLayerValid(uint8_t layer) {
    return layer < MAX_TILE_LAYERS && tilemaps[layer].initialized;
}

static bool TileIDValid(int tileID, int tileCount) {
    return tileID >= 0 && tileID < tileCount;
}

static bool TileCoordValid(uint8_t layer, uint16_t x, uint16_t y) {
    return x < tilemaps[layer].width && y < tilemaps[layer].height;
}

// Gate for functions that would otherwise produce a visible change. Layer
// must exist and be enabled; functions with no visible effect (bitmap
// upload, collision) do not call this and always proceed past TileLayerValid.
static bool TileLayerWritable(uint8_t layer) {
    if (!TileLayerValid(layer)) { return false; }
    if (!tilemaps[layer].enabled) {
        printf("[Tiles] layer %d is disabled -- visible-change call rejected\n", layer);
        return false;
    }

    return true;
}

// Dirty tracking
static void mark_tile_layer_dirty(uint8_t l) {
    g_tile_dirty_images[l] = 0xFF;

    return;
}

// Test pattern generation
// Builds a checkerboard test pattern for the given tile dimensions. The
// pattern uses 4x4 pixel blocks cycling through all 16 palette indices, same
// visual style as the sprite test pattern. Stored packed (high nibble = left).
static void build_tile_test_pattern(uint8_t layer, uint16_t tileWidth, uint16_t tileHeight) {
    unsigned char* pat = tileTestPattern[layer];
    int stride = tileWidth / 2;

    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth; x++) {
            int gridX = x / 4;
            int gridY = y / 4;
            int colorIndex = (gridY * (tileWidth / 4) + gridX) & 0x0F;
            int byteIndex  = y * stride + x / 2;

            if ((x & 1) == 0) {
                pat[byteIndex] = (pat[byteIndex] & 0x0F) | (unsigned char)(colorIndex << 4);
            } else {
                pat[byteIndex] = (pat[byteIndex] & 0xF0) | (unsigned char)(colorIndex & 0x0F);
            }
        }
    }

    return;
}

// Atlas creation / destruction
typedef struct {
    VkBuffer staging_buffer;
    VkImage  target_image;
    uint32_t layer_index;
    uint32_t width;
    uint32_t height;
    const unsigned char* pixels;  // unpacked (1 byte per pixel)
    uint32_t pixel_count;
} TileBitmapUploadData;

static void record_tile_atlas_init_layout(VkCommandBuffer cmd, void* user_data) {
    VkImageMemoryBarrier* barrier = (VkImageMemoryBarrier*)user_data;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, barrier);

    return;
}

static void record_tile_bitmap_upload(VkCommandBuffer cmd, void* user_data) {
    TileBitmapUploadData* data = (TileBitmapUploadData*)user_data;

    VkImageMemoryBarrier to_transfer = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = data->target_image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, data->layer_index, 1 },
        .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &to_transfer);

    VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, data->layer_index, 1 },
        .imageOffset       = { 0, 0, 0 },
        .imageExtent       = { data->width, data->height, 1 }
    };
    vkCmdCopyBufferToImage(cmd, data->staging_buffer, data->target_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier to_shader = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = data->target_image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, data->layer_index, 1 },
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &to_shader);

    return;
}

static int ensure_tile_atlas_view_and_sampler(uint8_t layer) {
    TileAtlas* atlas = &g_tile_atlas[layer];
    VkDevice   dev   = gdmf_get_device();

    if (atlas->view == VK_NULL_HANDLE) {
        VkImageViewCreateInfo view_info = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = atlas->image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format           = TILE_ATLAS_FORMAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, atlas->tileCount }
        };

        if (vkCreateImageView(dev, &view_info, NULL, &atlas->view) != VK_SUCCESS) {
            printf("[Tiles] Failed to create atlas view for layer %d\n", layer);
            return -1;
        }
    }

    if (atlas->sampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo sampler_info = {
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_NEAREST,
            .minFilter    = VK_FILTER_NEAREST,
            .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .borderColor  = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };

        if (vkCreateSampler(dev, &sampler_info, NULL, &atlas->sampler) != VK_SUCCESS) {
            printf("[Tiles] Failed to create atlas sampler for layer %d\n", layer);
            return -1;
        }
    }

    return 0;
}

static int create_tile_atlas(uint8_t layer) {
    TileAtlas* atlas   = &g_tile_atlas[layer];
    TileMap*   tilemap = &tilemaps[layer];
    VkDevice   dev     = gdmf_get_device();

    VkImageCreateInfo image_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = TILE_ATLAS_FORMAT,
        .extent        = { tilemap->tileWidth, tilemap->tileHeight, 1 },
        .mipLevels     = 1,
        .arrayLayers   = atlas->tileCount,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (vkCreateImage(dev, &image_info, NULL, &atlas->image) != VK_SUCCESS) {
        printf("[Tiles] Failed to create atlas image for layer %d\n", layer);
        return -1;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(dev, atlas->image, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &atlas->memory) != VK_SUCCESS) {
        printf("[Tiles] Failed to allocate atlas memory for layer %d\n", layer);
        vkDestroyImage(dev, atlas->image, NULL);
        atlas->image = VK_NULL_HANDLE;
        return -1;
    }
    vkBindImageMemory(dev, atlas->image, atlas->memory, 0);

    if (ensure_tile_atlas_view_and_sampler(layer) != 0) {
        destroy_tile_atlas(layer);
        return -1;
    }

    // Transition all array layers to SHADER_READ_ONLY_OPTIMAL before any
    // descriptor referencing this image is bound, even for empty slots.
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = atlas->image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, atlas->tileCount },
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT
    };
    if (gdmfExecuteOneTimeCommands(record_tile_atlas_init_layout, &barrier) != 0) {
        printf("[Tiles] Failed to initialize atlas layout for layer %d\n", layer);
        destroy_tile_atlas(layer);
        return -1;
    }

    printf("[Tiles] Layer %d atlas created (%dx%d, %d slots)\n",
           layer, tilemap->tileWidth, tilemap->tileHeight, atlas->tileCount);

    return 0;
}

static void destroy_tile_atlas(uint8_t layer) {
    TileAtlas* atlas = &g_tile_atlas[layer];
    VkDevice   dev   = gdmf_get_device();

    if (atlas->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, atlas->sampler, NULL);
        atlas->sampler = VK_NULL_HANDLE;
    }
    if (atlas->view != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, atlas->view, NULL);
        atlas->view = VK_NULL_HANDLE;
    }
    if (atlas->image != VK_NULL_HANDLE) {
        vkDestroyImage(dev, atlas->image, NULL);
        atlas->image = VK_NULL_HANDLE;
    }
    if (atlas->memory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, atlas->memory, NULL);
        atlas->memory = VK_NULL_HANDLE;
    }

    return;
}

// Lifecycle
bool InitTileLayer(uint8_t layer, uint16_t mapWidth, uint16_t mapHeight,
                   uint16_t tileWidth, uint16_t tileHeight, uint16_t tileCount,
                   float scale) {

    static bool versionshown = false;

    if (versionshown==false){
        Color old = tlGetColor();

        tlPrintFormattedC(GREEN, "[Tile Layers] Version %s", GDMF_TILES_VERSION);tlNewLine();tlSetColor(old);
        versionshown = true;
    }

    if (layer >= MAX_TILE_LAYERS) {
        printf("[Tiles] InitTileLayer: invalid layer %d\n", layer);
        return false;
    }
    if (mapWidth == 0 || mapWidth > MAX_MAP_WIDTH ||
        mapHeight == 0 || mapHeight > MAX_MAP_HEIGHT) {
        printf("[Tiles] InitTileLayer: map size %dx%d out of range\n", mapWidth, mapHeight);
        return false;
    }
    if (tileWidth == 0 || tileWidth > MAX_TILE_WIDTH ||
        tileHeight == 0 || tileHeight > MAX_TILE_HEIGHT) {
        printf("[Tiles] InitTileLayer: tile size %dx%d out of range\n", tileWidth, tileHeight);
        return false;
    }
    if (tileCount == 0 || tileCount > MAX_TILES) {
        printf("[Tiles] InitTileLayer: tileCount %d out of range\n", tileCount);
        return false;
    }
    if (scale <= 0.0f) {
        printf("[Tiles] InitTileLayer: scale must be > 0\n");
        return false;
    }

    TileMap* tilemap = &tilemaps[layer];
    memset(tilemap, 0, sizeof(TileMap));

    tilemap->width        = mapWidth;
    tilemap->height       = mapHeight;
    tilemap->tileWidth    = tileWidth;
    tilemap->tileHeight   = tileHeight;
    tilemap->scale        = scale;
    tilemap->visible      = true;
    tilemap->enabled      = true;
    tilemap->wrapX        = false;
    tilemap->wrapY        = false;
    tilemap->mapOffsetX   = 0.0;
    tilemap->mapOffsetY   = 0.0;
    tilemap->viewportX    = 0;
    tilemap->viewportY    = 0;
    tilemap->viewportWidth  = (uint16_t)TILE_REFERENCE_CANVAS_WIDTH;
    tilemap->viewportHeight = (uint16_t)TILE_REFERENCE_CANVAS_HEIGHT;
    tilemap->collidableColors = 0;

    // All cells default to type 0, no flags, no metadata.
    for (int x = 0; x < mapWidth; x++) {
        for (int y = 0; y < mapHeight; y++) {
            tilemap->location[x][y].tileTypeID = 0;
            tilemap->location[x][y].flags      = 0;
            tilemap->location[x][y].metadata   = NULL;
        }
    }

    // Tile types default to palette 0, fully opaque, showzero off.
    for (int i = 0; i < tileCount; i++) {
        tileTypes[layer][i].palette      = 0;
        tileTypes[layer][i].transparency = 255;
        tileTypes[layer][i].showzero     = false;
    }

    memset(tileBitmapValid[layer], 0, sizeof(tileBitmapValid[layer]));

    build_tile_test_pattern(layer, tileWidth, tileHeight);

    // Set up Vulkan atlas for this layer.
    TileAtlas* atlas = &g_tile_atlas[layer];
    memset(atlas, 0, sizeof(TileAtlas));
    atlas->tileWidth  = tileWidth;
    atlas->tileHeight = tileHeight;
    atlas->tileCount  = tileCount;

    if (create_tile_atlas(layer) != 0) {
        tilemap->initialized = false;
        return false;
    }

    tilemap->initialized = true;
    mark_tile_layer_dirty(layer);
    printf("[Tiles] Layer %d initialized: map %dx%d, tile %dx%d, %d slots, scale %.2f\n",
           layer, mapWidth, mapHeight, tileWidth, tileHeight, tileCount, (double)scale);

    return true;
}

void ShutdownTiles(void) {
    vkDeviceWaitIdle(gdmf_get_device());

    cleanup_tile_render_resources();

    if (g_tile_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(gdmf_get_device(), g_tile_descriptor_pool, NULL);
        g_tile_descriptor_pool = VK_NULL_HANDLE;
    }
    if (g_tile_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(gdmf_get_device(), g_tile_descriptor_set_layout, NULL);
        g_tile_descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (g_tile_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(gdmf_get_device(), g_tile_vk_pipeline, NULL);
        g_tile_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_tile_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(gdmf_get_device(), g_tile_vk_layout, NULL);
        g_tile_vk_layout = VK_NULL_HANDLE;
    }
    g_tile_pipeline_ready = false;

    for (uint8_t layer = 0; layer < MAX_TILE_LAYERS; layer++) {
        if (tilemaps[layer].initialized) {
            destroy_tile_atlas(layer);
            tilemaps[layer].initialized = false;
        }
        if (g_tile_frames[layer]) {
            free(g_tile_frames[layer]);
            g_tile_frames[layer] = NULL;
        }
    }

    printf("[Tiles] Shutdown complete\n");

    return;
}

bool ReleaseTileLayer(uint8_t layer) {
    if (!TileLayerValid(layer)) {
        printf("[Tiles] ReleaseTileLayer: layer %d not initialized\n", layer);
        return false;
    }

    vkDeviceWaitIdle(gdmf_get_device());

    VkDevice dev = gdmf_get_device();

    // Free this layer's per-frame GPU resources (vertex buffers only --
    // GDMF owns the shared palette buffer, not this layer).
    if (g_tile_frames[layer]) {
        for (uint32_t i = 0; i < g_tile_frame_count; i++) {
            TileFrameResources* f = &g_tile_frames[layer][i];

            if (f->vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(dev, f->vertexBuffer, NULL);
                vkFreeMemory(dev, f->vertexMemory, NULL);
            }
        }
        free(g_tile_frames[layer]);
        g_tile_frames[layer] = NULL;
    }

    // The descriptor pool has no VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    // so individual sets can't be freed -- tear down the whole pool instead. It is
    // lazily rebuilt for the remaining active layers on the next prepare() call.
    if (g_tile_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, g_tile_descriptor_pool, NULL);
        g_tile_descriptor_pool = VK_NULL_HANDLE;
    }

    destroy_tile_atlas(layer);
    memset(&g_tile_atlas[layer], 0, sizeof(TileAtlas));

    memset(&tilemaps[layer], 0, sizeof(TileMap));
    memset(tileBitmapValid[layer], 0, sizeof(tileBitmapValid[layer]));
    g_tile_dirty_images[layer]      = 0;
    g_tile_draw_vertex_count[layer] = 0;
    g_tile_atlas_view_active[layer] = false;

    printf("[Tiles] Layer %d shut down\n", layer);

    return true;
}

// Bitmap registration
bool UploadTileBitmap(uint8_t layer, int tileID, const unsigned char* bitmap) {
    if (!TileLayerValid(layer)) {
        printf("[Tiles] UploadTileBitmap: layer %d not initialized\n", layer);
        return false;
    }
    TileAtlas* atlas = &g_tile_atlas[layer];
    if (!TileIDValid(tileID, atlas->tileCount)) {
        printf("[Tiles] UploadTileBitmap: tileID %d out of range for layer %d\n", tileID, layer);
        return false;
    }

    uint16_t tw = atlas->tileWidth;
    uint16_t th = atlas->tileHeight;
    uint32_t pixel_count = (uint32_t)tw * th;

    const unsigned char* src = (bitmap != NULL) ? bitmap : tileTestPattern[layer];

    // Unpack 4-bit source into the CPU mirror (1 byte per pixel, value 0-15).
    unsigned char* mirror = tileBitmapData[layer][tileID];
    for (uint32_t i = 0; i < pixel_count; i += 2) {
        unsigned char packed = src[i / 2];

        mirror[i]     = (packed >> 4) & 0x0F;
        mirror[i + 1] = packed & 0x0F;
    }
    tileBitmapValid[layer][tileID] = true;

    // Stage the unpacked data to the GPU atlas.
    VkDevice dev = gdmf_get_device();

    VkBufferCreateInfo staging_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = pixel_count,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer staging_buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(dev, &staging_info, NULL, &staging_buf) != VK_SUCCESS) {
        printf("[Tiles] UploadTileBitmap: failed to create staging buffer\n");
        return false;
    }

    VkMemoryRequirements staging_req;
    vkGetBufferMemoryRequirements(dev, staging_buf, &staging_req);
    VkMemoryAllocateInfo staging_alloc = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = staging_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(staging_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    if (staging_alloc.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &staging_alloc, NULL, &staging_mem) != VK_SUCCESS) {
        printf("[Tiles] UploadTileBitmap: failed to allocate staging memory\n");
        vkDestroyBuffer(dev, staging_buf, NULL);
        return false;
    }
    vkBindBufferMemory(dev, staging_buf, staging_mem, 0);

    void* mapped;
    vkMapMemory(dev, staging_mem, 0, pixel_count, 0, &mapped);
    memcpy(mapped, mirror, pixel_count);
    vkUnmapMemory(dev, staging_mem);

    TileBitmapUploadData upload_data = {
        .staging_buffer = staging_buf,
        .target_image   = atlas->image,
        .layer_index    = (uint32_t)tileID,
        .width          = tw,
        .height         = th,
        .pixels         = mirror,
        .pixel_count    = pixel_count
    };
    int result = gdmfExecuteOneTimeCommands(record_tile_bitmap_upload, &upload_data);

    vkFreeMemory(dev, staging_mem, NULL);
    vkDestroyBuffer(dev, staging_buf, NULL);

    if (result != 0) {
        printf("[Tiles] UploadTileBitmap: GPU upload failed for layer %d tileID %d\n",
               layer, tileID);
        return false;
    }

    return true;
}

// Tile type appearance
bool SetTilePalette(uint8_t layer, int tileID, uint8_t palette) {
    if (!TileLayerWritable(layer) || !TileIDValid(tileID, g_tile_atlas[layer].tileCount)) { return false; }
    tileTypes[layer][tileID].palette = palette;
    mark_tile_layer_dirty(layer);

    return true;
}

bool SetTileTransparency(uint8_t layer, int tileID, uint8_t transparency) {
    if (!TileLayerWritable(layer) || !TileIDValid(tileID, g_tile_atlas[layer].tileCount)) { return false; }
    tileTypes[layer][tileID].transparency = transparency;
    mark_tile_layer_dirty(layer);

    return true;
}

bool SetTileShowZero(uint8_t layer, int tileID, bool showzero) {
    if (!TileLayerWritable(layer) || !TileIDValid(tileID, g_tile_atlas[layer].tileCount)) { return false; }
    tileTypes[layer][tileID].showzero = showzero;
    mark_tile_layer_dirty(layer);

    return true;
}

void TileTestPattern(uint8_t layer, int tileID) {
    UploadTileBitmap(layer, tileID, NULL);

    return;
}

void TileBoxPattern(uint8_t layer, int tileID) {
    if (!TileLayerValid(layer)) { return; }
    uint16_t w = g_tile_atlas[layer].tileWidth;
    uint16_t h = g_tile_atlas[layer].tileHeight;
    int stride = w / 2;
    // Max tile size is MAX_TILE_WIDTH * MAX_TILE_HEIGHT / 2 = 512 bytes.
    unsigned char box[MAX_TILE_WIDTH * MAX_TILE_HEIGHT / 2];
    memset(box, 0x00, sizeof(box));
    // Top 2 rows and bottom 2 rows: all index 1 (border).
    for (int r = 0; r < 2; r++) {
        memset(&box[r * stride], 0x11, (size_t)stride);
        memset(&box[(h - 1 - r) * stride], 0x11, (size_t)stride);
    }
    // Middle rows: left byte and right byte are index 1 (2 px wide side border).
    for (int r = 2; r < (int)h - 2; r++) {
        box[r * stride]              = 0x11;
        box[r * stride + stride - 1] = 0x11;
    }
    UploadTileBitmap(layer, tileID, box);

    return;
}

// Map placement
bool PlaceTile(uint8_t layer, uint16_t x, uint16_t y, uint16_t tileID) {
    if (!TileLayerWritable(layer) || !TileCoordValid(layer, x, y)) { return false; }
    tilemaps[layer].location[x][y].tileTypeID = tileID;
    mark_tile_layer_dirty(layer);

    return true;
}

bool SetTileFlip(uint8_t layer, uint16_t x, uint16_t y, bool hflip, bool vflip) {
    if (!TileLayerWritable(layer) || !TileCoordValid(layer, x, y)) { return false; }
    uint8_t* flags = &tilemaps[layer].location[x][y].flags;
    *flags &= ~(TILE_FLAG_HFLIP | TILE_FLAG_VFLIP);
    if (hflip) { *flags |= TILE_FLAG_HFLIP; }
    if (vflip) { *flags |= TILE_FLAG_VFLIP; }
    mark_tile_layer_dirty(layer);

    return true;
}

void SetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y, bool enabled) {
    if (!TileLayerValid(layer) || !TileCoordValid(layer, x, y)) { return; }
    if (enabled) { tilemaps[layer].location[x][y].flags |= TILE_FLAG_COLLISION; }
    else { tilemaps[layer].location[x][y].flags &= ~TILE_FLAG_COLLISION; }

    return;
}

// Layer-level settings
bool SetTileMapWrapping(uint8_t layer, bool wrapX, bool wrapY) {
    if (!TileLayerWritable(layer)) { return false; }
    tilemaps[layer].wrapX = wrapX;
    tilemaps[layer].wrapY = wrapY;

    return true;
}

bool SetTileLayerScale(uint8_t layer, float scale) {
    if (!TileLayerWritable(layer) || scale <= 0.0f) { return false; }
    tilemaps[layer].scale = scale;

    return true;
}

bool SetTileViewport(uint8_t layer, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (!TileLayerWritable(layer)) { return false; }
    tilemaps[layer].viewportX      = x;
    tilemaps[layer].viewportY      = y;
    tilemaps[layer].viewportWidth  = width;
    tilemaps[layer].viewportHeight = height;

    return true;
}

bool SetTileMapOffset(uint8_t layer, double x, double y) {
    if (!TileLayerWritable(layer)) { return false; }
    mark_tile_layer_dirty(layer);
    TileMap* m = &tilemaps[layer];
    double tileW = m->tileWidth  * (double)m->scale;
    double tileH = m->tileHeight * (double)m->scale;
    double mapPW = m->width  * tileW;
    double mapPH = m->height * tileH;

    if (m->wrapX) {
        m->mapOffsetX = fmod(x, mapPW);
        if (m->mapOffsetX < 0.0) { m->mapOffsetX += mapPW; }
    } else {
        double maxX = mapPW - m->viewportWidth;

        m->mapOffsetX = (x < 0.0) ? 0.0 : (x > maxX ? maxX : x);
    }

    if (m->wrapY) {
        m->mapOffsetY = fmod(y, mapPH);
        if (m->mapOffsetY < 0.0) { m->mapOffsetY += mapPH; }
    } else {
        double maxY = mapPH - m->viewportHeight;

        m->mapOffsetY = (y < 0.0) ? 0.0 : (y > maxY ? maxY : y);
    }

    return true;
}

bool SetTileOffset(uint8_t layer, uint16_t mapX, uint16_t mapY, int8_t offsetX, int8_t offsetY) {
    if (!TileLayerWritable(layer)) { return false; }
    TileMap* m = &tilemaps[layer];
    if (mapX >= m->width || mapY >= m->height) { return false; }
    m->location[mapX][mapY].offsetX = offsetX;
    m->location[mapX][mapY].offsetY = offsetY;
    mark_tile_layer_dirty(layer);

    return true;
}

bool ScrollTileMap(uint8_t layer, double deltaX, double deltaY) {
    if (!TileLayerWritable(layer)) { return false; }
    TileMap* m = &tilemaps[layer];

    return SetTileMapOffset(layer, m->mapOffsetX + deltaX * m->scale,
                                   m->mapOffsetY + deltaY * m->scale);
}

void SetTileLayerCollidableColors(uint8_t layer, uint16_t mask) {
    if (!TileLayerValid(layer)) { return; }
    tilemaps[layer].collidableColors = mask;

    return;
}

uint16_t GetTileLayerCollidableColors(uint8_t layer) {
    if (!TileLayerValid(layer)) { return 0; }

    return tilemaps[layer].collidableColors;
}

// Visibility -- render-time toggle only, never gates any other function.
bool GetTileLayerVisible(uint8_t layer) {
    if (!TileLayerValid(layer)) { return false; }

    return tilemaps[layer].visible;
}

bool SetTileLayerVisible(uint8_t layer, bool visible) {
    if (!TileLayerValid(layer)) { return false; }
    tilemaps[layer].visible = visible;

    return tilemaps[layer].visible;
}

bool ToggleTileLayerVisible(uint8_t layer) {
    if (!TileLayerValid(layer)) { return false; }
    tilemaps[layer].visible = !tilemaps[layer].visible;

    return tilemaps[layer].visible;
}

// Enablement -- gates write functions that would produce a visible change.
// See TileLayerWritable and the call sites that use it.
bool GetTileLayerEnabled(uint8_t layer) {
    if (!TileLayerValid(layer)) { return false; }

    return tilemaps[layer].enabled;
}

bool SetTileLayerEnabled(uint8_t layer, bool enabled) {
    if (!TileLayerValid(layer)) { return false; }
    tilemaps[layer].enabled = enabled;

    return tilemaps[layer].enabled;
}

bool ToggleTileLayerEnabled(uint8_t layer) {
    if (!TileLayerValid(layer)) { return false; }
    tilemaps[layer].enabled = !tilemaps[layer].enabled;

    return tilemaps[layer].enabled;
}

// Debug / atlas view (stubs -- full implementation deferred)
void ToggleTileAtlasView(uint8_t layer) {
    if (layer >= MAX_TILE_LAYERS) { return; }
    g_tile_atlas_view_active[layer] = !g_tile_atlas_view_active[layer];

    return;
}

bool GetTileAtlasViewActive(uint8_t layer) {
    if (layer >= MAX_TILE_LAYERS) { return false; }

    return g_tile_atlas_view_active[layer];
}

// Pipeline and descriptor management
static int ensure_tile_descriptor_set_layout(void) {
    if (g_tile_descriptor_set_layout != VK_NULL_HANDLE) { return 0; }

    VkDescriptorSetLayoutBinding bindings[2] = {
        {
            .binding         = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
        },
        {
            .binding         = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings
    };
    if (vkCreateDescriptorSetLayout(gdmf_get_device(), &layout_info, NULL,
            &g_tile_descriptor_set_layout) != VK_SUCCESS) {
        printf("[Tiles] Failed to create descriptor set layout\n");
        return -1;
    }

    return 0;
}

// Creates one descriptor set per (layer, swapchain image) pair. The pool is
// sized to cover all layers  all frame slots at once. Each set's palette
// binding points at GDMF's shared per-image palette buffer (see
// gdmf_get_palette_buffer), not a private one of this layer's own.
static int ensure_tile_descriptor_sets(uint32_t frameCount) {
    if (g_tile_descriptor_pool != VK_NULL_HANDLE) { return 0; }

    uint32_t active_layers = 0;
    for (uint8_t l = 0; l < MAX_TILE_LAYERS; l++)
        if (tilemaps[l].initialized) { active_layers++; }

    if (active_layers == 0) { return 0; }

    if (ensure_tile_descriptor_set_layout() != 0) { return -1; }

    VkDevice   dev        = gdmf_get_device();
    uint32_t   total_sets = active_layers * frameCount;

    VkDescriptorPoolSize pool_sizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = total_sets },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         .descriptorCount = total_sets }
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 2,
        .pPoolSizes    = pool_sizes,
        .maxSets       = total_sets
    };
    if (vkCreateDescriptorPool(dev, &pool_info, NULL, &g_tile_descriptor_pool) != VK_SUCCESS) {
        printf("[Tiles] Failed to create descriptor pool\n");
        return -1;
    }

    // Allocate and write descriptor sets per active layer.
    for (uint8_t l = 0; l < MAX_TILE_LAYERS; l++) {
        if (!tilemaps[l].initialized) { continue; }

        if (ensure_tile_atlas_view_and_sampler(l) != 0) { return -1; }

        VkDescriptorSetLayout* layouts = malloc(frameCount * sizeof(VkDescriptorSetLayout));
        VkDescriptorSet*       sets    = malloc(frameCount * sizeof(VkDescriptorSet));
        if (!layouts || !sets) {
            free(layouts); free(sets);
            printf("[Tiles] Out of memory allocating descriptor set arrays\n");
            return -1;
        }
        for (uint32_t i = 0; i < frameCount; i++) layouts[i] = g_tile_descriptor_set_layout;

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = g_tile_descriptor_pool,
            .descriptorSetCount = frameCount,
            .pSetLayouts        = layouts
        };
        VkResult alloc_result = vkAllocateDescriptorSets(dev, &alloc_info, sets);
        free(layouts);
        if (alloc_result != VK_SUCCESS) {
            printf("[Tiles] Failed to allocate descriptor sets for layer %d\n", l);
            free(sets);
            return -1;
        }

        VkDescriptorImageInfo image_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView   = g_tile_atlas[l].view,
            .sampler     = g_tile_atlas[l].sampler
        };

        for (uint32_t i = 0; i < frameCount; i++) {
            TileFrameResources* frame = &g_tile_frames[l][i];

            VkBuffer paletteBuffer = gdmf_get_palette_buffer(i);

            if (paletteBuffer == VK_NULL_HANDLE) {
                printf("[Tiles] Shared palette buffer not ready for image %u\n", i);
                free(sets);
                return -1;
            }
            frame->descriptorSet = sets[i];

            VkDescriptorBufferInfo buf_info = {
                .buffer = paletteBuffer,
                .offset = 0,
                .range  = GDMF_PALETTE_BUFFER_SIZE
            };
            VkWriteDescriptorSet writes[2] = {
                {
                    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet          = frame->descriptorSet,
                    .dstBinding      = 0,
                    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .pImageInfo      = &image_info
                },
                {
                    .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet          = frame->descriptorSet,
                    .dstBinding      = 1,
                    .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .pBufferInfo     = &buf_info
                }
            };
            vkUpdateDescriptorSets(dev, 2, writes, 0, NULL);
        }
        free(sets);
    }

    return 0;
}

static int ensure_tile_vertex_buffer(TileFrameResources* frame, uint32_t required_vertices) {
    if (frame->vertexBuffer != VK_NULL_HANDLE &&
        frame->vertexCapacity >= required_vertices) { return 0; }

    VkDevice dev = gdmf_get_device();
    if (frame->vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        vkFreeMemory(dev, frame->vertexMemory, NULL);
        frame->vertexBuffer   = VK_NULL_HANDLE;
        frame->vertexMemory   = VK_NULL_HANDLE;
        frame->vertexCapacity = 0;
    }

    uint32_t capacity = required_vertices < TILE_INITIAL_VERTEX_CAPACITY
                        ? TILE_INITIAL_VERTEX_CAPACITY : required_vertices;
    VkDeviceSize buf_size = (VkDeviceSize)capacity * sizeof(TileVertex);

    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = buf_size,
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &frame->vertexBuffer) != VK_SUCCESS) {
        printf("[Tiles] Failed to create vertex buffer\n");
        return -1;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(dev, frame->vertexBuffer, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &frame->vertexMemory) != VK_SUCCESS) {
        printf("[Tiles] Failed to allocate vertex buffer memory\n");
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        frame->vertexBuffer = VK_NULL_HANDLE;
        return -1;
    }
    vkBindBufferMemory(dev, frame->vertexBuffer, frame->vertexMemory, 0);
    frame->vertexCapacity = capacity;

    return 0;
}

static int ensure_tile_pipeline(void) {
    if (g_tile_pipeline_ready) { return 0; }

    if (ensure_tile_descriptor_set_layout() != 0) { return -1; }

    VkDevice dev = gdmf_get_device();

    VkShaderModuleCreateInfo vert_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = tile_vert_spv_len,
        .pCode    = (const uint32_t*)tile_vert_spv
    };
    VkShaderModuleCreateInfo frag_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = tile_frag_spv_len,
        .pCode    = (const uint32_t*)tile_frag_spv
    };
    VkShaderModule vert_module = VK_NULL_HANDLE;
    VkShaderModule frag_module = VK_NULL_HANDLE;

    if (vkCreateShaderModule(dev, &vert_info, NULL, &vert_module) != VK_SUCCESS ||
        vkCreateShaderModule(dev, &frag_info, NULL, &frag_module) != VK_SUCCESS) {
        printf("[Tiles] Failed to create shader modules\n");
        if (vert_module) { vkDestroyShaderModule(dev, vert_module, NULL); }
        if (frag_module) { vkDestroyShaderModule(dev, frag_module, NULL); }
        return -1;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName  = "main"
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName  = "main"
        }
    };

    // Vertex input: matches TileVertex field layout.
    VkVertexInputBindingDescription binding = {
        .binding   = 0,
        .stride    = sizeof(TileVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attrs[6] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(TileVertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(TileVertex, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,
          .offset = offsetof(TileVertex, tileTypeID) },
        { .location = 3, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,
          .offset = offsetof(TileVertex, palette) },
        { .location = 4, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,
          .offset = offsetof(TileVertex, transparency) },
        { .location = 5, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,
          .offset = offsetof(TileVertex, showzero) }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 6,
        .pVertexAttributeDescriptions    = attrs
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_NONE,
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth   = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend_attachment
    };

    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dynamic_states
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &g_tile_descriptor_set_layout
    };
    if (vkCreatePipelineLayout(dev, &layout_info, NULL, &g_tile_vk_layout) != VK_SUCCESS) {
        printf("[Tiles] Failed to create pipeline layout\n");
        vkDestroyShaderModule(dev, vert_module, NULL);
        vkDestroyShaderModule(dev, frag_module, NULL);
        return -1;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dynamic_state,
        .layout              = g_tile_vk_layout,
        .renderPass          = gdmf_get_render_pass(),
        .subpass             = 0
    };
    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_info,
            NULL, &g_tile_vk_pipeline) != VK_SUCCESS) {
        printf("[Tiles] Failed to create graphics pipeline\n");
        vkDestroyShaderModule(dev, vert_module, NULL);
        vkDestroyShaderModule(dev, frag_module, NULL);
        vkDestroyPipelineLayout(dev, g_tile_vk_layout, NULL);
        g_tile_vk_layout = VK_NULL_HANDLE;
        return -1;
    }

    vkDestroyShaderModule(dev, vert_module, NULL);
    vkDestroyShaderModule(dev, frag_module, NULL);

    g_tile_pipeline_ready = true;
    printf("[Tiles] Pipeline ready\n");

    return 0;
}

static void cleanup_tile_render_resources(void) {
    VkDevice dev = gdmf_get_device();

    for (uint8_t l = 0; l < MAX_TILE_LAYERS; l++) {
        if (!g_tile_frames[l]) { continue; }
        for (uint32_t i = 0; i < g_tile_frame_count; i++) {
            TileFrameResources* f = &g_tile_frames[l][i];

            if (f->vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(dev, f->vertexBuffer, NULL);
                vkFreeMemory(dev, f->vertexMemory, NULL);
                f->vertexBuffer = VK_NULL_HANDLE;
                f->vertexMemory = VK_NULL_HANDLE;
            }
            // No palette buffer to tear down here -- GDMF owns that now.
        }
    }

    return;
}

// GDMF integration: called from gdmf_vulkan.c each frame

// Ensure per-frame resources are allocated. Handles two cases:
//   (a) frameCount changed (swapchain recreation) -- tear down and rebuild all.
//   (b) frameCount unchanged but a layer was initialized after the first call --
//       layers are initialized lazily by game code between prepare calls, so the
//       early-return on count-match would leave g_tile_frames[l] NULL.
static int ensure_tile_frame_resources(uint32_t frameCount) {
    if (g_tile_frame_count != frameCount) {
        cleanup_tile_render_resources();
        for (uint8_t l = 0; l < MAX_TILE_LAYERS; l++) {
            if (g_tile_frames[l]) { free(g_tile_frames[l]); g_tile_frames[l] = NULL; }
        }
        g_tile_frame_count = frameCount;
    }

    for (uint8_t l = 0; l < MAX_TILE_LAYERS; l++) {
        if (!tilemaps[l].initialized || g_tile_frames[l] != NULL) { continue; }
        g_tile_frames[l] = calloc(frameCount, sizeof(TileFrameResources));
        if (!g_tile_frames[l]) {
            printf("[Tiles] Out of memory for frame resources on layer %d\n", l);
            return -1;
        }
    }

    return 0;
}

// Helper: world pixel coordinates to NDC, using the fixed reference canvas.
static float tile_world_to_ndc_x(float worldX) {
    return (worldX / TILE_REFERENCE_CANVAS_WIDTH) * 2.0f - 1.0f;
}
static float tile_world_to_ndc_y(float worldY) {
    return (worldY / TILE_REFERENCE_CANVAS_HEIGHT) * 2.0f - 1.0f;
}

// Emit 6 vertices (two triangles) for one tile quad into the vertex buffer.
// Flip is applied to UV coordinates here -- no shader involvement needed.
static void emit_tile_quad(TileVertex* verts, uint32_t* count,
                            float screenX, float screenY,
                            float screenW, float screenH,
                            float tileTypeID, float palette,
                            float transparency, float showzero,
                            bool hflip, bool vflip) {
    float x0 = tile_world_to_ndc_x(screenX);
    float y0 = tile_world_to_ndc_y(screenY);
    float x1 = tile_world_to_ndc_x(screenX + screenW);
    float y1 = tile_world_to_ndc_y(screenY + screenH);

    // UV corners: (u0,v0)=top-left, (u1,v1)=bottom-right.
    float u0 = hflip ? 1.0f : 0.0f;
    float u1 = hflip ? 0.0f : 1.0f;
    float v0 = vflip ? 1.0f : 0.0f;
    float v1 = vflip ? 0.0f : 1.0f;

    // Two triangles: top-left, top-right, bottom-left | top-right, bottom-right, bottom-left.
    TileVertex q[6] = {
        { {x0,y0}, {u0,v0}, tileTypeID, palette, transparency, showzero },
        { {x1,y0}, {u1,v0}, tileTypeID, palette, transparency, showzero },
        { {x0,y1}, {u0,v1}, tileTypeID, palette, transparency, showzero },
        { {x1,y0}, {u1,v0}, tileTypeID, palette, transparency, showzero },
        { {x1,y1}, {u1,v1}, tileTypeID, palette, transparency, showzero },
        { {x0,y1}, {u0,v1}, tileTypeID, palette, transparency, showzero }
    };

    memcpy(&verts[*count], q, sizeof(q));
    *count += 6;

    return;
}

// Called from gdmf_vulkan.c before the render pass begins. Builds the vertex
// buffer for each active tile layer. Does not touch Vulkan render state.
void gdmf_tiles_prepare(uint32_t imageIndex) {
    uint32_t frameCount = gdmf_get_swapchain_image_count();

    if (ensure_tile_frame_resources(frameCount) != 0) { return; }
    if (ensure_tile_pipeline() != 0) { return; }
    if (ensure_tile_descriptor_sets(frameCount) != 0) { return; }

    for (uint8_t l = 0; l < MAX_TILE_LAYERS; l++) {
        if (!tilemaps[l].initialized) { continue; }

        TileMap*            m     = &tilemaps[l];
        TileFrameResources* frame = &g_tile_frames[l][imageIndex];

        // Palette upload no longer happens here -- gdmf_vulkan.c's
        // gdmf_palette_prepare() already re-uploaded GDMF's shared buffer
        // for this image index before gdmf_tiles_prepare() was even
        // called; this layer's descriptor set (see
        // ensure_tile_descriptor_sets) is already bound to that same
        // buffer, no per-layer copy needed anymore.

        // Skip vertex rebuild if this image slot is current.
        uint8_t img_bit = (uint8_t)(1u << (imageIndex & 7));
        if ((g_tile_dirty_images[l] & img_bit) == 0) { continue; }
        g_tile_dirty_images[l] &= ~img_bit;

        g_tile_draw_vertex_count[l] = 0;

        float  scale  = m->scale;
        float  tileW  = (float)m->tileWidth  * scale;
        float  tileH  = (float)m->tileHeight * scale;
        double offsetX = m->mapOffsetX;
        double offsetY = m->mapOffsetY;

        // Determine the range of tile cells visible in the viewport, plus one
        // tile of overdraw on each edge to avoid popping at the boundary.
        int tileStartX = (int)(offsetX / tileW) - 1;
        int tileStartY = (int)(offsetY / tileH) - 1;
        int tilesAcross = (int)ceilf((float)m->viewportWidth  / tileW) + 3;
        int tilesDown   = (int)ceilf((float)m->viewportHeight / tileH) + 3;

        uint32_t max_tiles = (uint32_t)(tilesAcross * tilesDown);
        uint32_t required_verts = max_tiles * 6;
        if (ensure_tile_vertex_buffer(frame, required_verts) != 0) { continue; }

        void* mapped;
        vkMapMemory(gdmf_get_device(), frame->vertexMemory, 0,
                    (VkDeviceSize)(required_verts * sizeof(TileVertex)), 0, &mapped);
        TileVertex* verts = (TileVertex*)mapped;
        uint32_t    count = 0;

        // Sub-tile pixel offset for smooth scrolling within one tile's width.
        float subOffsetX = -(float)fmod(offsetX, tileW);
        float subOffsetY = -(float)fmod(offsetY, tileH);

        for (int ty = 0; ty < tilesDown; ty++) {
            for (int tx = 0; tx < tilesAcross; tx++) {
                int mapX = tileStartX + tx;
                int mapY = tileStartY + ty;

                // Handle wrapping or clamped edge.
                if (m->wrapX) {
                    mapX = ((mapX % (int)m->width) + (int)m->width) % (int)m->width;
                } else {
                    if (mapX < 0 || mapX >= (int)m->width) { continue; }
                }
                if (m->wrapY) {
                    mapY = ((mapY % (int)m->height) + (int)m->height) % (int)m->height;
                } else {
                    if (mapY < 0 || mapY >= (int)m->height) { continue; }
                }

                TileLocation* loc  = &m->location[mapX][mapY];
                uint16_t      tid  = loc->tileTypeID;
                if (tid >= (uint16_t)g_tile_atlas[l].tileCount) { continue; }

                TileType* type = &tileTypes[l][tid];

                // Screen position of this tile's top-left corner.
                // tx=0 is the one-tile overdraw to the left of the viewport, so
                // it maps to screenX = viewportX - tileW + subOffsetX (i.e. tx-1).
                float screenX = (float)m->viewportX + (tx - 1) * tileW + subOffsetX + (float)loc->offsetX;
                float screenY = (float)m->viewportY + (ty - 1) * tileH + subOffsetY + (float)loc->offsetY;

                bool hflip = (loc->flags & TILE_FLAG_HFLIP) != 0;
                bool vflip = (loc->flags & TILE_FLAG_VFLIP) != 0;

                emit_tile_quad(verts, &count,
                               screenX, screenY, tileW, tileH,
                               (float)tid,
                               (float)type->palette,
                               (float)type->transparency,
                               type->showzero ? 1.0f : 0.0f,
                               hflip, vflip);
            }
        }
        vkUnmapMemory(gdmf_get_device(), frame->vertexMemory);
        g_tile_draw_vertex_count[l] = count;
    }

    return;
}

// Called from gdmf_vulkan.c inside the active render pass once per tile layer.
// The interleaved render loop calls this between sprite band draws so that
// tile layers and sprite priority groups can be ordered against each other.
// A disabled layer never draws regardless of its visible flag, same as a
// disabled sprite never draws regardless of its own visible flag (see
// gdmf_sprites_prepare) -- visible only matters once a layer is enabled.
void gdmf_tiles_record_layer(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t layer) {
    if (!g_tile_pipeline_ready) { return; }
    if (layer >= MAX_TILE_LAYERS) { return; }
    if (!tilemaps[layer].initialized) { return; }
    if (!tilemaps[layer].visible || !tilemaps[layer].enabled) { return; }
    if (g_tile_draw_vertex_count[layer] == 0) { return; }
    if (!g_tile_frames[layer]) { return; }

    TileFrameResources* frame = &g_tile_frames[layer][imageIndex];
    if (frame->vertexBuffer == VK_NULL_HANDLE) { return; }
    if (frame->descriptorSet == VK_NULL_HANDLE) { return; }

    VkRect2D render_rect = gdmf_get_render_viewport_rect();
    VkViewport viewport = {
        .x        = (float)render_rect.offset.x,
        .y        = (float)render_rect.offset.y,
        .width    = (float)render_rect.extent.width,
        .height   = (float)render_rect.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_tile_vk_pipeline);

    // Per-layer scissor: layer viewport in reference canvas space, mapped
    // to render-rect pixel space, then clamped to the render rect.
    float scaleX = (float)render_rect.extent.width  / TILE_REFERENCE_CANVAS_WIDTH;
    float scaleY = (float)render_rect.extent.height / TILE_REFERENCE_CANVAS_HEIGHT;
    int rx0 = render_rect.offset.x;
    int ry0 = render_rect.offset.y;
    int rx1 = rx0 + (int)render_rect.extent.width;
    int ry1 = ry0 + (int)render_rect.extent.height;

    TileMap* m = &tilemaps[layer];
    int lx = rx0 + (int)((float)m->viewportX * scaleX);
    int ly = ry0 + (int)((float)m->viewportY * scaleY);
    int lw = (int)((float)m->viewportWidth  * scaleX);
    int lh = (int)((float)m->viewportHeight * scaleY);
    int cx0 = lx > rx0 ? lx : rx0;
    int cy0 = ly > ry0 ? ly : ry0;
    int cx1 = (lx + lw) < rx1 ? (lx + lw) : rx1;
    int cy1 = (ly + lh) < ry1 ? (ly + lh) : ry1;
    VkRect2D layer_scissor = {
        .offset  = { cx0, cy0 },
        .extent  = {
            .width  = cx1 > cx0 ? (uint32_t)(cx1 - cx0) : 0,
            .height = cy1 > cy0 ? (uint32_t)(cy1 - cy0) : 0
        }
    };
    vkCmdSetScissor(cmd, 0, 1, &layer_scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_tile_vk_layout, 0, 1, &frame->descriptorSet, 0, NULL);
    VkDeviceSize vb_offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &frame->vertexBuffer, &vb_offset);
    vkCmdDraw(cmd, g_tile_draw_vertex_count[layer], 1, 0, 0);

    return;
}

// Called from the swapchain recreation path in gdmf_vulkan.c. Vertex and
// palette buffers are per-image-index, so they remain valid across a resize
// (only their contents are rebuilt next prepare call). The pipeline uses
// dynamic viewport/scissor, so it too survives without rebuild.
void gdmf_tiles_on_swapchain_recreated(void) {
    return;
}