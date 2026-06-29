#include "gdmf_textlayer.h"
#include "gdmf_vulkan_internal.h"
//#include "fuselage_log.h"
#include <string.h>
#include <stddef.h>

//  characterBitmap[256][64] -- packed 2-bit-per-pixel 16x16 glyphs.
//  Edit with Edibima; do not modify by hand.
#include "packedbitmaps.h"
#include "shaders/text_frag.h"
#include "shaders/text_vert.h"

// Text display state
// These are the authoritative CPU-side grid that all tl* functions write to.
// Nothing here should change until we refine text layer internals.

static bool           textLayerActiveStatus  = 1;
//static bool           textHasChanged         = 0;
static Color          cursorColor            = { 255, 255, 255, 255 };
//static Color          backgroundColor        = { 0, 0, 0, 0 };
static Color          textDisplayCellColor[TEXT_LAYER_WIDTH][TEXT_LAYER_HEIGHT];
static unsigned char  textDisplayCell[TEXT_LAYER_WIDTH][TEXT_LAYER_HEIGHT];
static unsigned short cursorPositionX        = 0;
static unsigned short cursorPositionY        = 0;
static unsigned short textHistoryBufferPosition = 0;
unsigned char  charCountIncWrap(void);

// Vulkan resource types
typedef struct {
    float pos[2];
    float character_id;
    float color[4];
} TextVertex;

typedef struct {
    VkImage        atlas_image;
    VkDeviceMemory atlas_memory;
    VkImageView    atlas_view;
    VkSampler      atlas_sampler;
} VulkanTextAtlas;

typedef struct {
    VkBuffer staging_buffer;
    VkImage  atlas_image;
    uint32_t atlas_size;
} AtlasUploadData;

// One vertex buffer per swapchain image. gdmf_vulkan_render_frame waits on
// a per-image fence before recording that image's command buffer, but that
// only guarantees the *previous* frame that used this same image index has
// finished -- a different image index's command buffer, submitted more
// recently, can still be executing on the GPU concurrently. Without
// per-image copies, gdmf_textlayer_prepare() would overwrite one shared
// vertex buffer that an in-flight frame's GPU work might still be reading
// (same hazard as the sprite layer's vertex/palette buffers). The atlas
// descriptor set itself stays single/shared -- the glyph atlas is static
// after creation, so there's nothing per-frame to race on there.
typedef struct {
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexMemory;
    uint32_t       vertexCapacity;  // vertices
} TextFrameResources;

static TextFrameResources* g_text_frames      = NULL;  // [g_text_frame_count]
static uint32_t            g_text_frame_count = 0;

// Vulkan resource statics
static VulkanTextAtlas       g_text_atlas                  = { 0 };
static VkDescriptorSetLayout g_text_descriptor_set_layout  = VK_NULL_HANDLE;
static VkDescriptorPool      g_text_descriptor_pool        = VK_NULL_HANDLE;
static VkDescriptorSet       g_text_descriptor_set         = VK_NULL_HANDLE;
static VkPipeline            g_text_vk_pipeline            = VK_NULL_HANDLE;
static VkPipelineLayout      g_text_vk_layout              = VK_NULL_HANDLE;
static bool                  g_text_pipeline_ready         = false;
static bool                  g_text_active_this_frame      = false;

// Forward declarations
static void record_atlas_upload(VkCommandBuffer cmd, void* user_data);
static void create_vulkan_atlas(void);
static void destroy_vulkan_atlas(void);
static int  ensure_atlas_view_and_sampler(void);
static int  create_text_descriptor_set_layout(void);
static int  create_text_descriptor_set(void);
static int  ensure_text_pipeline(void);
static int  ensure_text_vertex_buffer(TextFrameResources* frame, uint32_t required_vertices);

// Atlas upload (one-time command callback)
static void record_atlas_upload(VkCommandBuffer cmd_buffer, void* user_data) {
    AtlasUploadData* upload_data = (AtlasUploadData*)user_data;

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = upload_data->atlas_image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset       = { 0, 0, 0 },
        .imageExtent       = { 256, 256, 1 }
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


// Atlas creation
// Unpacks the 2-bit-per-pixel characterBitmap into a 256x256 RGBA texture
// (16 glyphs per row, 16 rows, each glyph 16x16 pixels).
static void create_vulkan_atlas(void) {
    VkDevice dev = gdmf_get_device();
    const uint32_t atlas_size = 256 * 256 * 4;
    unsigned char* atlas_data = malloc(atlas_size);

    if (!atlas_data) { return; }
    memset(atlas_data, 0, atlas_size);

    char unpackedValues[4] = { 0 };
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 64; j++) {
            for (int k = 0; k < 4; ++k)
                unpackedValues[k] = ((unsigned char)(characterBitmap[i][j]) >> (2 * k)) & 0x3;
            for (int k = 0; k < 4; k++) {
                int charX = (i % 16) * 16;
                int charY = (i / 16) * 16;
                int x = charX + (j * 4 + k) % 16;
                int y = charY + (j * 4 + k) / 16;
                uint32_t pixel_index = (uint32_t)(y * 256 + x) * 4;
                unsigned char grayscale_value;

                switch (unpackedValues[k]) {
                case 1:  grayscale_value = 255; atlas_data[pixel_index + 3] = 255; break;
                case 2:  grayscale_value = 85;  atlas_data[pixel_index + 3] = 255; break;
                case 3:  grayscale_value = 170; atlas_data[pixel_index + 3] = 255; break;
                default: grayscale_value = 0;   atlas_data[pixel_index + 3] = 0;   break;
                }
                atlas_data[pixel_index + 0] = grayscale_value;
                atlas_data[pixel_index + 1] = grayscale_value;
                atlas_data[pixel_index + 2] = grayscale_value;
            }
        }
    }

    int non_zero_pixels = 0;
    for (int i = 0; i < (int)atlas_size; i += 4){
        if (atlas_data[i + 3] != 0) {
            non_zero_pixels++;
        }
    }

    //FLOG("[Text Layer] Atlas has %d non-transparent pixels\n", non_zero_pixels);
    printf("[Text Layer] Atlas has %d non-transparent pixels\n", non_zero_pixels);
    tlPrintFormatted("[Text Layer] Atlas has %d non-transparent pixels", WHITE,
        non_zero_pixels);tlNewLine();

    VkImageCreateInfo image_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extent        = { 256, 256, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(dev, &image_info, NULL, &g_text_atlas.atlas_image) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create atlas image\n");
        tlPrint("[Text Layer] Failed to create atlas image");tlNewLine();

        free(atlas_data);
        return;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(dev, g_text_atlas.atlas_image, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &g_text_atlas.atlas_memory) != VK_SUCCESS) {
        printf("[Text Layer] Failed to allocate atlas memory\n");
        tlPrint("[Text Layer] Failed to allocate atlas memory");tlNewLine();

        vkDestroyImage(dev, g_text_atlas.atlas_image, NULL);
        g_text_atlas.atlas_image = VK_NULL_HANDLE;
        free(atlas_data);
        return;
    }
    vkBindImageMemory(dev, g_text_atlas.atlas_image, g_text_atlas.atlas_memory, 0);

    // Staging buffer
    VkBuffer       staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = atlas_size,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &staging_buffer) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create atlas staging buffer\n");
        tlPrint("[Text Layer] Failed to create atlas staging buffer");tlNewLine();

        vkDestroyImage(dev, g_text_atlas.atlas_image, NULL);
        vkFreeMemory(dev, g_text_atlas.atlas_memory, NULL);
        g_text_atlas.atlas_image  = VK_NULL_HANDLE;
        g_text_atlas.atlas_memory = VK_NULL_HANDLE;
        free(atlas_data);
        return;
    }
    VkMemoryRequirements buf_mem_req;
    vkGetBufferMemoryRequirements(dev, staging_buffer, &buf_mem_req);
    VkMemoryAllocateInfo buf_alloc = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = buf_mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(buf_mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (buf_alloc.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &buf_alloc, NULL, &staging_memory) != VK_SUCCESS) {
        printf("[Text Layer] Failed to allocate atlas staging memory\n");
        tlPrint("[Text Layer] Failed to allocate atlas staging memory");tlNewLine();

        vkDestroyBuffer(dev, staging_buffer, NULL);
        vkDestroyImage(dev, g_text_atlas.atlas_image, NULL);
        vkFreeMemory(dev, g_text_atlas.atlas_memory, NULL);
        g_text_atlas.atlas_image  = VK_NULL_HANDLE;
        g_text_atlas.atlas_memory = VK_NULL_HANDLE;
        free(atlas_data);
        return;
    }
    vkBindBufferMemory(dev, staging_buffer, staging_memory, 0);

    void* mapped;
    if (vkMapMemory(dev, staging_memory, 0, atlas_size, 0, &mapped) != VK_SUCCESS) {
        printf("[Text Layer] Failed to map atlas staging memory\n");
        tlPrint("[Text Layer] Failed to map atlas staging memory");tlNewLine();

        vkDestroyBuffer(dev, staging_buffer, NULL);
        vkFreeMemory(dev, staging_memory, NULL);
        vkDestroyImage(dev, g_text_atlas.atlas_image, NULL);
        vkFreeMemory(dev, g_text_atlas.atlas_memory, NULL);
        g_text_atlas.atlas_image  = VK_NULL_HANDLE;
        g_text_atlas.atlas_memory = VK_NULL_HANDLE;
        free(atlas_data);
        return;
    }
    memcpy(mapped, atlas_data, atlas_size);
    vkUnmapMemory(dev, staging_memory);
    free(atlas_data);

    AtlasUploadData upload_data = {
        .staging_buffer = staging_buffer,
        .atlas_image    = g_text_atlas.atlas_image,
        .atlas_size     = atlas_size
    };
    if (gdmfExecuteOneTimeCommands(record_atlas_upload, &upload_data) != 0) {
        printf("[Text Layer] Failed to upload atlas data\n");
        tlPrint("[Text Layer] Failed to upload atlas data");tlNewLine();

        vkDestroyBuffer(dev, staging_buffer, NULL);
        vkFreeMemory(dev, staging_memory, NULL);
        vkDestroyImage(dev, g_text_atlas.atlas_image, NULL);
        vkFreeMemory(dev, g_text_atlas.atlas_memory, NULL);
        g_text_atlas.atlas_image  = VK_NULL_HANDLE;
        g_text_atlas.atlas_memory = VK_NULL_HANDLE;
        return;
    }
    vkDestroyBuffer(dev, staging_buffer, NULL);
    vkFreeMemory(dev, staging_memory, NULL);

    //FLOG("[Text Layer] Character bitmap atlas created\n");
    printf("[Text Layer] Character bitmap atlas created\n");
    tlPrint("[Text Layer] Character bitmap atlas created");tlNewLine();

    return;
}

static void destroy_vulkan_atlas(void) {
    VkDevice dev = gdmf_get_device();

    if (g_text_atlas.atlas_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, g_text_atlas.atlas_sampler, NULL);
        g_text_atlas.atlas_sampler = VK_NULL_HANDLE;
    }
    if (g_text_atlas.atlas_view != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, g_text_atlas.atlas_view, NULL);
        g_text_atlas.atlas_view = VK_NULL_HANDLE;
    }
    if (g_text_atlas.atlas_image != VK_NULL_HANDLE) {
        vkDestroyImage(dev, g_text_atlas.atlas_image, NULL);
        g_text_atlas.atlas_image = VK_NULL_HANDLE;
    }
    if (g_text_atlas.atlas_memory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, g_text_atlas.atlas_memory, NULL);
        g_text_atlas.atlas_memory = VK_NULL_HANDLE;
    }

    return;
}

// Descriptor resources
static int ensure_atlas_view_and_sampler(void) {
    VkDevice dev = gdmf_get_device();

    if (g_text_atlas.atlas_view == VK_NULL_HANDLE) {
        VkImageViewCreateInfo view_info = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = g_text_atlas.atlas_image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R8G8B8A8_UNORM,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        if (vkCreateImageView(dev, &view_info, NULL, &g_text_atlas.atlas_view) != VK_SUCCESS) {
            printf("[Text Layer] Failed to create atlas image view\n");
            tlPrint("[Text Layer] Failed to create atlas image view");tlNewLine();

            return -1;
        }
    }
    if (g_text_atlas.atlas_sampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo samp_info = {
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_NEAREST,
            .minFilter    = VK_FILTER_NEAREST,
            .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxAnisotropy = 1.0f
        };

        if (vkCreateSampler(dev, &samp_info, NULL, &g_text_atlas.atlas_sampler) != VK_SUCCESS) {
            printf("[Text Layer] Failed to create atlas sampler\n");
            tlPrint("[Text Layer] Failed to create atlas sampler");tlNewLine();

            return -1;
        }
    }

    return 0;
}

static int create_text_descriptor_set_layout(void) {
    VkDescriptorSetLayoutBinding binding = {
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &binding
    };

    if (vkCreateDescriptorSetLayout(gdmf_get_device(), &layout_info, NULL,
            &g_text_descriptor_set_layout) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create descriptor set layout\n");
        tlPrint("[Text Layer] Failed to create descriptor set layout");tlNewLine();

        return -1;
    }

    return 0;
}

static int create_text_descriptor_set(void) {
    if (ensure_atlas_view_and_sampler() != 0) { return -1; }

    VkDevice dev = gdmf_get_device();
    VkDescriptorPoolSize pool_size = {
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes    = &pool_size,
        .maxSets       = 1
    };
    if (vkCreateDescriptorPool(dev, &pool_info, NULL, &g_text_descriptor_pool) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create descriptor pool\n");
        tlPrint("[Text Layer] Failed to create descriptor pool");tlNewLine();

        return -1;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = g_text_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &g_text_descriptor_set_layout
    };
    if (vkAllocateDescriptorSets(dev, &alloc_info, &g_text_descriptor_set) != VK_SUCCESS) {
        printf("[Text Layer] Failed to allocate descriptor set\n");
        tlPrint("[Text Layer] Failed to allocate descriptor set");tlNewLine();

        return -1;
    }

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView   = g_text_atlas.atlas_view,
        .sampler     = g_text_atlas.atlas_sampler
    };
    VkWriteDescriptorSet write = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = g_text_descriptor_set,
        .dstBinding      = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo      = &image_info
    };
    vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    return 0;
}

// Vertex buffer (grow-only). One per frame slot -- see TextFrameResources.
static int ensure_text_vertex_buffer(TextFrameResources* frame, uint32_t required_vertices) {
    if (frame->vertexBuffer != VK_NULL_HANDLE && required_vertices <= frame->vertexCapacity) { return 0; }

    VkDevice dev = gdmf_get_device();
    if (frame->vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        vkFreeMemory(dev, frame->vertexMemory, NULL);
        frame->vertexBuffer = VK_NULL_HANDLE;
        frame->vertexMemory = VK_NULL_HANDLE;
    }

    frame->vertexCapacity = required_vertices + 1024;
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = frame->vertexCapacity * sizeof(TextVertex),
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &frame->vertexBuffer) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create vertex buffer\n");
        tlPrint("[Text Layer] Failed to create vertex buffer");tlNewLine();

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
        printf("[Text Layer] Failed to allocate vertex buffer memory\n");
        tlPrint("[Text Layer] Failed to allocate vertex buffer memory");tlNewLine();

        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        frame->vertexBuffer   = VK_NULL_HANDLE;
        frame->vertexCapacity = 0;

        return -1;
    }
    vkBindBufferMemory(dev, frame->vertexBuffer, frame->vertexMemory, 0);

    return 0;
}

// Pipeline
// Shader modules are created here and immediately destroyed after pipeline
// creation. They are not needed afterwards.
static int ensure_text_pipeline(void) {
    if (g_text_pipeline_ready) { return 0; }

    VkDevice dev = gdmf_get_device();

    if (g_text_atlas.atlas_image == VK_NULL_HANDLE) { create_vulkan_atlas(); }
    if (g_text_atlas.atlas_image == VK_NULL_HANDLE) { return -1; }

    if (g_text_frames == NULL) {
        uint32_t frameCount = gdmf_get_swapchain_image_count();

        if (frameCount == 0) { return -1; }
        g_text_frames = calloc(frameCount, sizeof(TextFrameResources));
        if (!g_text_frames) {
            printf("[Text Layer] Failed to allocate per-frame resource array\n");
            return -1;
        }
        g_text_frame_count = frameCount;
    }

    if (g_text_descriptor_set_layout == VK_NULL_HANDLE) { if (create_text_descriptor_set_layout() != 0) { return -1; } }
    if (g_text_descriptor_set == VK_NULL_HANDLE) { if (create_text_descriptor_set() != 0) { return -1; } }

    VkShaderModuleCreateInfo vert_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = text_vert_spv_len,
        .pCode    = (const uint32_t*)text_vert_spv
    };
    VkShaderModuleCreateInfo frag_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = text_frag_spv_len,
        .pCode    = (const uint32_t*)text_frag_spv
    };
    VkShaderModule vert_mod, frag_mod;
    if (vkCreateShaderModule(dev, &vert_ci, NULL, &vert_mod) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create vertex shader module\n");
        tlPrint("[Text Layer] Failed to create vertex shader module");tlNewLine();

        return -1;
    }
    if (vkCreateShaderModule(dev, &frag_ci, NULL, &frag_mod) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create fragment shader module\n");
        tlPrint("[Text Layer] Failed to create fragment shader module");tlNewLine();

        vkDestroyShaderModule(dev, vert_mod, NULL);
        return -1;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_mod,
            .pName  = "main"
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_mod,
            .pName  = "main"
        }
    };

    VkVertexInputBindingDescription binding = {
        .binding   = 0,
        .stride    = sizeof(TextVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attrs[3] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = (uint32_t)offsetof(TextVertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,          .offset = (uint32_t)offsetof(TextVertex, character_id) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = (uint32_t)offsetof(TextVertex, color) }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 3,
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
        .pSetLayouts    = &g_text_descriptor_set_layout
    };
    if (vkCreatePipelineLayout(dev, &layout_ci, NULL, &g_text_vk_layout) != VK_SUCCESS) {
        printf("[Text Layer] Failed to create pipeline layout\n");
        tlPrint("[Text Layer] Failed to create pipeline layout");tlNewLine();

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
        .layout              = g_text_vk_layout,
        .renderPass          = gdmf_get_render_pass(),
        .subpass             = 0
    };
    VkResult result = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &g_text_vk_pipeline);

    vkDestroyShaderModule(dev, vert_mod, NULL);
    vkDestroyShaderModule(dev, frag_mod, NULL);

    if (result != VK_SUCCESS) {
        printf("[Text Layer] Failed to create graphics pipeline\n");
        tlPrint("[Text Layer] Failed to create graphics pipeline");tlNewLine();

        vkDestroyPipelineLayout(dev, g_text_vk_layout, NULL);
        g_text_vk_layout = VK_NULL_HANDLE;
        return -1;
    }

    g_text_pipeline_ready = true;

    //FLOG("[Text Layer] Pipeline ready\n");
    printf("[Text Layer] Pipeline ready\n");
    tlPrint("[Text Layer] Pipeline ready");tlNewLine();

    return 0;
}

// Cleanup
void cleanup_text_layer_resources(void) {
    VkDevice dev = gdmf_get_device();

    if (dev == VK_NULL_HANDLE) { return; }
    vkDeviceWaitIdle(dev);

    for (uint32_t i = 0; i < g_text_frame_count; i++) {
        TextFrameResources* frame = &g_text_frames[i];

        if (frame->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
            frame->vertexBuffer = VK_NULL_HANDLE;
        }
        if (frame->vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->vertexMemory, NULL);
            frame->vertexMemory = VK_NULL_HANDLE;
        }
    }
    free(g_text_frames);
    g_text_frames      = NULL;
    g_text_frame_count = 0;

    if (g_text_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, g_text_vk_pipeline, NULL);
        g_text_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_text_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, g_text_vk_layout, NULL);
        g_text_vk_layout = VK_NULL_HANDLE;
    }
    if (g_text_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, g_text_descriptor_pool, NULL);
        g_text_descriptor_pool = VK_NULL_HANDLE;
        g_text_descriptor_set  = VK_NULL_HANDLE;
    }
    if (g_text_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, g_text_descriptor_set_layout, NULL);
        g_text_descriptor_set_layout = VK_NULL_HANDLE;
    }
    g_text_pipeline_ready = false;

    return;
}

// Swapchain invalidation hook (called from gdmf_recreate_swapchain). The
// pipeline was built against the old render pass and the frame resource
// array was sized to the old swapchain image count -- either may now be
// stale, so tear both down and let the next gdmf_textlayer_prepare() call
// lazily rebuild them via ensure_text_pipeline(). The glyph atlas itself is
// swapchain-independent and is deliberately left untouched.
void gdmf_textlayer_on_swapchain_recreated(void) {
    cleanup_text_layer_resources();

    return;
}


// Prepare hook (called by gdmf_vulkan_render_frame before each render pass)
// Fills the vertex buffer with the current grid contents and sets the flag
// so gdmf_textlayer_record() will issue the draw command this frame.
//
// imageIndex selects which TextFrameResources slot to write into -- see the
// comment on TextFrameResources for why this can't be one shared buffer.
void gdmf_textlayer_prepare(uint32_t imageIndex) {
    if (!textLayerActiveStatus) { return; }

    if (ensure_text_pipeline() != 0) {
        printf("[Text Layer] Failed to ensure text pipeline\n");
        tlPrint("[Text Layer] Failed to ensure text pipeline");tlNewLine();

        return;
    }
    if (imageIndex >= g_text_frame_count) { return; }  // swapchain image count changed since pipeline creation

    TextFrameResources* frame = &g_text_frames[imageIndex];

    const uint32_t total_vertices = TEXT_LAYER_WIDTH * TEXT_LAYER_HEIGHT * 6;
    if (ensure_text_vertex_buffer(frame, total_vertices) != 0) {
        printf("[Text Layer] Failed to ensure vertex buffer\n");
        tlPrint("[Text Layer] Failed to ensure vertex buffer");tlNewLine();
        return;
    }

    TextVertex* vertices;
    if (vkMapMemory(gdmf_get_device(), frame->vertexMemory, 0, VK_WHOLE_SIZE, 0,
            (void**)&vertices) != VK_SUCCESS) {
        printf("[Text Layer] Failed to map vertex buffer\n");
        tlPrint("[Text Layer] Failed to map vertex buffer");tlNewLine();

        return;
    }

    uint32_t vertex_index = 0;
    float cell_width  = 2.0f / TEXT_LAYER_WIDTH;
    float cell_height = 2.0f / TEXT_LAYER_HEIGHT;

    for (int y = 0; y < TEXT_LAYER_HEIGHT; y++) {
        for (int x = 0; x < TEXT_LAYER_WIDTH; x++) {
            int flipped_y = TEXT_LAYER_HEIGHT - 1 - y;
            unsigned char character = textDisplayCell[x][flipped_y];
            Color cell_color = textDisplayCellColor[x][flipped_y];

            float left   = -1.0f + (x * cell_width);
            float right  = left + cell_width;
            float top    =  1.0f - (y * cell_height);
            float bottom = top - cell_height;

            float r = cell_color.r / 255.0f;
            float g = cell_color.g / 255.0f;
            float b = cell_color.b / 255.0f;
            float a = cell_color.a / 255.0f;

            vertices[vertex_index++] = (TextVertex){ {left,  top},    (float)character, {r, g, b, a} };
            vertices[vertex_index++] = (TextVertex){ {left,  bottom}, (float)character, {r, g, b, a} };
            vertices[vertex_index++] = (TextVertex){ {right, top},    (float)character, {r, g, b, a} };
            vertices[vertex_index++] = (TextVertex){ {left,  bottom}, (float)character, {r, g, b, a} };
            vertices[vertex_index++] = (TextVertex){ {right, bottom}, (float)character, {r, g, b, a} };
            vertices[vertex_index++] = (TextVertex){ {right, top},    (float)character, {r, g, b, a} };
        }
    }

    vkUnmapMemory(gdmf_get_device(), frame->vertexMemory);
    g_text_active_this_frame = true;

    return;
}

// Internal render hook (called from gdmf_vulkan_render_frame inside render pass).
// imageIndex must match the value gdmf_textlayer_prepare() was just called
// with this frame, so the vertex buffer bound here is the one just written.
void gdmf_textlayer_record(VkCommandBuffer cmd, uint32_t imageIndex) {
    if (!g_text_active_this_frame) { return; }
    g_text_active_this_frame = false;
    if (imageIndex >= g_text_frame_count) { return; }

    TextFrameResources* frame = &g_text_frames[imageIndex];

    // Letterboxed/pillarboxed sub-rect of the swapchain, not the full
    // extent -- keeps the reference canvas's aspect ratio intact when the
    // window/monitor doesn't match it (see gdmf_get_render_viewport_rect).
    VkRect2D render_rect = gdmf_get_render_viewport_rect();
    VkViewport viewport = {
        .x        = (float)render_rect.offset.x,
        .y        = (float)render_rect.offset.y,
        .width    = (float)render_rect.extent.width,
        .height   = (float)render_rect.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor = render_rect;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_text_vk_pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer     vertex_buffers[] = { frame->vertexBuffer };
    VkDeviceSize offsets[]        = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_text_vk_layout, 0, 1, &g_text_descriptor_set, 0, NULL);
    vkCmdDraw(cmd, TEXT_LAYER_WIDTH * TEXT_LAYER_HEIGHT * 6, 1, 0, 0);

    return;
}

// Setup and Shutdown
void SetupCharacterMaps(void) { // DEPRECATED
    return;
}

void ShutdownCharacterMaps(void) {
    cleanup_text_layer_resources();
    destroy_vulkan_atlas();

    //FLOG("[Text Layer] Character bitmaps shutdown\n");
    printf("[Text Layer] Character bitmaps shutdown\n");
    tlPrint("[Text Layer] Character bitmaps shutdown");tlNewLine();

    return;
}

void gdmf_textlayer_shutdown(void) {
    ShutdownCharacterMaps();

    return;
}

// Status
bool TextLayerStatus(void)  { return textLayerActiveStatus; }
bool TextLayerActive(void)  { textLayerActiveStatus = 1;

 return textLayerActiveStatus; }
bool TextLayerInactive(void){ textLayerActiveStatus = 0;

 return textLayerActiveStatus; }
bool TextLayerToggle(void)  { textLayerActiveStatus = !textLayerActiveStatus;

 return textLayerActiveStatus; }

// Debug helpers
void debug_print_text_grid(void) {
    printf("[Debug] Text grid contents:\n");
    for (int y = 0; y < 10; y++) {
        printf("Row %2d: ", y);
        for (int x = 0; x < 20; x++) {
            unsigned char ch = textDisplayCell[x][y];

            if (ch >= 32 && ch <= 126) { printf("%c", ch); }
            else { printf("[%d]", ch); }
        }
        printf("\n");
    }

    return;
}

void debug_cursor_and_text(const char* operation) {
    printf("[Debug] %s: cursor at (%d, %d)\n", operation, cursorPositionX, cursorPositionY);
    printf("[Debug] First 10 chars of row 0: ");
    for (int i = 0; i < 10; i++) printf("[%d]", textDisplayCell[i][0]);
    printf("\n");
    printf("[Debug] First 10 chars of row 1: ");
    for (int i = 0; i < 10; i++) printf("[%d]", textDisplayCell[i][1]);
    printf("\n");

    return;
}

void debug_buffer_contents(void) {
    printf("[Debug] debug_buffer_contents: not applicable in vertex-based text layer\n");

    return;
}

// ALL THE ORIGINAL FUNCTIONS BELOW - UNCHANGED
// This is the primary function of Layer 0. This is a grid based text overlay which can be enabled or disabled.
// packedbitmaps.h provides the typeface and can be edited with Edibima.
// Eventually all tlPrint functions will be rolled into one variadic function.
// Several overloads are provided to handle various argument arrangements.
// Formatted accepts arguments like printf. Does not accept Fuselage options other than color.
// Usage: tlPrintFormatted(const char*, (optional color), variadic arguments...);
int tlPrintFormatted(const char* format, ...) {
    if (!format) {
        return 0; // Gracefully handle null format
    }

    // Default to the current text layer state
    Color color = tlGetColor(); // Default color

    va_list args;
    va_start(args, format);

    // Peek at the first variadic argument
    va_list argsCopy;
    va_copy(argsCopy, args);

    // Initialize a flag for whether `Color` is provided
    int hasColor = 0;

    // Check if the first argument is a valid `Color`
    Color tempColor = va_arg(argsCopy, Color);
    if (tempColor.r >= 0 && tempColor.g >= 0 && tempColor.b >= 0 && tempColor.a >= 0) {
        hasColor = 1;  // Mark `Color` as present
        color = tempColor; // Use the provided color
    }
    va_end(argsCopy);

    // Advance the argument list if `Color` was provided
    if (hasColor) {
        (void)va_arg(args, Color);
    }

    // Buffer to hold the formatted string
    char buffer[256];

    // Format the string with the remaining arguments
    int neededSize = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Check for truncation
    if (neededSize >= (int)sizeof(buffer)) {
        fprintf(stderr, "Error: Formatted string exceeds buffer size of 256 characters.\n");
        return -1;
    }

    // Pass the formatted string and parameters to the core tlPrint function
    return tlPrintCP(buffer, color, cursorPositionX, cursorPositionY);
}

int tlPrintCP(const char* print, Color color, int currentPrintX, int currentPrintY) {

    int outputcount = 0;

    cursorColor = color;
    for (int i = 0; print[i] != '\0'; i++) {
        if (currentPrintX >= TEXT_LAYER_WIDTH) {
            currentPrintX = 0;
            currentPrintY++;
        }
        if (currentPrintY > TEXT_LAYER_HEIGHT - 1) {
            tlScrollUp();
            currentPrintY = TEXT_LAYER_HEIGHT - 1;
        }
        PlaceCharacterAtCell(currentPrintX, currentPrintY, print[i], color);
        outputcount++;
        currentPrintX++;
    }
    cursorPositionX = currentPrintX;
    cursorPositionY = currentPrintY;

    return outputcount;
}

int tlPrintC(const char* print, Color color) {

    return tlPrintCP(print, color, cursorPositionX, cursorPositionY);
}

int tlPrint(const char* print) {

    Color color = cursorColor;

    return tlPrintCP(print, color, cursorPositionX, cursorPositionY);
}

// Integer based calls to standard tlPrint will need to have ASCII values added to them in order to return the expected character results.
// 32 = Whitespace : 48 = Zero : 65 = A
// This will be handled automatically once the function has been made variadic.
int tlPrintCharCP(unsigned char input, Color color, int currentPrintX, int currentPrintY) {
    int outputcount = 0;

    cursorColor = color;
    if (currentPrintX >= TEXT_LAYER_WIDTH) {
        currentPrintX = 0;
        currentPrintY++;
    }
    if (currentPrintY > TEXT_LAYER_HEIGHT - 1) {
        tlScrollUp();
        currentPrintY = TEXT_LAYER_HEIGHT - 1;
    }
    PlaceCharacterAtCell(currentPrintX, currentPrintY, input, color);
    outputcount++;
    currentPrintX++;

    cursorPositionX = currentPrintX;
    cursorPositionY = currentPrintY;

    return outputcount;
}

int tlPrintCharC(unsigned char input, Color color) {

    return tlPrintCharCP(input, color, cursorPositionX, cursorPositionY);
}

int tlPrintChar(unsigned char input) {

    return tlPrintCharCP(input, cursorColor, cursorPositionX, cursorPositionY);
}

// Integer based Print Function converts Int type variables to Strings for Printing.
int tlPrintIntCP(int input, Color color, int currentPrintX, int currentPrintY) {

    char output[16] = "";
    int temp = input;
    int count = 1;

    for (long int i = 10; i <= input; i = i * 10) {
        count++;
    }

    for (int i = count - 1; i >= 0; i--) {
        if (count < 15) { output[i] = (temp % 10) + 48; }
        temp = temp / 10;
    }

    output[count] = '\0';

    return tlPrintCP((char*)output, color, currentPrintX, currentPrintY);
}

int tlPrintIntC(int input, Color color) {

    return tlPrintIntCP(input, color, cursorPositionX, cursorPositionY);
}

int tlPrintInt(int input) {

    return tlPrintIntCP(input, cursorColor, cursorPositionX, cursorPositionY);
}

// Clears Screen and sets cursor color.
void tlCLS(void) {
    Color color = BLACK;

    for (int y = 0; y < TEXT_LAYER_HEIGHT; y++)
        for (int x = 0; x < TEXT_LAYER_WIDTH; x++) {
            {
                if (y < 45) {
                    textDisplayCell[x][y] = 32;
                    textDisplayCellColor[x][y] = color;
                }
            }
        }
    cursorColor = color;
    tlHome();

    return;
}

// Sets cursor position to X0:Y0.
void tlHome(void) {
    cursorPositionX = 0;
    cursorPositionY = 0;

    return;
}

// Sets cursor color.
void tlSetColor(Color color) {
    cursorColor = color;

    return;
}
// Gets cursor color.
Color tlGetColor(void) {
    return cursorColor;
}

// Sets cursor location.
void tlSetCursor(int x, int y) {
    cursorPositionX = x;
    cursorPositionY = y;

    return;
}

// Gets cursor location 0-3599
int tlGetCursor(void) {
    int x = cursorPositionX;
    int y = cursorPositionY;
    return y * TEXT_LAYER_WIDTH + x;
}

unsigned short tlGetCursorX(void) {
    return cursorPositionX;
}

unsigned short tlGetCursorY(void) {
    return cursorPositionY;
}

// Scrolls screen up and stores contents of top line in history buffer.
// History buffer currently unused.
unsigned short tlScrollUp(void) {
    for (int y = 0; y < TEXT_LAYER_HEIGHT - 1; ++y) {
        for (int x = 0; x < TEXT_LAYER_WIDTH; ++x) {
            textDisplayCell[x][y] = textDisplayCell[x][y + 1];
            textDisplayCellColor[x][y] = textDisplayCellColor[x][y + 1];
        }
    }

    for (int i = 0; i < TEXT_LAYER_WIDTH; ++i) {
        textDisplayCell[i][TEXT_LAYER_HEIGHT - 1] = 0;
        textDisplayCellColor[i][TEXT_LAYER_HEIGHT - 1] = BLANK;
    }

    return textHistoryBufferPosition;
}

// Returns cursor to position X0 and linefeeds cursor down to next line.
int tlNewLine(void) {
    cursorPositionX = 0;
    cursorPositionY++;
    if (cursorPositionY > TEXT_LAYER_HEIGHT - 1) {
        tlScrollUp();
        cursorPositionY = TEXT_LAYER_HEIGHT - 1;
    }

    return cursorPositionY;
}

// Places Character value C inside Layer 0 at X/Y location.
void PlaceCharacterAtCell(unsigned short x, unsigned short y, unsigned char c, Color color) {
    textDisplayCell[x][y] = c;
    textDisplayCellColor[x][y] = color;

    return;
}

// For testing - cycles through letters
unsigned char charCountIncWrap(void) {

    static unsigned char count = 0;

    if ((count < 32) || (count > 127)) { count = 65; }
    else { count++; }

    return count;
}