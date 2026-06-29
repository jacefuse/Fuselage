#include "gdmf_sprites.h"
#include "gdmf_textlayer.h"
#include "gdmf_vulkan_internal.h"
#include "colors.h"
#include "shaders/sprite_vert.h"
#include "shaders/sprite_frag.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SPRITE_BITMAP_BYTES (SPRITE_WIDTH * SPRITE_HEIGHT / 2)  // 2 pixels per byte
#define SPRITE_ATLAS_FORMAT VK_FORMAT_R8_UINT                   // raw palette index, 0-15; no filtering/blending
#define SPRITE_PALETTE_BUFFER_SIZE (256 * 16 * sizeof(uint32_t)) // matches sizeof(Colors), one uint per Color

// Sprite positions/sizes are interpreted against this fixed reference
// canvas, not live window pixels -- matches the text layer's implicit
// native resolution (its 80x45 grid of 16px cells = 1280x720). Keeping both
// layers on the same reference canvas means a coordinate means the same
// place in either one, and both scale identically when the window resizes.
#define SPRITE_REFERENCE_CANVAS_WIDTH  1280.0f
#define SPRITE_REFERENCE_CANVAS_HEIGHT 720.0f

// Atlas debug view: a reserved block of sprite indices at the top of the
// MAX_SPRITES range (never touched by normal sprite use, so the view never
// has anything to save/restore -- just hide them on close). Rendered as raw
// grayscale in the fragment shader (see SpriteVertex.rawGrayscale below),
// not via any palette -- the atlas stores no color of its own to look up.
#define ATLAS_VIEW_SPRITE_COUNT 256
#define ATLAS_VIEW_SPRITE_BASE  (MAX_SPRITES - ATLAS_VIEW_SPRITE_COUNT)
#define ATLAS_VIEW_MAX_SCALE    2.0f

static Sprite sprites[MAX_SPRITES];

// CPU-side mirror of the atlas, kept in step by UploadSpriteBitmap. Collision
// math needs raw index bytes; reading them back from the GPU atlas every
// check would be far too slow, so a copy lives here instead.
static unsigned char spriteBitmapData[MAX_SPRITE_BITMAPS][SPRITE_BITMAP_BYTES];
static bool          spriteBitmapValid[MAX_SPRITE_BITMAPS];

// GPU-side atlas: one VkImage, MAX_SPRITE_BITMAPS array layers, each a
// SPRITE_WIDTH x SPRITE_HEIGHT slot holding unpacked (1 byte/pixel) palette
// indices. A bitmap ID is simply an array layer index.
typedef struct {
    VkImage        atlas_image;
    VkDeviceMemory atlas_memory;
    VkImageView    atlas_view;
    VkSampler      atlas_sampler;
} VulkanSpriteAtlas;

typedef struct {
    VkBuffer       staging_buffer;
    VkImage        atlas_image;
    SpriteBitmapID bitmapID;
} SpriteAtlasUploadData;

static VulkanSpriteAtlas g_sprite_atlas = { 0 };

// Per-vertex draw data. Rotation/scale/skew are resolved into a final NDC
// position on the CPU each frame, same philosophy as the text layer's
// per-cell vertex generation -- the vertex shader just passes data through.
typedef struct {
    float pos[2];
    float uv[2];
    float bitmapID;
    float palette;
    float transparency;
    float showzero;
    float rawGrayscale;  // 0 or 1 -- bypass the palette entirely (atlas debug view)
} SpriteVertex;

static VkDescriptorSetLayout g_sprite_descriptor_set_layout = VK_NULL_HANDLE;
static VkDescriptorPool      g_sprite_descriptor_pool       = VK_NULL_HANDLE;

// One full set of per-frame GPU-written resources per swapchain image.
// gdmf_vulkan_render_frame waits on a per-image fence before recording that
// image's command buffer, but that only guarantees the *previous* frame
// that used this same image index has finished -- a different image index's
// command buffer, submitted more recently, can still be executing on the
// GPU concurrently. Without per-image copies, gdmf_sprites_prepare() would
// overwrite one shared vertex/palette buffer that an in-flight frame's GPU
// work might still be reading. Keyed by image index, sized once at pipeline
// creation to the swapchain's image count -- the same granularity the
// renderer already uses for command buffers/fences/framebuffers.
typedef struct {
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexMemory;
    uint32_t       vertexCapacity;  // vertices
    VkBuffer       paletteBuffer;
    VkDeviceMemory paletteMemory;
    VkDescriptorSet descriptorSet;
} SpriteFrameResources;

static SpriteFrameResources* g_sprite_frames      = NULL;  // [g_sprite_frame_count]
static uint32_t              g_sprite_frame_count = 0;

static VkPipeline            g_sprite_vk_pipeline           = VK_NULL_HANDLE;
static VkPipelineLayout      g_sprite_vk_layout             = VK_NULL_HANDLE;
static bool                  g_sprite_pipeline_ready        = false;
static bool                  g_sprite_active_this_frame     = false;
static uint32_t              g_sprite_draw_vertex_count     = 0;
static int                   g_sprite_draw_order[MAX_SPRITES];

static bool                  g_atlasViewActive               = false;

// Number of sprite priority bands. Must equal MAX_TILE_LAYERS (gdmf_tiles.h)
// because gdmf_vulkan.c interleaves one tile layer and one sprite band per
// loop iteration. 256 priority levels / 16 bands = 16 priorities per band.
#define SPRITE_PRIORITY_BANDS 16

// Per-band vertex slice computed each frame in gdmf_sprites_prepare after the
// priority sort. Band N covers sprite priorities [N*16, N*16+15]. Used by
// gdmf_sprites_record_band so the interleaved render loop can draw one band
// at a time between tile layer draws.
static uint32_t g_sprite_band_first_vertex[SPRITE_PRIORITY_BANDS];
static uint32_t g_sprite_band_vertex_count[SPRITE_PRIORITY_BANDS];

static void create_vulkan_sprite_atlas(void);
static void destroy_vulkan_sprite_atlas(void);
static int  ensure_sprite_atlas_view_and_sampler(void);
static void record_sprite_atlas_init_layout(VkCommandBuffer cmd, void* user_data);
static void record_sprite_bitmap_upload(VkCommandBuffer cmd, void* user_data);
static int  ensure_sprite_descriptor_set_layout(void);
static int  ensure_sprite_palette_buffer(SpriteFrameResources* frame);
static int  ensure_sprite_descriptor_sets(uint32_t frameCount);
static int  ensure_sprite_vertex_buffer(SpriteFrameResources* frame, uint32_t required_vertices);
static int  ensure_sprite_pipeline(void);
static void cleanup_sprite_render_resources(void);
static void RunSpriteCollisions(void);

static unsigned char testsprite[SPRITE_BITMAP_BYTES];

// Per-sprite, per-frame collision results. Cleared and repopulated by
// RunSpriteCollisions every frame; query functions just read these.
static SpriteCollisionInfo g_spriteCollisions[MAX_SPRITES][MAX_COLLISIONS_PER_SPRITE];
static int                 g_spriteCollisionCount[MAX_SPRITES];

// Per-sprite cache of this frame's world-space AABB, rebuilt once per
// frame in RunSpriteCollisions rather than recomputed per pair.
typedef struct {
    float minX, minY, maxX, maxY;
} SpriteAABB;
static SpriteAABB g_spriteCollisionAABB[MAX_SPRITES];

static bool SpriteIndexValid(int spriteIndex) {
    return spriteIndex >= 0 && spriteIndex < MAX_SPRITES;
}

static bool BitmapIDValid(SpriteBitmapID bitmapID) {
    return bitmapID >= 0 && bitmapID < MAX_SPRITE_BITMAPS;
}

// Atlas creation. The image starts at VK_IMAGE_LAYOUT_UNDEFINED with no
// data; individual layers are populated on demand by UploadSpriteBitmap.
static void create_vulkan_sprite_atlas(void) {
    VkDevice dev = gdmf_get_device();

    VkImageCreateInfo image_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = SPRITE_ATLAS_FORMAT,
        .extent        = { SPRITE_WIDTH, SPRITE_HEIGHT, 1 },
        .mipLevels     = 1,
        .arrayLayers   = MAX_SPRITE_BITMAPS,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (vkCreateImage(dev, &image_info, NULL, &g_sprite_atlas.atlas_image) != VK_SUCCESS) {
        printf("[Sprites] Failed to create sprite atlas image\n");
        return;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(dev, g_sprite_atlas.atlas_image, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &g_sprite_atlas.atlas_memory) != VK_SUCCESS) {
        printf("[Sprites] Failed to allocate sprite atlas memory\n");
        vkDestroyImage(dev, g_sprite_atlas.atlas_image, NULL);
        g_sprite_atlas.atlas_image = VK_NULL_HANDLE;
        return;
    }
    vkBindImageMemory(dev, g_sprite_atlas.atlas_image, g_sprite_atlas.atlas_memory, 0);

    if (ensure_sprite_atlas_view_and_sampler() != 0) {
        destroy_vulkan_sprite_atlas();
        return;
    }

    // The descriptor set's view spans all MAX_SPRITE_BITMAPS layers, and Vulkan
    // requires every layer covered by a bound descriptor's view to be in the
    // layout declared at write time -- not just the layers actually sampled.
    // So every layer needs a real layout before any descriptor referencing
    // this image is ever used, even the ones with no data uploaded yet.
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = g_sprite_atlas.atlas_image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, MAX_SPRITE_BITMAPS },
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT
    };
    if (gdmfExecuteOneTimeCommands(record_sprite_atlas_init_layout, &barrier) != 0) {
        printf("[Sprites] Failed to initialize sprite atlas layout\n");
        destroy_vulkan_sprite_atlas();
        return;
    }

    printf("[Sprites] Sprite bitmap atlas created (%d slots)\n", MAX_SPRITE_BITMAPS);

    return;
}

static void destroy_vulkan_sprite_atlas(void) {
    VkDevice dev = gdmf_get_device();

    if (g_sprite_atlas.atlas_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, g_sprite_atlas.atlas_sampler, NULL);
        g_sprite_atlas.atlas_sampler = VK_NULL_HANDLE;
    }
    if (g_sprite_atlas.atlas_view != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, g_sprite_atlas.atlas_view, NULL);
        g_sprite_atlas.atlas_view = VK_NULL_HANDLE;
    }
    if (g_sprite_atlas.atlas_image != VK_NULL_HANDLE) {
        vkDestroyImage(dev, g_sprite_atlas.atlas_image, NULL);
        g_sprite_atlas.atlas_image = VK_NULL_HANDLE;
    }
    if (g_sprite_atlas.atlas_memory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, g_sprite_atlas.atlas_memory, NULL);
        g_sprite_atlas.atlas_memory = VK_NULL_HANDLE;
    }

    return;
}

static int ensure_sprite_atlas_view_and_sampler(void) {
    VkDevice dev = gdmf_get_device();

    if (g_sprite_atlas.atlas_view == VK_NULL_HANDLE) {
        VkImageViewCreateInfo view_info = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = g_sprite_atlas.atlas_image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format           = SPRITE_ATLAS_FORMAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, MAX_SPRITE_BITMAPS }
        };

        if (vkCreateImageView(dev, &view_info, NULL, &g_sprite_atlas.atlas_view) != VK_SUCCESS) {
            printf("[Sprites] Failed to create sprite atlas image view\n");
            return -1;
        }
    }
    if (g_sprite_atlas.atlas_sampler == VK_NULL_HANDLE) {
        // SPRITE_ATLAS_FORMAT is an integer format: Vulkan requires NEAREST
        // filtering for these (linear filtering of palette indices would be
        // meaningless anyway -- the actual color comes from the palette
        // lookup in the fragment shader, not the atlas).
        VkSamplerCreateInfo samp_info = {
            .sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter     = VK_FILTER_NEAREST,
            .minFilter     = VK_FILTER_NEAREST,
            .mipmapMode    = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxAnisotropy = 1.0f
        };

        if (vkCreateSampler(dev, &samp_info, NULL, &g_sprite_atlas.atlas_sampler) != VK_SUCCESS) {
            printf("[Sprites] Failed to create sprite atlas sampler\n");
            return -1;
        }
    }

    return 0;
}

// One-time command callback: transitions every layer of a freshly created
// atlas straight to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL before any data
// exists. Required because the descriptor set's view spans all layers, and
// Vulkan requires every layer a bound descriptor's view covers to be in the
// layout declared when the descriptor was written -- not just the layers
// actually sampled that draw. Sampling an untouched layer just reads garbage,
// which is fine; the layout itself must still be valid.
static void record_sprite_atlas_init_layout(VkCommandBuffer cmd_buffer, void* user_data) {
    VkImageMemoryBarrier* barrier = (VkImageMemoryBarrier*)user_data;

    vkCmdPipelineBarrier(cmd_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, barrier);

    return;
}

// Per-slot upload (one-time command callback). Every layer already rests at
// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (see record_sprite_atlas_init_layout),
// so every upload -- first or repeat -- transitions from there uniformly.
static void record_sprite_bitmap_upload(VkCommandBuffer cmd_buffer, void* user_data) {
    SpriteAtlasUploadData* upload_data = (SpriteAtlasUploadData*)user_data;

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = upload_data->atlas_image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, (uint32_t)upload_data->bitmapID, 1 },
        .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)upload_data->bitmapID, 1 },
        .imageOffset       = { 0, 0, 0 },
        .imageExtent       = { SPRITE_WIDTH, SPRITE_HEIGHT, 1 }
    };
    vkCmdCopyBufferToImage(cmd_buffer, upload_data->staging_buffer,
        upload_data->atlas_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    return;
}

// Descriptor set layout: binding 0 is the atlas (sampled in the fragment
// shader), binding 1 is the palette lookup table.
static int ensure_sprite_descriptor_set_layout(void) {
    if (g_sprite_descriptor_set_layout != VK_NULL_HANDLE) { return 0; }

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
            &g_sprite_descriptor_set_layout) != VK_SUCCESS) {
        printf("[Sprites] Failed to create descriptor set layout\n");
        return -1;
    }

    return 0;
}

// Palette buffer: a host-visible mirror of Colors[256][16], re-uploaded in
// full every frame (16 KiB -- trivial bandwidth) since palettes can change
// at runtime via SetPalette. One per frame slot -- see SpriteFrameResources.
static int ensure_sprite_palette_buffer(SpriteFrameResources* frame) {
    if (frame->paletteBuffer != VK_NULL_HANDLE) { return 0; }

    VkDevice dev = gdmf_get_device();
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = SPRITE_PALETTE_BUFFER_SIZE,
        .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &frame->paletteBuffer) != VK_SUCCESS) {
        printf("[Sprites] Failed to create palette buffer\n");
        return -1;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(dev, frame->paletteBuffer, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &frame->paletteMemory) != VK_SUCCESS) {
        printf("[Sprites] Failed to allocate palette buffer memory\n");
        vkDestroyBuffer(dev, frame->paletteBuffer, NULL);
        frame->paletteBuffer = VK_NULL_HANDLE;
        return -1;
    }
    vkBindBufferMemory(dev, frame->paletteBuffer, frame->paletteMemory, 0);

    return 0;
}

// Allocates one descriptor set per frame slot, each bound to the (shared,
// read-only) atlas view/sampler and that slot's own palette buffer. All
// sets are created up front here, rather than lazily per-frame, since the
// descriptor pool has to be sized for the full frame count anyway.
static int ensure_sprite_descriptor_sets(uint32_t frameCount) {
    if (g_sprite_descriptor_pool != VK_NULL_HANDLE) { return 0; }

    if (ensure_sprite_atlas_view_and_sampler() != 0) { return -1; }

    VkDevice dev = gdmf_get_device();
    VkDescriptorPoolSize pool_sizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = frameCount },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         .descriptorCount = frameCount }
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 2,
        .pPoolSizes    = pool_sizes,
        .maxSets       = frameCount
    };
    if (vkCreateDescriptorPool(dev, &pool_info, NULL, &g_sprite_descriptor_pool) != VK_SUCCESS) {
        printf("[Sprites] Failed to create descriptor pool\n");
        return -1;
    }

    VkDescriptorSetLayout* layouts = malloc(frameCount * sizeof(VkDescriptorSetLayout));
    VkDescriptorSet*       sets    = malloc(frameCount * sizeof(VkDescriptorSet));
    if (!layouts || !sets) {
        printf("[Sprites] Failed to allocate descriptor set bookkeeping arrays\n");
        free(layouts);
        free(sets);
        return -1;
    }
    for (uint32_t i = 0; i < frameCount; i++) layouts[i] = g_sprite_descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = g_sprite_descriptor_pool,
        .descriptorSetCount = frameCount,
        .pSetLayouts        = layouts
    };
    VkResult alloc_result = vkAllocateDescriptorSets(dev, &alloc_info, sets);
    free(layouts);
    if (alloc_result != VK_SUCCESS) {
        printf("[Sprites] Failed to allocate descriptor sets\n");
        free(sets);
        return -1;
    }

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView   = g_sprite_atlas.atlas_view,
        .sampler     = g_sprite_atlas.atlas_sampler
    };

    for (uint32_t i = 0; i < frameCount; i++) {
        SpriteFrameResources* frame = &g_sprite_frames[i];

        if (ensure_sprite_palette_buffer(frame) != 0) {
            free(sets);
            return -1;
        }
        frame->descriptorSet = sets[i];

        VkDescriptorBufferInfo buffer_info = {
            .buffer = frame->paletteBuffer,
            .offset = 0,
            .range  = SPRITE_PALETTE_BUFFER_SIZE
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
                .pBufferInfo     = &buffer_info
            }
        };
        vkUpdateDescriptorSets(dev, 2, writes, 0, NULL);
    }

    free(sets);

    return 0;
}

// Vertex buffer (grow-only, same pattern as the text layer's). One per
// frame slot -- see SpriteFrameResources.
static int ensure_sprite_vertex_buffer(SpriteFrameResources* frame, uint32_t required_vertices) {
    if (frame->vertexBuffer != VK_NULL_HANDLE && required_vertices <= frame->vertexCapacity) { return 0; }

    VkDevice dev = gdmf_get_device();
    if (frame->vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        vkFreeMemory(dev, frame->vertexMemory, NULL);
        frame->vertexBuffer = VK_NULL_HANDLE;
        frame->vertexMemory = VK_NULL_HANDLE;
    }

    frame->vertexCapacity = required_vertices + 256;
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = frame->vertexCapacity * sizeof(SpriteVertex),
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &frame->vertexBuffer) != VK_SUCCESS) {
        printf("[Sprites] Failed to create vertex buffer\n");
        frame->vertexCapacity = 0;
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
        printf("[Sprites] Failed to allocate vertex buffer memory\n");
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        frame->vertexBuffer   = VK_NULL_HANDLE;
        frame->vertexCapacity = 0;
        return -1;
    }
    vkBindBufferMemory(dev, frame->vertexBuffer, frame->vertexMemory, 0);

    return 0;
}

// Pipeline. Created once from InitSprites (the atlas already exists by
// then); shader modules are temporary and destroyed right after.
static int ensure_sprite_pipeline(void) {
    if (g_sprite_pipeline_ready) { return 0; }

    VkDevice dev = gdmf_get_device();

    if (g_sprite_atlas.atlas_image == VK_NULL_HANDLE) { return -1; }

    uint32_t frameCount = gdmf_get_swapchain_image_count();
    if (frameCount == 0) { return -1; }
    if (g_sprite_frames == NULL) {
        g_sprite_frames = calloc(frameCount, sizeof(SpriteFrameResources));
        if (!g_sprite_frames) {
            printf("[Sprites] Failed to allocate per-frame resource array\n");
            return -1;
        }
        g_sprite_frame_count = frameCount;
    }

    if (ensure_sprite_descriptor_set_layout() != 0) { return -1; }
    if (ensure_sprite_descriptor_sets(g_sprite_frame_count) != 0) { return -1; }

    VkShaderModuleCreateInfo vert_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sprite_vert_spv_len,
        .pCode    = (const uint32_t*)sprite_vert_spv
    };
    VkShaderModuleCreateInfo frag_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sprite_frag_spv_len,
        .pCode    = (const uint32_t*)sprite_frag_spv
    };
    VkShaderModule vert_mod, frag_mod;
    if (vkCreateShaderModule(dev, &vert_ci, NULL, &vert_mod) != VK_SUCCESS) {
        printf("[Sprites] Failed to create vertex shader module\n");
        return -1;
    }
    if (vkCreateShaderModule(dev, &frag_ci, NULL, &frag_mod) != VK_SUCCESS) {
        printf("[Sprites] Failed to create fragment shader module\n");
        vkDestroyShaderModule(dev, vert_mod, NULL);
        return -1;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_mod, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_mod, .pName = "main" }
    };

    VkVertexInputBindingDescription binding = {
        .binding   = 0,
        .stride    = sizeof(SpriteVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attrs[7] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = (uint32_t)offsetof(SpriteVertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = (uint32_t)offsetof(SpriteVertex, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,    .offset = (uint32_t)offsetof(SpriteVertex, bitmapID) },
        { .location = 3, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,    .offset = (uint32_t)offsetof(SpriteVertex, palette) },
        { .location = 4, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,    .offset = (uint32_t)offsetof(SpriteVertex, transparency) },
        { .location = 5, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,    .offset = (uint32_t)offsetof(SpriteVertex, showzero) },
        { .location = 6, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,    .offset = (uint32_t)offsetof(SpriteVertex, rawGrayscale) }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 7,
        .pVertexAttributeDescriptions    = attrs
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn_state = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dyn_states
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

    VkPipelineLayoutCreateInfo layout_ci = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &g_sprite_descriptor_set_layout
    };
    if (vkCreatePipelineLayout(dev, &layout_ci, NULL, &g_sprite_vk_layout) != VK_SUCCESS) {
        printf("[Sprites] Failed to create pipeline layout\n");
        vkDestroyShaderModule(dev, vert_mod, NULL);
        vkDestroyShaderModule(dev, frag_mod, NULL);
        return -1;
    }

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dyn_state,
        .layout              = g_sprite_vk_layout,
        .renderPass          = gdmf_get_render_pass(),
        .subpass             = 0
    };
    VkResult result = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &g_sprite_vk_pipeline);

    vkDestroyShaderModule(dev, vert_mod, NULL);
    vkDestroyShaderModule(dev, frag_mod, NULL);

    if (result != VK_SUCCESS) {
        printf("[Sprites] Failed to create graphics pipeline\n");
        vkDestroyPipelineLayout(dev, g_sprite_vk_layout, NULL);
        g_sprite_vk_layout = VK_NULL_HANDLE;
        return -1;
    }

    g_sprite_pipeline_ready = true;
    printf("[Sprites] Pipeline ready\n");

    return 0;
}

static void cleanup_sprite_render_resources(void) {
    VkDevice dev = gdmf_get_device();

    if (dev == VK_NULL_HANDLE) { return; }
    vkDeviceWaitIdle(dev);

    for (uint32_t i = 0; i < g_sprite_frame_count; i++) {
        SpriteFrameResources* frame = &g_sprite_frames[i];

        if (frame->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
            frame->vertexBuffer = VK_NULL_HANDLE;
        }
        if (frame->vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->vertexMemory, NULL);
            frame->vertexMemory = VK_NULL_HANDLE;
        }
        if (frame->paletteBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->paletteBuffer, NULL);
            frame->paletteBuffer = VK_NULL_HANDLE;
        }
        if (frame->paletteMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->paletteMemory, NULL);
            frame->paletteMemory = VK_NULL_HANDLE;
        }
    }
    free(g_sprite_frames);
    g_sprite_frames      = NULL;
    g_sprite_frame_count = 0;

    if (g_sprite_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, g_sprite_vk_pipeline, NULL);
        g_sprite_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_sprite_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, g_sprite_vk_layout, NULL);
        g_sprite_vk_layout = VK_NULL_HANDLE;
    }
    if (g_sprite_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, g_sprite_descriptor_pool, NULL);
        g_sprite_descriptor_pool = VK_NULL_HANDLE;
    }
    if (g_sprite_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, g_sprite_descriptor_set_layout, NULL);
        g_sprite_descriptor_set_layout = VK_NULL_HANDLE;
    }
    g_sprite_pipeline_ready = false;

    return;
}

// Swapchain invalidation hook (called from gdmf_recreate_swapchain). The
// pipeline was built against the old render pass and the frame resource
// array was sized to the old swapchain image count -- either may now be
// stale, so tear both down and let the next gdmf_sprites_prepare() call
// lazily rebuild them via ensure_sprite_pipeline(). The atlas itself is
// swapchain-independent and is deliberately left untouched.
void gdmf_sprites_on_swapchain_recreated(void) {
    cleanup_sprite_render_resources();

    return;
}

// Shared by rendering and collision: a sprite's transformed quad corners
// in world/canvas space (shear -> rotate -> translate), before any NDC
// conversion. Corner order TL, TR, BL, BR -- matches the lx/ly layout
// below, so callers indexing into outX/outY agree with the render path.
static void ComputeSpriteWorldQuad(const Sprite* s, float outX[4], float outY[4]) {
    float halfW = (SPRITE_WIDTH  * s->scale) * 0.5f;
    float halfH = (SPRITE_HEIGHT * s->scale) * 0.5f;
    float centerX = s->x + halfW;
    float centerY = s->y + halfH;

    float angle = (float)(s->rotation * (M_PI / 180.0));
    float cosA = cosf(angle), sinA = sinf(angle);

    float lx[4] = { -halfW,  halfW, -halfW,  halfW };
    float ly[4] = { -halfH, -halfH,  halfH,  halfH };

    for (int c = 0; c < 4; c++) {
        float sx = lx[c] + s->skewX * ly[c];
        float sy = ly[c] + s->skewY * lx[c];
        float rx = cosA * sx - sinA * sy;
        float ry = sinA * sx + cosA * sy;

        outX[c] = centerX + rx;
        outY[c] = centerY + ry;
    }

    return;
}

// Prepare hook (called by gdmf_vulkan_render_frame before the render pass
// opens). Builds this frame's quad list from sprites that are both visible
// and enabled -- a disabled sprite never renders regardless of its visible
// flag, same as it never participates in collision (see RunSpriteCollisions
// below); visible alone only matters once a sprite is enabled. Sorted by
// priority ascending so lower-priority sprites draw first (further back) --
// a painter's-algorithm substitute for a depth buffer, which the render
// pass doesn't have.
//
// imageIndex selects which SpriteFrameResources slot to write into -- the
// caller's own command buffer for this image index has already had its
// fence waited on by this point, but a *different* image index's command
// buffer may still be executing on the GPU concurrently, so each image
// index must own its own vertex/palette buffers rather than share one.
void gdmf_sprites_prepare(uint32_t imageIndex) {
    RunSpriteCollisions();

    // Lazily (re)creates the pipeline/frame resources if they don't exist
    // yet, or were just torn down by gdmf_sprites_on_swapchain_recreated();
    // a cheap no-op (single flag check) once everything is already ready.
    if (ensure_sprite_pipeline() != 0) { return; }
    if (imageIndex >= g_sprite_frame_count) { return; }  // swapchain image count changed since pipeline creation

    SpriteFrameResources* frame = &g_sprite_frames[imageIndex];

    int drawCount = 0;
    for (int i = 0; i < MAX_SPRITES; i++) {
        Sprite* s = &sprites[i];

        if (!s->visible || !s->enabled) { continue; }
        if (!BitmapIDValid(s->bitmapID) || !spriteBitmapValid[s->bitmapID]) { continue; }
        g_sprite_draw_order[drawCount++] = i;
    }

    if (drawCount == 0) {
        g_sprite_active_this_frame = false;
        return;
    }

    // Stable insertion sort -- drawCount is small (<= MAX_SPRITES) and
    // frame-to-frame ordering rarely changes much, so this is cheap in
    // practice despite the O(n^2) worst case.
    for (int i = 1; i < drawCount; i++) {
        int key = g_sprite_draw_order[i];
        unsigned char keyPriority = sprites[key].priority;
        int j = i - 1;

        while (j >= 0 && sprites[g_sprite_draw_order[j]].priority > keyPriority) {
            g_sprite_draw_order[j + 1] = g_sprite_draw_order[j];
            j--;
        }
        g_sprite_draw_order[j + 1] = key;
    }

    if (ensure_sprite_vertex_buffer(frame, (uint32_t)drawCount * 6) != 0) {
        g_sprite_active_this_frame = false;
        return;
    }

    VkDevice dev = gdmf_get_device();

    SpriteVertex* vertices;
    if (vkMapMemory(dev, frame->vertexMemory, 0, VK_WHOLE_SIZE, 0, (void**)&vertices) != VK_SUCCESS) {
        printf("[Sprites] Failed to map vertex buffer\n");
        g_sprite_active_this_frame = false;
        return;
    }

    // Sprite coordinates are positions on a fixed reference canvas, not live
    // window pixels -- same convention the text layer uses (its 80x45 grid
    // of 16px cells is fractions of NDC, independent of the live extent).
    // The dynamic viewport in gdmf_sprites_record() stretches that canvas to
    // whatever the real window size is, so sprites scale with the window
    // exactly like text does instead of staying pinned to absolute pixels.
    float toNdcX = 2.0f / SPRITE_REFERENCE_CANVAS_WIDTH;
    float toNdcY = 2.0f / SPRITE_REFERENCE_CANVAS_HEIGHT;

    uint32_t vertex_index = 0;
    for (int k = 0; k < drawCount; k++) {
        Sprite* s = &sprites[g_sprite_draw_order[k]];

        // Corners in world/canvas space, indexed TL, TR, BL, BR.
        float worldX[4], worldY[4];

        ComputeSpriteWorldQuad(s, worldX, worldY);

        // Mirroring is just swapping which UV corner lands on which world
        // corner -- the world-space quad itself (and therefore the AABB
        // and any bounding-box collision) is completely unaffected. Pixel
        // collision applies the same flip bits when sampling the bitmap
        // (see WorldPixelToLocalBitmapPixel), so what's drawn and what's
        // hit-tested always agree.
        float uvx[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
        float uvy[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        if (s->flip & SPRITE_FLIP_X) { for (int c = 0; c < 4; c++) uvx[c] = 1.0f - uvx[c]; }
        if (s->flip & SPRITE_FLIP_Y) { for (int c = 0; c < 4; c++) uvy[c] = 1.0f - uvy[c]; }

        float ndcX[4], ndcY[4];
        for (int c = 0; c < 4; c++) {
            ndcX[c] = worldX[c] * toNdcX - 1.0f;
            ndcY[c] = worldY[c] * toNdcY - 1.0f;
        }

        float alpha    = s->transparency / 255.0f;
        float palette  = (float)s->palette;
        float bitmapf  = (float)s->bitmapID;
        float showzero = s->showzero ? 1.0f : 0.0f;

        // Atlas-debug-view sprites live in a reserved index range (see
        // ToggleSpriteAtlasView) and always bypass the palette entirely --
        // recognized by index alone, no per-sprite flag needed.
        float rawGrayscale = (g_sprite_draw_order[k] >= ATLAS_VIEW_SPRITE_BASE) ? 1.0f : 0.0f;

        static const int order[6] = { 0, 2, 1,  2, 3, 1 }; // TL,BL,TR, BL,BR,TR
        for (int v = 0; v < 6; v++) {
            int c = order[v];

            vertices[vertex_index++] = (SpriteVertex){
                { ndcX[c], ndcY[c] }, { uvx[c], uvy[c] }, bitmapf, palette, alpha, showzero, rawGrayscale
            };
        }
    }

    vkUnmapMemory(dev, frame->vertexMemory);

    void* palette_mapped;
    if (vkMapMemory(dev, frame->paletteMemory, 0, SPRITE_PALETTE_BUFFER_SIZE, 0, &palette_mapped) == VK_SUCCESS) {
        // Packed explicitly via PackRGBA8 rather than memcpy-ing Colors'
        // raw bytes -- the GPU-side layout is now an explicit, named
        // contract instead of an accident of Color's in-memory layout.
        uint32_t* packed = (uint32_t*)palette_mapped;

        for (int p = 0; p < 256; p++)
            for (int c = 0; c < FUSELAGE_PALETTE_SIZE; c++)
                packed[p * FUSELAGE_PALETTE_SIZE + c] = PackRGBA8(Colors[p][c]);
        vkUnmapMemory(dev, frame->paletteMemory);
    }

    g_sprite_draw_vertex_count = vertex_index;

    // Partition vertices into priority bands. Draw order is already sorted by
    // priority, so each band's sprites are contiguous in the vertex buffer.
    memset(g_sprite_band_first_vertex, 0, sizeof(g_sprite_band_first_vertex));
    memset(g_sprite_band_vertex_count, 0, sizeof(g_sprite_band_vertex_count));
    uint32_t bv = 0;
    for (int k = 0; k < drawCount; k++) {
        uint8_t b = sprites[g_sprite_draw_order[k]].priority >> 4;  // /16

        if (g_sprite_band_vertex_count[b] == 0) { g_sprite_band_first_vertex[b] = bv; }
        g_sprite_band_vertex_count[b] += 6;
        bv += 6;
    }

    g_sprite_active_this_frame = true;

    return;
}

// Render hook for one priority band (called from the interleaved render loop
// in gdmf_vulkan.c). Band N draws only sprites with priority in [N*16, N*16+15].
// Skips silently when the band is empty, so the caller need not check.
void gdmf_sprites_record_band(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t band) {
    if (!g_sprite_active_this_frame) { return; }
    if (imageIndex >= g_sprite_frame_count) { return; }
    if (band >= SPRITE_PRIORITY_BANDS) { return; }
    if (g_sprite_band_vertex_count[band] == 0) { return; }

    SpriteFrameResources* frame = &g_sprite_frames[imageIndex];

    VkRect2D render_rect = gdmf_get_render_viewport_rect();
    VkViewport viewport = {
        .x = (float)render_rect.offset.x, .y = (float)render_rect.offset.y,
        .width = (float)render_rect.extent.width, .height = (float)render_rect.extent.height,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    VkRect2D scissor = render_rect;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_sprite_vk_pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer     vertex_buffers[] = { frame->vertexBuffer };
    VkDeviceSize offsets[]        = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_sprite_vk_layout, 0, 1, &frame->descriptorSet, 0, NULL);
    vkCmdDraw(cmd, g_sprite_band_vertex_count[band], 1, g_sprite_band_first_vertex[band], 0);

    return;
}

// Initialize sprites
int InitSprites(void) {
    printf("Initializing all sprites...\n");
    Color old = tlGetColor();
    tlPrintFormatted("[Sprites] Version %s", GREEN, GDMF_SPRITES_VERSION);tlNewLine(); tlSetColor(old);
    int initcount = 0;

    for (int i = 0; i < MAX_SPRITES; i++) {
        sprites[i].x = 0.0f;
        sprites[i].y = 0.0f;
        sprites[i].scale = 1.0f;
        sprites[i].rotation = 0.0f;
        sprites[i].skewX = 0.0f;
        sprites[i].skewY = 0.0f;
        sprites[i].transparency = 255;
        sprites[i].priority = 0;
        sprites[i].palette = 0;
        sprites[i].bitmapID = SPRITE_BITMAP_NONE;
        sprites[i].enabled = false;
        sprites[i].visible = false;
        sprites[i].showzero = false;
        sprites[i].collidableColors = 0xFFFE;  // all colors collidable except background
        sprites[i].collisionTypes = COLLISION_TYPE_NONE;  // opt-in -- a sprite must explicitly request collision reporting
        sprites[i].flip = SPRITE_FLIP_NONE;

        initcount++;
    }

    memset(spriteBitmapValid, 0, sizeof(spriteBitmapValid));

    create_vulkan_sprite_atlas();

    // Checkerboard test pattern: a 4x4 grid of 16x16 cells, one of each of
    // the 16 palette indices.
    for (int y = 0; y < SPRITE_HEIGHT; ++y) {
        for (int x = 0; x < SPRITE_WIDTH; x += 2) {
            int gridX = x / 16;
            int gridY = y / 16;
            int colorIndex = gridY * 4 + gridX;
            unsigned char packedValue = (unsigned char)((colorIndex << 4) | (colorIndex & 0x0F));

            testsprite[(y * SPRITE_WIDTH + x) / 2] = packedValue;
        }
    }

    UploadSpriteBitmap(SPRITE_TEST_PATTERN_BITMAP_ID, testsprite);

    // Sprite 0 is a naming convention, not an engine-enforced behavior: by
    // agreement, games with mouse support use it as the cursor. It's
    // preassigned the test pattern here but stays disabled/invisible like
    // every other slot until a game opts in.
    sprites[0].bitmapID = 0;

    if (ensure_sprite_pipeline() != 0) {
        printf("[Sprites] Render pipeline unavailable -- sprites will not draw.\n");
    }

    printf("Initialized %d sprites.\n", initcount);

    return initcount;
}

void ShutdownSprites(void) {
    cleanup_sprite_render_resources();
    destroy_vulkan_sprite_atlas();

    return;
}

bool UploadSpriteBitmap(SpriteBitmapID bitmapID, const unsigned char* bitmap) {
    if (!BitmapIDValid(bitmapID)) {
        printf("UploadSpriteBitmap: bitmap ID %d is out of range.\n", bitmapID);
        return false;
    }
    if (!bitmap) {
        printf("UploadSpriteBitmap: bitmap data is NULL.\n");
        return false;
    }

    // Unconditional overwrite of the whole slot -- every byte of the region
    // is replaced, so nothing from a previous occupant can show through.
    memcpy(spriteBitmapData[bitmapID], bitmap, SPRITE_BITMAP_BYTES);
    spriteBitmapValid[bitmapID] = true;

    // GPU side: skip quietly if the atlas doesn't exist (Vulkan not up yet,
    // or atlas creation failed) -- the CPU mirror above is already correct
    // and is all collision detection needs.
    if (g_sprite_atlas.atlas_image == VK_NULL_HANDLE) { return true; }

    VkDevice dev = gdmf_get_device();

    // Atlas pixels are 1 byte/pixel (raw palette index), unlike the 2
    // pixels/byte source -- expand before staging.
    unsigned char* unpacked = malloc(SPRITE_WIDTH * SPRITE_HEIGHT);
    if (!unpacked) {
        printf("UploadSpriteBitmap: failed to allocate unpack buffer.\n");
        return true;
    }
    for (int i = 0; i < SPRITE_WIDTH * SPRITE_HEIGHT; i++) {
        unpacked[i] = (bitmap[i / 2] >> ((i % 2 == 0) ? 4 : 0)) & 0x0F;
    }

    VkBuffer       staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = SPRITE_WIDTH * SPRITE_HEIGHT,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &staging_buffer) != VK_SUCCESS) {
        printf("UploadSpriteBitmap: failed to create staging buffer.\n");
        free(unpacked);
        return true;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(dev, staging_buffer, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &staging_memory) != VK_SUCCESS) {
        printf("UploadSpriteBitmap: failed to allocate staging memory.\n");
        vkDestroyBuffer(dev, staging_buffer, NULL);
        free(unpacked);
        return true;
    }
    vkBindBufferMemory(dev, staging_buffer, staging_memory, 0);

    void* mapped;
    if (vkMapMemory(dev, staging_memory, 0, SPRITE_WIDTH * SPRITE_HEIGHT, 0, &mapped) != VK_SUCCESS) {
        printf("UploadSpriteBitmap: failed to map staging memory.\n");
        vkFreeMemory(dev, staging_memory, NULL);
        vkDestroyBuffer(dev, staging_buffer, NULL);
        free(unpacked);
        return true;
    }
    memcpy(mapped, unpacked, SPRITE_WIDTH * SPRITE_HEIGHT);
    vkUnmapMemory(dev, staging_memory);
    free(unpacked);

    SpriteAtlasUploadData upload_data = {
        .staging_buffer = staging_buffer,
        .atlas_image    = g_sprite_atlas.atlas_image,
        .bitmapID       = bitmapID
    };
    if (gdmfExecuteOneTimeCommands(record_sprite_bitmap_upload, &upload_data) != 0) {
        printf("UploadSpriteBitmap: failed to upload bitmap %d to the atlas.\n", bitmapID);
    }

    vkDestroyBuffer(dev, staging_buffer, NULL);
    vkFreeMemory(dev, staging_memory, NULL);

    return true;
}

bool AssignSprite(int spriteIndex, SpriteBitmapID bitmapID) {
    if (!SpriteIndexValid(spriteIndex)) {
        printf("AssignSprite: sprite %d is out of range.\n", spriteIndex);
        return false;
    }
    if (bitmapID != SPRITE_BITMAP_NONE && !BitmapIDValid(bitmapID)) {
        printf("AssignSprite: bitmap ID %d is out of range.\n", bitmapID);
        return false;
    }

    sprites[spriteIndex].bitmapID = bitmapID;

    return true;
}

bool AssignSpriteBitmapFromSprite(int spriteSource, int spriteDestination) {
    if (!SpriteIndexValid(spriteSource)) {
        printf("AssignSpriteBitmapFromSprite: source sprite %d is out of range.\n", spriteSource);
        return false;
    }
    if (!SpriteIndexValid(spriteDestination)) {
        printf("AssignSpriteBitmapFromSprite: destination sprite %d is out of range.\n", spriteDestination);
        return false;
    }

    sprites[spriteDestination].bitmapID = sprites[spriteSource].bitmapID;
    sprites[spriteDestination].showzero = sprites[spriteSource].showzero;

    return true;
}

SpriteBitmapID GetSpriteBitmapID(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return SPRITE_BITMAP_NONE; }

    return sprites[spriteIndex].bitmapID;
}

void ClearSprite(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return; }

    printf("Clearing sprite: %d\n", spriteIndex);
    SetSpriteEnabled(spriteIndex, false);
    AssignSprite(spriteIndex, SPRITE_BITMAP_NONE);

    return;
}

void SpriteTestPattern(int spriteIndex) {
    AssignSprite(spriteIndex, SPRITE_TEST_PATTERN_BITMAP_ID);

    return;
}

bool GetSpriteEnabled(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }

    return sprites[spriteIndex].enabled;
}

bool SetSpriteEnabled(int spriteIndex, bool enabled) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }
    sprites[spriteIndex].enabled = enabled;

    return sprites[spriteIndex].enabled;
}

bool ToggleSpriteEnabled(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }
    sprites[spriteIndex].enabled = !sprites[spriteIndex].enabled;

    return sprites[spriteIndex].enabled;
}

bool GetSpriteVisible(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }

    return sprites[spriteIndex].visible;
}

bool SetSpriteVisible(int spriteIndex, bool visible) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }
    sprites[spriteIndex].visible = visible;

    return sprites[spriteIndex].visible;
}

bool ToggleSpriteVisible(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }
    sprites[spriteIndex].visible = !sprites[spriteIndex].visible;

    return sprites[spriteIndex].visible;
}

void SetSpritePosition(int spriteIndex, float x, float y) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].x = x;
    sprites[spriteIndex].y = y;

    return;
}

void UpdateSpritePosition(int spriteIndex, float deltaX, float deltaY) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].x += deltaX;
    sprites[spriteIndex].y += deltaY;

    return;
}

float GetSpriteX(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0.0f; }

    return sprites[spriteIndex].x;
}

float GetSpriteY(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0.0f; }

    return sprites[spriteIndex].y;
}

void SetSpriteScale(int spriteIndex, float scale) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    // Collision (WorldPixelToLocalBitmapPixel) divides world-space deltas by
    // scale to recover local bitmap coordinates -- zero is a division by
    // zero, and negative isn't a feature anyone has signed off on (it'd
    // mirror the sprite by accident of arithmetic, with collision math never
    // verified against that case). Reject both rather than let either in.
    if (scale <= 0.0f) { return; }
    sprites[spriteIndex].scale = scale;

    return;
}

float GetSpriteScale(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0.0f; }

    return sprites[spriteIndex].scale;
}

float ChangeSpriteScale(int spriteIndex, float delta) {
    if (!SpriteIndexValid(spriteIndex)) { return 0.0f; }
    float newScale = sprites[spriteIndex].scale + delta;
    if (newScale <= 0.0f) { return sprites[spriteIndex].scale; }  // would be invalid -- leave scale unchanged
    sprites[spriteIndex].scale = newScale;

    return newScale;
}

void SetSpriteRotation(int spriteIndex, float rotation) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].rotation = rotation;

    return;
}

float GetSpriteRotation(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0.0f; }

    return sprites[spriteIndex].rotation;
}

float ChangeSpriteRotation(int spriteIndex, float delta) {
    if (!SpriteIndexValid(spriteIndex)) { return 0.0f; }
    sprites[spriteIndex].rotation += delta;

    return sprites[spriteIndex].rotation;
}

void SetSpriteSkew(int spriteIndex, float skewX, float skewY) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].skewX = skewX;
    sprites[spriteIndex].skewY = skewY;

    return;
}

void GetSpriteSkew(int spriteIndex, float* skewX, float* skewY) {
    float x = 0.0f, y = 0.0f;

    if (SpriteIndexValid(spriteIndex)) {
        x = sprites[spriteIndex].skewX;
        y = sprites[spriteIndex].skewY;
    }
    if (skewX) { *skewX = x; }
    if (skewY) { *skewY = y; }

    return;
}

unsigned char SetSpriteFlip(int spriteIndex, unsigned char flipMask) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }
    sprites[spriteIndex].flip = flipMask;

    return sprites[spriteIndex].flip;
}

unsigned char GetSpriteFlip(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return sprites[spriteIndex].flip;
}

void SetSpritePriority(int spriteIndex, unsigned char priority) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].priority = priority;

    return;
}

unsigned char GetSpritePriority(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return sprites[spriteIndex].priority;
}

bool SetSpriteColorPalette(int spriteIndex, unsigned char palette) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }
    sprites[spriteIndex].palette = palette;

    return true;
}

unsigned char GetSpriteColorPalette(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return sprites[spriteIndex].palette;
}

void SetSpriteTransparency(int spriteIndex, unsigned char transparency) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].transparency = transparency;

    return;
}

unsigned char GetSpriteTransparency(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return sprites[spriteIndex].transparency;
}

void SpriteShowZero(int spriteIndex, bool showzero) {
    if (!SpriteIndexValid(spriteIndex)) { return; }
    sprites[spriteIndex].showzero = showzero;

    return;
}

unsigned short SetSpriteCollidableColors(int spriteIndex, unsigned short mask) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }
    sprites[spriteIndex].collidableColors = mask;

    return sprites[spriteIndex].collidableColors;
}

unsigned short GetSpriteCollidableColors(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return sprites[spriteIndex].collidableColors;
}

unsigned char SetSpriteCollisionTypes(int spriteIndex, unsigned char typeMask) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }
    sprites[spriteIndex].collisionTypes = typeMask;

    return sprites[spriteIndex].collisionTypes;
}

unsigned char GetSpriteCollisionTypes(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return sprites[spriteIndex].collisionTypes;
}

bool SpriteHasCollision(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }

    return g_spriteCollisionCount[spriteIndex] > 0;
}

int GetSpriteCollisionCount(int spriteIndex) {
    if (!SpriteIndexValid(spriteIndex)) { return 0; }

    return g_spriteCollisionCount[spriteIndex];
}

const SpriteCollisionInfo* GetSpriteCollisions(int spriteIndex, int* outCount) {
    if (!SpriteIndexValid(spriteIndex)) {
        if (outCount) { *outCount = 0; }
        return NULL;
    }
    if (outCount) { *outCount = g_spriteCollisionCount[spriteIndex]; }

    return g_spriteCollisions[spriteIndex];
}

// Maps a world/canvas-space point into sprite s's local 64x64 bitmap pixel
// space, undoing exactly the forward chain ComputeSpriteWorldQuad applies
// (translate -> rotate -> shear) in reverse: untranslate, unrotate,
// unshear, unscale. Returns false if the point doesn't land on s's bitmap,
// or if s's shear is degenerate this frame (no valid inverse).
static bool WorldPixelToLocalBitmapPixel(const Sprite* s, float wx, float wy, int* outX, int* outY) {
    float halfW = (SPRITE_WIDTH  * s->scale) * 0.5f;
    float halfH = (SPRITE_HEIGHT * s->scale) * 0.5f;
    float centerX = s->x + halfW;
    float centerY = s->y + halfH;

    // Undo translate.
    float px = wx - centerX;
    float py = wy - centerY;

    // Undo rotate -- transpose of the forward rotation matrix.
    float angle = (float)(s->rotation * (M_PI / 180.0));
    float cosA = cosf(angle), sinA = sinf(angle);
    float sx = cosA * px + sinA * py;
    float sy = -sinA * px + cosA * py;

    // Undo shear. Forward shear matrix is [[1, skewX], [skewY, 1]];
    // det = 1 - skewX*skewY. Near-zero means this sprite's shear isn't
    // invertible this frame -- bail out rather than divide by ~zero.
    float det = 1.0f - s->skewX * s->skewY;

    if (fabsf(det) < 1e-6f) { return false; }
    float lx = (sx - s->skewX * sy) / det;
    float ly = (-s->skewY * sx + sy) / det;

    // Undo scale, recenter to bitmap-pixel origin.
    int localX = (int)(lx / s->scale + SPRITE_WIDTH  * 0.5f);
    int localY = (int)(ly / s->scale + SPRITE_HEIGHT * 0.5f);

    if (localX < 0 || localX >= SPRITE_WIDTH || localY < 0 || localY >= SPRITE_HEIGHT) { return false; }

    // Undo flip -- must mirror the same axes gdmf_sprites_prepare() mirrors
    // in UV space, or a flipped sprite would visually mirror but collide
    // against its un-mirrored silhouette.
    if (s->flip & SPRITE_FLIP_X) { localX = SPRITE_WIDTH  - 1 - localX; }
    if (s->flip & SPRITE_FLIP_Y) { localY = SPRITE_HEIGHT - 1 - localY; }

    *outX = localX;
    *outY = localY;

    return true;
}

// Public, geometry-only containment test: is world point (wx,wy) within
// sprite spriteIndex's actual transformed footprint (the rotated/skewed
// quad ComputeSpriteWorldQuad places it as), not just its axis-aligned
// bounding box? A rotated or skewed sprite's AABB is larger than its real
// footprint -- this catches the AABB's empty corners. Deliberately reuses
// WorldPixelToLocalBitmapPixel's bounds check (true scale/rotate/shear
// inverse + range check) rather than re-deriving it, and deliberately
// ignores its flip handling and color output: the bounds check happens
// before flip is applied and flip only remaps one valid index to another,
// so containment here never depends on flip (or any per-pixel sampling)
// being correct -- callers that need that independence (e.g. a click-test
// hit-testing collision-detection test sprites) get it for free.
bool WorldPointOnSprite(int spriteIndex, float worldX, float worldY) {
    if (!SpriteIndexValid(spriteIndex)) { return false; }
    int localX, localY;
    return WorldPixelToLocalBitmapPixel(&sprites[spriteIndex], worldX, worldY, &localX, &localY);
}

// Samples sprite s's bitmap at local pixel (localX, localY) using one
// consistent nibble convention everywhere in the collision system (even x
// -> high nibble, odd x -> low nibble) -- resolves the inherited mismatch
// between the old CheckPixelCollision and CheckRotatedPixelCollision.
static unsigned char SampleSpriteBitmapPixel(const Sprite* s, int localX, int localY) {
    const unsigned char* bitmap = spriteBitmapData[s->bitmapID];
    int byteIndex = (localY * SPRITE_WIDTH + localX) / 2;
    unsigned char packed = bitmap[byteIndex];

    return (localX % 2 == 0) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
}

static void RecordSpriteCollision(int spriteIndex, int otherSprite, SpriteCollisionType type) {
    int count = g_spriteCollisionCount[spriteIndex];

    if (count >= MAX_COLLISIONS_PER_SPRITE) { return; }  // silently saturate, no realloc/error
    g_spriteCollisions[spriteIndex][count].otherSprite = otherSprite;
    g_spriteCollisions[spriteIndex][count].type = type;
    g_spriteCollisionCount[spriteIndex] = count + 1;

    return;
}

// Pure pair test -- no side effects on the global per-frame result arrays.
// Tests sprite a against sprite b for exactly the types set in typesToTest
// (independent of either sprite's own collisionTypes field), given each
// sprite's already-computed world-space AABB. Returns a bitmask of which
// requested types actually matched (0 if none). Shared by the automatic
// per-frame pass (RunSpriteCollisions) and the explicit on-demand pair
// check (CheckSpritePairCollision) so the two can never disagree.
static unsigned char TestSpriteTypesAgainst(int a, int b, unsigned char typesToTest,
                                             const SpriteAABB* boxA, const SpriteAABB* boxB) {
    if (boxA->minX >= boxB->maxX || boxA->maxX <= boxB->minX ||
        boxA->minY >= boxB->maxY || boxA->maxY <= boxB->minY) {
        return 0;  // broad-phase reject -- no type is geometrically possible
    }

    unsigned char matched = 0;
    if (typesToTest & COLLISION_TYPE_BOUNDING_BOX) {
        matched |= COLLISION_TYPE_BOUNDING_BOX;
    }

    unsigned char pixelTypes = typesToTest & (COLLISION_TYPE_ANY_COLOR | COLLISION_TYPE_COLLIDABLE_COLORS);
    if (pixelTypes == 0) { return matched; }

    bool aHasBitmap = BitmapIDValid(sprites[a].bitmapID) && spriteBitmapValid[sprites[a].bitmapID];
    bool bHasBitmap = BitmapIDValid(sprites[b].bitmapID) && spriteBitmapValid[sprites[b].bitmapID];
    if (!aHasBitmap || !bHasBitmap) { return matched; }

    bool needAnyColor   = (pixelTypes & COLLISION_TYPE_ANY_COLOR) != 0;
    bool needCollidable = (pixelTypes & COLLISION_TYPE_COLLIDABLE_COLORS) != 0;

    int overlapMinX = (int)fmaxf(boxA->minX, boxB->minX);
    int overlapMinY = (int)fmaxf(boxA->minY, boxB->minY);
    int overlapMaxX = (int)fminf(boxA->maxX, boxB->maxX);
    int overlapMaxY = (int)fminf(boxA->maxY, boxB->maxY);

    for (int wy = overlapMinY; wy < overlapMaxY && (needAnyColor || needCollidable); wy++) {
        for (int wx = overlapMinX; wx < overlapMaxX && (needAnyColor || needCollidable); wx++) {
            int localXA, localYA, localXB, localYB;

            if (!WorldPixelToLocalBitmapPixel(&sprites[a], (float)wx, (float)wy, &localXA, &localYA)) { continue; }
            if (!WorldPixelToLocalBitmapPixel(&sprites[b], (float)wx, (float)wy, &localXB, &localYB)) { continue; }

            unsigned char colorA = SampleSpriteBitmapPixel(&sprites[a], localXA, localYA);
            unsigned char colorB = SampleSpriteBitmapPixel(&sprites[b], localXB, localYB);

            if (needAnyColor && colorA != 0 && colorB != 0) {
                matched |= COLLISION_TYPE_ANY_COLOR;
                needAnyColor = false;
            }

            if (needCollidable &&
                (sprites[a].collidableColors & (1 << colorA)) &&
                (sprites[b].collidableColors & (1 << colorB))) {
                matched |= COLLISION_TYPE_COLLIDABLE_COLORS;
                needCollidable = false;
            }
        }
    }

    return matched;
}

// Explicit, on-demand pair test -- independent of the automatic per-frame
// pass: ignores both sprites' `enabled` flag and `collisionTypes` field
// entirely (the caller specifies exactly what to test via typesToTest),
// and never touches the global per-frame result arrays, so it can't
// interfere with SpriteHasCollision/GetSpriteCollisions or be affected by
// them. Computes fresh world-space AABBs for just these two sprites rather
// than relying on the per-frame cache (which only covers enabled sprites).
// Returns a bitmask of which requested types matched (0 if none, or if
// either index is invalid).
unsigned char CheckSpritePairCollision(int spriteIndexA, int spriteIndexB, unsigned char typesToTest) {
    if (!SpriteIndexValid(spriteIndexA) || !SpriteIndexValid(spriteIndexB)) { return 0; }
    if (spriteIndexA == spriteIndexB) { return 0; }

    float qxA[4], qyA[4], qxB[4], qyB[4];
    ComputeSpriteWorldQuad(&sprites[spriteIndexA], qxA, qyA);
    ComputeSpriteWorldQuad(&sprites[spriteIndexB], qxB, qyB);

    SpriteAABB boxA, boxB;
    boxA.minX = fminf(fminf(qxA[0], qxA[1]), fminf(qxA[2], qxA[3]));
    boxA.maxX = fmaxf(fmaxf(qxA[0], qxA[1]), fmaxf(qxA[2], qxA[3]));
    boxA.minY = fminf(fminf(qyA[0], qyA[1]), fminf(qyA[2], qyA[3]));
    boxA.maxY = fmaxf(fmaxf(qyA[0], qyA[1]), fmaxf(qyA[2], qyA[3]));
    boxB.minX = fminf(fminf(qxB[0], qxB[1]), fminf(qxB[2], qxB[3]));
    boxB.maxX = fmaxf(fmaxf(qxB[0], qxB[1]), fmaxf(qxB[2], qxB[3]));
    boxB.minY = fminf(fminf(qyB[0], qyB[1]), fminf(qyB[2], qyB[3]));
    boxB.maxY = fmaxf(fmaxf(qyB[0], qyB[1]), fmaxf(qyB[2], qyB[3]));

    return TestSpriteTypesAgainst(spriteIndexA, spriteIndexB, typesToTest, &boxA, &boxB);
}

// Runs once per frame (from gdmf_sprites_prepare, before the render
// pipeline readiness check -- collision is pure CPU state and must not be
// skipped just because Vulkan resources aren't ready). Tests every enabled
// sprite that wants something reported (collisionTypes != 0) against every
// other enabled sprite, fully accounting for scale, rotation, and skew.
// Which types get recorded for a sprite depends only on that sprite's own
// collisionTypes mask -- a sprite can be flagged to detect a collision
// with another sprite that itself never opted into anything, or that cares
// about entirely different metrics.
static void RunSpriteCollisions(void) {
    for (int i = 0; i < MAX_SPRITES; i++) g_spriteCollisionCount[i] = 0;

    static int targets[MAX_SPRITES];
    int targetCount = 0;
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (!sprites[i].enabled) { continue; }

        float qx[4], qy[4];
        ComputeSpriteWorldQuad(&sprites[i], qx, qy);

        SpriteAABB* box = &g_spriteCollisionAABB[i];
        box->minX = fminf(fminf(qx[0], qx[1]), fminf(qx[2], qx[3]));
        box->maxX = fmaxf(fmaxf(qx[0], qx[1]), fmaxf(qx[2], qx[3]));
        box->minY = fminf(fminf(qy[0], qy[1]), fminf(qy[2], qy[3]));
        box->maxY = fmaxf(fmaxf(qy[0], qy[1]), fmaxf(qy[2], qy[3]));

        targets[targetCount++] = i;
    }

    for (int ri = 0; ri < targetCount; ri++) {
        int a = targets[ri];
        unsigned char typesA = sprites[a].collisionTypes;

        if (typesA == COLLISION_TYPE_NONE) { continue; }

        const SpriteAABB* boxA = &g_spriteCollisionAABB[a];

        for (int ti = 0; ti < targetCount; ti++) {
            int b = targets[ti];

            if (b == a) { continue; }

            unsigned char matched = TestSpriteTypesAgainst(a, b, typesA, boxA, &g_spriteCollisionAABB[b]);
            if (matched & COLLISION_TYPE_BOUNDING_BOX)      { RecordSpriteCollision(a, b, COLLISION_TYPE_BOUNDING_BOX); }
            if (matched & COLLISION_TYPE_ANY_COLOR)         { RecordSpriteCollision(a, b, COLLISION_TYPE_ANY_COLOR); }
            if (matched & COLLISION_TYPE_COLLIDABLE_COLORS) { RecordSpriteCollision(a, b, COLLISION_TYPE_COLLIDABLE_COLORS); }
        }
    }

    return;
}

// Atlas debug view. Reuses the normal sprite draw path entirely (one extra
// vertex attribute, no new pipeline) -- each visible atlas slot becomes an
// ordinary preview sprite, recognized by its reserved index range in
// gdmf_sprites_prepare() and rendered as a raw grayscale read of the atlas,
// bypassing the palette system entirely. The atlas stores no color of its
// own, so showing it grayscale is intrinsic to the draw, not dependent on
// any palette slot happening to hold the right values.
void ToggleSpriteAtlasView(void) {
    g_atlasViewActive = !g_atlasViewActive;

    if (!g_atlasViewActive) {
        for (int i = 0; i < ATLAS_VIEW_SPRITE_COUNT; i++) {
            SetSpriteVisible(ATLAS_VIEW_SPRITE_BASE + i, false);
        }
        return;
    }

    SpriteBitmapID validIDs[ATLAS_VIEW_SPRITE_COUNT];
    int validCount = 0;
    for (SpriteBitmapID id = 0; id < MAX_SPRITE_BITMAPS && validCount < ATLAS_VIEW_SPRITE_COUNT; id++) {
        if (spriteBitmapValid[id]) { validIDs[validCount++] = id; }
    }

    if (validCount == 0) {
        g_atlasViewActive = false;
        return;
    }

    // Grid sized to validCount, aspect-aware so cells start roughly square;
    // scale is whatever fits that grid into the reference canvas, capped at
    // ATLAS_VIEW_MAX_SCALE so a handful of bitmaps don't blow up huge.
    int cols = (int)ceilf(sqrtf((float)validCount *
        (SPRITE_REFERENCE_CANVAS_WIDTH / SPRITE_REFERENCE_CANVAS_HEIGHT)));
    if (cols < 1) { cols = 1; }
    int rows = (validCount + cols - 1) / cols;

    float cellWidth  = SPRITE_REFERENCE_CANVAS_WIDTH  / (float)cols;
    float cellHeight = SPRITE_REFERENCE_CANVAS_HEIGHT / (float)rows;
    float scale = fminf(cellWidth / SPRITE_WIDTH, cellHeight / SPRITE_HEIGHT);
    if (scale > ATLAS_VIEW_MAX_SCALE) { scale = ATLAS_VIEW_MAX_SCALE; }

    float gridWidth  = cols * SPRITE_WIDTH  * scale;
    float gridHeight = rows * SPRITE_HEIGHT * scale;
    float originX = (SPRITE_REFERENCE_CANVAS_WIDTH  - gridWidth)  / 2.0f;
    float originY = (SPRITE_REFERENCE_CANVAS_HEIGHT - gridHeight) / 2.0f;

    for (int i = 0; i < validCount; i++) {
        int spriteIndex = ATLAS_VIEW_SPRITE_BASE + i;
        int col = i % cols;
        int row = i / cols;

        AssignSprite(spriteIndex, validIDs[i]);
        SetSpriteScale(spriteIndex, scale);
        SetSpriteRotation(spriteIndex, 0.0f);
        SetSpriteSkew(spriteIndex, 0.0f, 0.0f);
        SetSpriteTransparency(spriteIndex, 255);
        SpriteShowZero(spriteIndex, true);  // a debug view should show every register, including 0
        SetSpriteEnabled(spriteIndex, false);  // preview only, no collision participation
        SetSpritePosition(spriteIndex,
            originX + (float)col * SPRITE_WIDTH  * scale,
            originY + (float)row * SPRITE_HEIGHT * scale);
        SetSpriteVisible(spriteIndex, true);
    }

    // Hide any leftover slots from a previous, larger view.
    for (int i = validCount; i < ATLAS_VIEW_SPRITE_COUNT; i++) {
        SetSpriteVisible(ATLAS_VIEW_SPRITE_BASE + i, false);
    }

    return;
}

bool GetSpriteAtlasViewActive(void) {
    return g_atlasViewActive;
}