// GDMF Pixies - BUTTOCKS pixie subsystem implementation.
// See gdmf_pixies.h for the public API contract and pixie_plans.txt for the
// full design abstract.
//
// STATE: lifecycle (Init/Shutdown/Write/ReadString), the CPU-only opcodes
// (SET_ATTR, PLOT, CLEAR, DRAW, SHOW, HIDE) in both PIXIE_MODE_TEXTURE and
// PIXIE_MODE_LIVE, the ergonomic wrappers, and the full Vulkan-side
// rendering path for both modes (Mode 0: per-pixie image + descriptor
// set + dirty upload + textured quad pipeline; Mode 1: per-primitive
// vertex accumulation + a separate plain-colored-vertex pipeline, no
// descriptor sets at all) are all real. UNPACK and EXECUTE remain stubs
// by design (UNPACK's format/tooling is a separate discussion; EXECUTE is
// the future VPU bridge, permanently a no-op in every mode so far).
//
// gdmf_pixies_on_swapchain_recreated tears down each mode's pipeline and
// per-image vertex buffers (the only things actually tied to the render
// pass / swapchain image count) and lazily rebuilds them on the next
// prepare() call -- see cleanup_pixie_swapchain_dependent_resources for
// why this is narrower than gdmf_sprites.c's equivalent (which reuses its
// full shutdown cleanup; pixies can't, since each pixie's descriptor set
// is its own non-shared resource, not a per-frame one).

#include "gdmf_pixies.h"
#include "gdmf_vulkan_internal.h"
#include "colors.h"
#include "shaders/pixie_vert.h"
#include "shaders/pixie_frag.h"
#include "shaders/pixie_live_vert.h"
#include "shaders/pixie_live_frag.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// Cap on Mode 1 primitives (PLOT/DRAW/CLEAR-with-color, each one quad or
// rotated rectangle = 6 vertices) accumulated per pixie per frame. Not a
// spec limit -- just a sane ceiling so a runaway per-frame command count
// can't grow the live vertex buffer unbounded.
#define PIXIE_LIVE_MAX_PRIMITIVES 4096
#define PIXIE_LIVE_MAX_VERTICES (PIXIE_LIVE_MAX_PRIMITIVES * 6)

// Mode 1 ("live") vertex -- plain position + color, no UV/texture at all,
// since there's nothing to sample. NDC position and RGBA (0..1) color are
// computed immediately when PixieCommand builds each primitive, not
// deferred to prepare() -- nothing about the transform depends on when
// between command-issue and prepare() it happens, since the reference
// canvas is fixed. Defined here (rather than alongside the other Vulkan-
// facing vertex types further down) because Pixie itself needs it below.
typedef struct {
    float pos[2];
    float color[4];
} PixieLiveVertex;

typedef struct {
    bool     initialized;
    PixieMode mode;

    unsigned char* ram;      // PIXIE_RAM_SIZE bytes, zeroed at InitPixie
    Color*         output;   // Mode 0 only: outputW * outputH RGBA8, zeroed at InitPixie. NULL in Mode 1.
    int            outputW;  // logical coordinate space PLOT/DRAW/CLEAR are expressed in, both modes
    int            outputH;

    int           x, y;      // display position
    int           w, h;      // display size (may differ from output dims -> scale)
    unsigned char priority;
    bool          enabled;
    bool          shown;
    bool          dirty;     // Mode 0 only: set whenever output changes, cleared once uploaded

    // Mode 1 only: this frame's accumulated primitives (each PLOT/DRAW/
    // CLEAR-with-color appends one quad/rectangle, 6 vertices). Built by
    // PixieCommand as calls arrive, copied to the GPU in prepare(), then
    // reset to empty -- nothing survives to the next frame. NULL/0 in
    // Mode 0.
    PixieLiveVertex* liveVertices;
    int              liveVertexCount;
    int              liveVertexCapacity;

    char output_string[PIXIE_OUTPUT_STRING_LEN];
} Pixie;

static Pixie g_pixies[MAX_PIXIES];

// GPU-side per-pixie resources, indexed the same way as g_pixies[] --
// mirrors how gdmf_tiles.c keeps g_tile_atlas[layer] separate from
// tilemaps[layer] rather than nesting Vulkan handles into the state
// struct itself. Unlike sprites/tiles sharing one atlas, a pixie's output
// buffer *is* its own distinct texture, so each pixie needs its own
// image/view/descriptor set rather than a slot in something bigger.
// Created lazily on the first gdmf_pixies_prepare() call after InitPixie;
// ready stays false until then. All zeroed/false via static init below --
// nothing allocates a real Vulkan object yet.
typedef struct {
    bool            ready;
    VkImage         image;
    VkDeviceMemory  imageMemory;
    VkImageView     imageView;
    VkDescriptorSet descriptorSet;
} PixieGPUResources;

static PixieGPUResources g_pixie_gpu[MAX_PIXIES];

// Shared across all pixies -- one sampler/layout/pipeline serves every
// pixie's descriptor set, same as sprites share one pipeline across every
// sprite instance. Nothing creates these yet.
static VkSampler             g_pixie_sampler               = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_pixie_descriptor_set_layout = VK_NULL_HANDLE;
static VkDescriptorPool      g_pixie_descriptor_pool       = VK_NULL_HANDLE;
static VkPipelineLayout      g_pixie_vk_layout             = VK_NULL_HANDLE;
static VkPipeline            g_pixie_vk_pipeline           = VK_NULL_HANDLE;
static bool                  g_pixie_pipeline_ready        = false;

// Mode 1's pipeline -- no sampler/descriptor set layout/pool at all,
// unlike Mode 0's: there's nothing to sample, just plain colored
// vertices. Shared by every Mode 1 pixie the same way g_pixie_vk_pipeline
// is shared by every Mode 0 pixie.
static VkPipelineLayout      g_pixie_live_vk_layout        = VK_NULL_HANDLE;
static VkPipeline            g_pixie_live_vk_pipeline      = VK_NULL_HANDLE;
static bool                  g_pixie_live_pipeline_ready   = false;

// Per-vertex draw data -- position/UV only, recomputed fresh every frame
// from each pixie's current x/y/w/h (same philosophy as sprites resolving
// their transform on the CPU every frame, no dirty-tracking needed since
// it's cheap). No per-vertex color/palette/transparency like sprites/tiles
// carry -- a pixie's own output buffer already is the final color.
typedef struct {
    float pos[2];
    float uv[2];
} PixieVertex;

// One vertex buffer per swapchain image, same reasoning as
// SpriteFrameResources in gdmf_sprites.c: a previous frame's command
// buffer (a different image index) may still be executing on the GPU and
// reading its own copy while this one is being rebuilt for the current
// frame.
typedef struct {
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexMemory;
    uint32_t       vertexCapacity;  // vertices
} PixieFrameResources;

// Same per-swapchain-image reasoning as PixieFrameResources, but grow-only
// like sprites'/tiles' vertex buffers rather than fixed-size like
// PixieFrameResources -- Mode 1's total vertex count (summed across every
// live pixie's accumulated primitives) genuinely varies frame to frame,
// unlike Mode 0's fixed MAX_PIXIES * 6.
typedef struct {
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexMemory;
    uint32_t       vertexCapacity;  // vertices
} PixieLiveFrameResources;

static PixieLiveFrameResources* g_pixie_live_frames      = NULL;  // [g_pixie_live_frame_count]
static uint32_t                 g_pixie_live_frame_count = 0;

static PixieFrameResources* g_pixie_frames      = NULL;  // [g_pixie_frame_count]
static uint32_t             g_pixie_frame_count = 0;

// Matches the shared priority-band system tiles/sprites interleave with in
// gdmf_vulkan.c's render loop (band = priority >> 4; band N covers
// priorities [N*16, N*16+15]). Unlike gdmf_sprites.c's band-aggregated
// first_vertex/vertex_count arrays, tracking here is per-PIXIE: sprites
// can batch a whole band into one draw call because every sprite shares
// one descriptor set (the atlas), but each pixie has its own descriptor
// set (its own image), so every pixie needs its own bind+draw regardless
// of band. gdmf_pixies_record_band filters by each pixie's own priority
// directly rather than drawing a precomputed contiguous band slice.
#define PIXIE_PRIORITY_BANDS 16
static uint32_t g_pixie_vertex_offset[MAX_PIXIES];      // this frame's vertex-buffer offset, if drawn
static bool     g_pixie_drawn_this_frame[MAX_PIXIES];   // valid only when true
static uint32_t g_pixie_live_vertex_count[MAX_PIXIES];  // Mode 1 only -- Mode 0 always draws exactly 6

static bool pixie_id_valid(int id) {
    return id >= 0 && id < MAX_PIXIES;
}

static bool pixie_ready(int id) {
    return pixie_id_valid(id) && g_pixies[id].initialized;
}

// Same byte order as colors.c's PackRGBA8 (r low byte .. a high byte), so a
// packed color built with PackRGBA8 on the C side unpacks back correctly
// here, and vice versa.
static Color pixie_unpack_color(uint32_t packed) {
    Color c;

    c.r = (unsigned char)(packed & 0xFFu);
    c.g = (unsigned char)((packed >> 8) & 0xFFu);
    c.b = (unsigned char)((packed >> 16) & 0xFFu);
    c.a = (unsigned char)((packed >> 24) & 0xFFu);

    return c;
}

// Packs a 2D point into one arg slot: high 16 bits = x, low 16 bits = y,
// each sign-extended back out as int16_t. Signed (not just unsigned) on
// purpose -- a coordinate a few pixels off-canvas is a normal thing to
// pass to a clipping primitive like DRAW, not an error. Output buffers
// max out at 1280x720, comfortably inside int16_t range either way.
static void pixie_unpack_xy(uint32_t packed, int* x, int* y) {
    *x = (int16_t)((packed >> 16) & 0xFFFFu);
    *y = (int16_t)(packed & 0xFFFFu);

    return;
}

// Same reference-canvas convention as sprites/tiles (SPRITE_REFERENCE_CANVAS_*/
// TILE_REFERENCE_CANVAS_* in their respective files) -- a coordinate means
// the same place in every layer, and the dynamic viewport in record_band
// stretches this fixed canvas to whatever the real window size is. Shared
// by Mode 0's quad emission (further down) and Mode 1's opcode bodies
// below, since both ultimately need world-space -> NDC.
#define PIXIE_REFERENCE_CANVAS_WIDTH  1280.0f
#define PIXIE_REFERENCE_CANVAS_HEIGHT 720.0f

static float pixie_world_to_ndc_x(float worldX) {
    return (worldX / PIXIE_REFERENCE_CANVAS_WIDTH) * 2.0f - 1.0f;
}
static float pixie_world_to_ndc_y(float worldY) {
    return (worldY / PIXIE_REFERENCE_CANVAS_HEIGHT) * 2.0f - 1.0f;
}

// Converts one local pixie-space point (lx, ly, both within [0, outputW)/
// [0, outputH)) into NDC -- local space -> this pixie's display rect
// (x/y/w/h) -> the shared reference canvas -> NDC. Used only by Mode 1;
// Mode 0 never needs this, since it always draws exactly one quad
// spanning its whole display rect and lets the GPU sampler do the
// stretching instead.
static void pixie_local_to_ndc(const Pixie* p, float lx, float ly, float* ndcX, float* ndcY) {
    float worldX = (float)p->x + (lx / (float)p->outputW) * (float)p->w;
    float worldY = (float)p->y + (ly / (float)p->outputH) * (float)p->h;

    *ndcX = pixie_world_to_ndc_x(worldX);
    *ndcY = pixie_world_to_ndc_y(worldY);

    return;
}

// Appends one quad (2 triangles, 6 vertices) to a Mode 1 pixie's this-
// frame accumulator, centered on local point (cx, cy) with the given
// local-space half-width/half-height. Silently drops the primitive if the
// cap is reached rather than overflowing -- PIXIE_LIVE_MAX_PRIMITIVES is a
// safety ceiling, not a spec limit, so a dropped primitive under extreme
// load is preferable to corrupting memory.
static void pixie_live_push_quad(Pixie* p, float cx, float cy, float halfW, float halfH, Color color) {
    if (p->liveVertexCount + 6 > p->liveVertexCapacity) {
        return;
    }

    float col[4] = {
        (float)color.r / 255.0f, (float)color.g / 255.0f,
        (float)color.b / 255.0f, (float)color.a / 255.0f
    };

    float corners[4][2] = {
        { cx - halfW, cy - halfH }, { cx + halfW, cy - halfH },
        { cx - halfW, cy + halfH }, { cx + halfW, cy + halfH }
    };
    float ndc[4][2];
    for (int c = 0; c < 4; c++) {
        pixie_local_to_ndc(p, corners[c][0], corners[c][1], &ndc[c][0], &ndc[c][1]);
    }

    static const int order[6] = { 0, 1, 2,  1, 3, 2 };  // TL,TR,BL, TR,BR,BL
    PixieLiveVertex* v = &p->liveVertices[p->liveVertexCount];
    for (int i = 0; i < 6; i++) {
        int c = order[i];

        v[i].pos[0] = ndc[c][0];
        v[i].pos[1] = ndc[c][1];
        memcpy(v[i].color, col, sizeof(col));
    }
    p->liveVertexCount += 6;

    return;
}

// Appends one rotated rectangle (2 triangles, 6 vertices) representing a
// thick line from local point (x0,y0) to (x1,y1) with the given local-
// space width. Unlike Mode 0's pixie_draw_line (a per-pixel Bresenham
// walk with a stamp at each step -- cheap for a CPU buffer write, but
// would mean one GPU quad per pixel here), this is the whole line as a
// single primitive regardless of length -- the actual reason Mode 1 is
// worth having instead of just always using Mode 0.
static void pixie_live_push_line(Pixie* p, float x0, float y0, float x1, float y1, float width, Color color) {
    // Zero-length "line" -- the trick DriftingPlots-style callers use to
    // stamp a single point via DRAW (identical start/end point). A
    // rotated rectangle only offsets *perpendicular* to the line
    // direction; with both endpoints coincident there is no direction and
    // no along-the-line extent either, so the math below would collapse
    // to a zero-area sliver, not a square. Delegate to the quad helper
    // instead, which is what a "point" actually needs.
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.0001f) {
        float half = width / 2.0f;

        pixie_live_push_quad(p, x0, y0, half, half, color);
        return;
    }

    if (p->liveVertexCount + 6 > p->liveVertexCapacity) {
        return;
    }

    float nx = -dy / len, ny = dx / len;  // unit normal, perpendicular to the line direction
    float hw = width / 2.0f;
    float ox = nx * hw, oy = ny * hw;

    float col[4] = {
        (float)color.r / 255.0f, (float)color.g / 255.0f,
        (float)color.b / 255.0f, (float)color.a / 255.0f
    };

    float corners[4][2] = {
        { x0 - ox, y0 - oy }, { x0 + ox, y0 + oy },
        { x1 - ox, y1 - oy }, { x1 + ox, y1 + oy }
    };
    float ndc[4][2];
    for (int c = 0; c < 4; c++) {
        pixie_local_to_ndc(p, corners[c][0], corners[c][1], &ndc[c][0], &ndc[c][1]);
    }

    static const int order[6] = { 0, 1, 2,  1, 3, 2 };
    PixieLiveVertex* v = &p->liveVertices[p->liveVertexCount];
    for (int i = 0; i < 6; i++) {
        int c = order[i];

        v[i].pos[0] = ndc[c][0];
        v[i].pos[1] = ndc[c][1];
        memcpy(v[i].color, col, sizeof(col));
    }
    p->liveVertexCount += 6;

    return;
}

// Stamps a filled width x width square centered on (cx, cy) -- used for
// width > 1 lines. No anti-aliasing/mitered joints, matching the rest of
// the engine's pixel-art aesthetic; corners at a line's joints just come
// out blocky, same as everywhere else nothing is smoothed.
static void pixie_stamp_square(Pixie* p, int cx, int cy, int width, Color color) {
    int half = width / 2;

    for (int dy = 0; dy < width; dy++) {
        int y = cy - half + dy;

        if (y < 0 || y >= p->outputH) {
            continue;
        }
        for (int dx = 0; dx < width; dx++) {
            int x = cx - half + dx;

            if (x < 0 || x >= p->outputW) {
                continue;
            }
            p->output[y * p->outputW + x] = color;
        }
    }

    return;
}

// Standard integer Bresenham. Points outside the output buffer are
// skipped individually rather than rejecting the whole line -- a line
// that runs off the edge of the canvas is routine, not an error.
// width == 1 plots single pixels (the original path); width > 1 stamps a
// square at each step instead.
static void pixie_draw_line(Pixie* p, int x0, int y0, int x1, int y1, Color color, int width) {
    int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (width <= 1) {
            if (x0 >= 0 && x0 < p->outputW && y0 >= 0 && y0 < p->outputH) {
                p->output[y0 * p->outputW + x0] = color;
            }
        } else {
            pixie_stamp_square(p, x0, y0, width, color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }

    return;
}

// Reads a little-endian multi-byte field via memcpy rather than a direct
// pointer cast -- RAM offsets are arbitrary game-supplied byte positions,
// not guaranteed aligned, so an unaligned uint16_t*/uint32_t* dereference
// would be undefined behavior even though it happens to work in practice
// on x86/x64.
static uint16_t pixie_read_u16(const unsigned char* p) {
    uint16_t v;

    memcpy(&v, p, sizeof(v));

    return v;
}
static uint32_t pixie_read_u32(const unsigned char* p) {
    uint32_t v;

    memcpy(&v, p, sizeof(v));

    return v;
}

// Mirrors tools/PixiePacker's rle_decode -- (count, value) byte pairs, one
// pass, no intermediate buffer. Unlike the tool (which only ever decodes
// data it just encoded itself), `outCapacity` is enforced here: pixie RAM
// is written by PixieWrite with no format validation at write time, so a
// corrupt or hand-crafted blob's run lengths could otherwise sum past the
// destination buffer. Excess runs are clipped, not rejected outright --
// there's no way to signal a mid-decode failure back through a plain
// pixel buffer, and a clipped/garbled image is a better failure mode than
// a heap overflow.
static void pixie_rle_decode(const unsigned char* data, uint32_t len, unsigned char* out, size_t outCapacity) {
    uint32_t i = 0;
    size_t o = 0;

    while (i + 2 <= len && o < outCapacity) {
        unsigned char run = data[i++];
        unsigned char v   = data[i++];
        size_t n = run;

        if (o + n > outCapacity) { n = outCapacity - o; }
        memset(out + o, v, n);
        o += n;
    }

    return;
}

// Must match tools/PixiePacker's LZ_MIN_MATCH.
#define PIXIE_LZ_MIN_MATCH 3

// Mirrors tools/PixiePacker's lz_decode -- byte-oriented literal-run /
// match tokens (see pixiepackertool.txt's LZ section): tag byte high bit
// 0 = literal run (low 7 bits = count), high bit 1 = match (low 7 bits =
// length-PIXIE_LZ_MIN_MATCH, followed by a 4-byte back-offset).
//
// Unlike the tool (which only ever decodes data it just encoded itself),
// every field here is treated as untrusted: a corrupt/hand-crafted
// blob's match offset could reference before the start of `out` (an
// actual out-of-bounds READ, not just a garbled pixel like a bad RLE run
// would be, since a match's source bytes are `out` itself) or claim a
// length that runs past outCapacity or past the remaining input. All
// three are checked explicitly and clipped/stopped rather than trusted --
// RLE's "fill with a known value" fallback isn't available here, since a
// match's whole point is copying bytes that must already be valid.
static void pixie_lz_decode(const unsigned char* data, uint32_t len, unsigned char* out, size_t outCapacity) {
    uint32_t i = 0;
    size_t o = 0;

    while (i < len && o < outCapacity) {
        uint8_t tag = data[i++];

        if (tag & 0x80u) {
            if (i + 4 > len) { break; }
            uint32_t offset = pixie_read_u32(data + i);
            i += 4;
            if (offset == 0 || offset > o) { break; }  // would read before the start of `out`
            size_t matchLen = (size_t)(tag & 0x7Fu) + PIXIE_LZ_MIN_MATCH;
            if (o + matchLen > outCapacity) { matchLen = outCapacity - o; }
            for (size_t k = 0; k < matchLen; k++) {
                out[o] = out[o - offset];
                o++;
            }
        } else {
            size_t litLen = tag;

            if (i + litLen > len) { litLen = len - i; }
            if (o + litLen > outCapacity) { litLen = outCapacity - o; }
            memcpy(out + o, data + i, litLen);
            i += (uint32_t)litLen;
            o += litLen;
        }
    }

    return;
}

// Mirrors tools/PixiePacker's delta_decode exactly: integrates residuals
// back into absolute byte values (out[i] = out[i-1] + data[i], wrapping,
// with an implicit "previous byte" of 0 before the first). Only ever
// applied to PIXIE_FORMAT_RLE_RGBA8's per-channel planes, always
// immediately after RLE-decoding, before the plane's bytes are combined
// into Color values -- see pixie_unpack_blob's RLE_RGBA8 case.
static void pixie_delta_decode(const unsigned char* data, size_t count, unsigned char* out) {
    unsigned char prev = 0;

    for (size_t i = 0; i < count; i++) {
        prev = (unsigned char)(prev + data[i]);
        out[i] = prev;
    }

    return;
}

// Writes one decoded pixel into the pixie's output buffer at pixie-local
// coordinates, clipping silently if it falls outside outputW/outputH --
// an unpacked image landing partially off-canvas is routine (same spirit
// as pixie_draw_line's per-pixel clip), not an error worth failing the
// whole UNPACK over.
static void pixie_blit_pixel(Pixie* p, int x, int y, Color color) {
    if (x < 0 || x >= p->outputW || y < 0 || y >= p->outputH) {
        return;
    }
    p->output[y * p->outputW + x] = color;

    return;
}

// Canonical Huffman decode -- mirrors tools/PixiePacker's huffman_encode
// exactly (see that file's comments for why canonical: only 256 code
// LENGTHS travel with the asset, not the tree/codes themselves, and this
// side rebuilds the identical code assignment from lengths alone).

// MSB-first bit reader, matching the packer's BitWriter. Reading past
// `len` returns 0 bits rather than failing -- those are exactly the
// zero-padding bits BitWriter itself wrote past the last real code, and
// treating "read past the buffer" the same way keeps this from needing a
// separate end-of-buffer error path for the common, correct case. This
// never reads out of bounds: bytePos is checked against len every time,
// so a genuinely truncated/malformed blob just decodes to garbage
// symbols instead of touching invalid memory (same tradeoff
// pixie_rle_decode makes).
typedef struct {
    const unsigned char* data;
    size_t len;
    size_t bytePos;
    int    bitPos;
} PixieBitReader;

static int pixie_bitreader_read_bit(PixieBitReader* br) {
    if (br->bytePos >= br->len) {
        return 0;
    }
    int bit = (br->data[br->bytePos] >> (7 - br->bitPos)) & 1;
    br->bitPos++;
    if (br->bitPos == 8) {
        br->bitPos = 0;
        br->bytePos++;
    }

    return bit;
}

// Built once per decode from the 256 code-length bytes stored in the
// blob. firstCode/count are the standard canonical-Huffman per-length
// tables (RFC 1951 3.2.2); symbols is every used byte value laid out
// grouped by (length, then ascending byte value), the same order
// pixiepacker.c's huffman_assign_canonical_codes assigns codes in, so a
// decoded (length, code) pair maps to a symbol via one array index.
typedef struct {
    uint32_t firstCode[256];
    int      count[256];
    int      offset[256];
    unsigned char symbols[256];
} PixieHuffmanDecodeTable;

static void pixie_huffman_build_decode_table(const uint8_t codeLengths[256], PixieHuffmanDecodeTable* t) {
    int blCount[256] = { 0 };

    for (int s = 0; s < 256; s++) {
        if (codeLengths[s] > 0) { blCount[codeLengths[s]]++; }
    }
    memcpy(t->count, blCount, sizeof(blCount));

    uint32_t code = 0;
    for (int len = 1; len < 256; len++) {
        code = (code + (uint32_t)blCount[len - 1]) << 1;
        t->firstCode[len] = code;
    }

    int offset = 0;
    for (int len = 1; len < 256; len++) {
        t->offset[len] = offset;
        offset += blCount[len];
    }

    int cursor[256];
    memcpy(cursor, t->offset, sizeof(cursor));
    for (int s = 0; s < 256; s++) {
        if (codeLengths[s] > 0) {
            t->symbols[cursor[codeLengths[s]]++] = (unsigned char)s;
        }
    }

    return;
}

// Decodes exactly `decodedLength` symbols from `br` using `t`, writing
// them into `out` (caller-allocated, >= decodedLength bytes). Walks one
// bit at a time, extending the candidate code length until it falls in
// the range assigned to some length (the standard canonical-Huffman
// decode loop) -- capped at 255 bits, the maximum any code can be with
// at most 256 symbols, so a malformed/inconsistent code-length table
// can't spin forever; returns false in that case rather than writing
// anything.
static bool pixie_huffman_decode(PixieBitReader* br, const PixieHuffmanDecodeTable* t, size_t decodedLength, unsigned char* out) {
    for (size_t i = 0; i < decodedLength; i++) {
        uint32_t code = 0;
        int len = 0;
        bool found = false;

        while (len < 255) {
            code = (code << 1) | (uint32_t)pixie_bitreader_read_bit(br);
            len++;
            if (t->count[len] > 0) {
                uint32_t idx = code - t->firstCode[len];

                if (idx < (uint32_t)t->count[len]) {
                    out[i] = t->symbols[t->offset[len] + idx];
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

// Reads the length-prefixed byte stream PIXIE_PASS_RLE produces --
// optionally with PIXIE_PASS_HUFFMAN wrapped around it -- undoing
// Huffman if present. `*pos` is advanced past everything consumed either
// way. On success, `*outData`/`*outLen` describe the RLE-encoded stream
// itself; `*outOwned` tells the caller whether that's a pointer straight
// into RAM (false -- do not free) or a freshly heap-decoded buffer (true
// -- caller must free after use). What the RLE stream itself *means*
// (one index stream, or PIXIE_FORMAT_RLE_RGBA8's four-plane layout) is
// still up to the caller -- this only undoes the Huffman wrapping, one
// pass at a time, same as pixie_rle_decode only ever undoes RLE itself.
static bool pixie_undo_huffman_if_present(const Pixie* p, size_t* pos, bool useHuffman,
                                           const unsigned char** outData, size_t* outLen, bool* outOwned) {
    if (!useHuffman) {
        if (*pos + 4 > PIXIE_RAM_SIZE) {
            return false;
        }
        uint32_t len = pixie_read_u32(p->ram + *pos);
        *pos += 4;
        if (*pos + len > PIXIE_RAM_SIZE) {
            return false;
        }
        *outData = p->ram + *pos;
        *outLen = len;
        *outOwned = false;
        *pos += len;
        return true;
    }

    if (*pos + 256 + 8 > PIXIE_RAM_SIZE) {
        return false;
    }
    uint8_t codeLengths[256];
    memcpy(codeLengths, p->ram + *pos, 256);
    *pos += 256;
    uint32_t decodedLength = pixie_read_u32(p->ram + *pos); *pos += 4;
    uint32_t encodedLength = pixie_read_u32(p->ram + *pos); *pos += 4;
    if (*pos + encodedLength > PIXIE_RAM_SIZE) {
        return false;
    }

    PixieHuffmanDecodeTable table;
    pixie_huffman_build_decode_table(codeLengths, &table);

    unsigned char* decoded = malloc(decodedLength);
    PixieBitReader br = { p->ram + *pos, encodedLength, 0, 0 };
    bool ok = pixie_huffman_decode(&br, &table, decodedLength, decoded);
    *pos += encodedLength;
    if (!ok) {
        free(decoded);
        return false;
    }

    *outData = decoded;
    *outLen = decodedLength;
    *outOwned = true;

    return true;
}

// Decodes whichever "primary" pass produced `data` -- RLE or its
// alternative LZ -- into `out`. A single conditional shared by every
// call site below rather than repeating the if/else at each one.
static void pixie_decode_primary(bool useLZ, const unsigned char* data, uint32_t len, unsigned char* out, size_t outCapacity) {
    if (useLZ) {
        pixie_lz_decode(data, len, out, outCapacity);
    } else {
        pixie_rle_decode(data, len, out, outCapacity);
    }

    return;
}

// Decodes a tools/PixiePacker-produced blob starting at RAM byte `offset`
// and blits it into the output buffer at (dstX, dstY). Mirrors the packed
// layout documented in tools/PixiePacker/pixiepackertool.txt byte for
// byte -- uint16 width, uint16 height, uint8 passCount, passCount pass
// IDs, then a format-specific body. The only pass combinations that
// exist are {} (raw formats), {RLE}/{LZ}, {RLE,HUFFMAN}/{LZ,HUFFMAN},
// {DELTA,RLE}/{DELTA,LZ}, and {DELTA,RLE,HUFFMAN}/{DELTA,LZ,HUFFMAN} --
// validated explicitly below rather than interpreted generically, since
// that's every combination PixiePacker can actually produce; a blob
// claiming anything else is malformed. paletteIndex selects which of
// GDMF's 256 shared palette slots (see colors.c) to resolve indices
// against for the two formats that need one; ignored otherwise.
//
// Every read is bounds-checked against PIXIE_RAM_SIZE before it happens --
// pixie RAM is arbitrary game-written bytes (PixieWrite has no format
// validation at write time), so a blob's own header claiming more data
// than actually fits is a real boundary to defend, not a defensive-
// programming reflex. Returns false (output buffer untouched) on any
// bounds failure or unrecognized format; true and p->dirty = true on
// success.
static bool pixie_unpack_blob(Pixie* p, size_t offset, PixieUnpackFormat format,
                               int dstX, int dstY, unsigned char paletteIndex) {
    if (offset + 5 > PIXIE_RAM_SIZE) {
        return false;
    }
    const unsigned char* base = p->ram + offset;
    int width  = pixie_read_u16(base);
    int height = pixie_read_u16(base + 2);
    uint8_t passCount = base[4];
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (offset + 5 + passCount > PIXIE_RAM_SIZE) {
        return false;
    }

    // PIXIE_PASS_LZ is an ALTERNATIVE to PIXIE_PASS_RLE, never stacked
    // with it -- wherever RLE could appear below, LZ can appear in
    // exactly the same position instead. DELTA is only ever paired with
    // RLE_RGBA8 -- it predicts one true-color channel byte from its
    // neighbor, which is meaningless for the two palette-indexed tiers
    // (categorical indices, not a numeric gradient). A blob claiming
    // {DELTA, RLE/LZ(, HUFFMAN)} for a palette format is malformed and
    // rejected outright here, rather than silently decoding an index
    // stream into garbage.
    bool useHuffman = false;
    bool useDelta   = false;
    bool useLZ      = false;
    if (passCount == 0) {
        // no passes -- valid only for the two raw formats, enforced by
        // each of their own cases below
    } else if (passCount == 1) {
        if (base[5] == PIXIE_PASS_LZ) { useLZ = true; }
        else if (base[5] != PIXIE_PASS_RLE) { return false; }
    } else if (passCount == 2) {
        if (base[5] == PIXIE_PASS_RLE && base[6] == PIXIE_PASS_HUFFMAN) {
            useHuffman = true;
        } else if (base[5] == PIXIE_PASS_LZ && base[6] == PIXIE_PASS_HUFFMAN) {
            useLZ = true;
            useHuffman = true;
        } else if (base[5] == PIXIE_PASS_DELTA && base[6] == PIXIE_PASS_RLE) {
            if (format != PIXIE_FORMAT_RLE_RGBA8) { return false; }
            useDelta = true;
        } else if (base[5] == PIXIE_PASS_DELTA && base[6] == PIXIE_PASS_LZ) {
            if (format != PIXIE_FORMAT_RLE_RGBA8) { return false; }
            useDelta = true;
            useLZ    = true;
        } else {
            return false;
        }
    } else if (passCount == 3) {
        if (base[5] != PIXIE_PASS_DELTA || base[7] != PIXIE_PASS_HUFFMAN) {
            return false;
        }
        if (base[6] == PIXIE_PASS_LZ) { useLZ = true; }
        else if (base[6] != PIXIE_PASS_RLE) { return false; }
        if (format != PIXIE_FORMAT_RLE_RGBA8) { return false; }
        useDelta   = true;
        useHuffman = true;
    } else {
        return false;  // no pass combination beyond DELTA+RLE/LZ(+HUFFMAN) exists yet
    }

    size_t pixelCount = (size_t)width * (size_t)height;
    size_t pos = offset + 5 + passCount;

    switch (format) {
        case PIXIE_FORMAT_RLE_PALETTE_OWN: {
            if (pos + 256 * sizeof(Color) > PIXIE_RAM_SIZE) {
                return false;
            }
            const Color* palette = (const Color*)(p->ram + pos);
            pos += 256 * sizeof(Color);

            const unsigned char* rleData; size_t rleLen; bool owned;
            if (!pixie_undo_huffman_if_present(p, &pos, useHuffman, &rleData, &rleLen, &owned)) {
                return false;
            }
            unsigned char* indices = malloc(pixelCount);
            pixie_decode_primary(useLZ, rleData, (uint32_t)rleLen, indices, pixelCount);
            if (owned) { free((void*)rleData); }
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    pixie_blit_pixel(p, dstX + x, dstY + y, palette[indices[(size_t)y * width + x]]);
                }
            }
            free(indices);
            break;
        }

        case PIXIE_FORMAT_RLE_PALETTE_SHARED: {
            const unsigned char* rleData; size_t rleLen; bool owned;
            if (!pixie_undo_huffman_if_present(p, &pos, useHuffman, &rleData, &rleLen, &owned)) {
                return false;
            }
            unsigned char* indices = malloc(pixelCount);
            pixie_decode_primary(useLZ, rleData, (uint32_t)rleLen, indices, pixelCount);
            if (owned) { free((void*)rleData); }
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    Color c = GetPalette(paletteIndex, indices[(size_t)y * width + x]);

                    pixie_blit_pixel(p, dstX + x, dstY + y, c);
                }
            }
            free(indices);
            break;
        }

        case PIXIE_FORMAT_PALETTE4BPP: {
            // Raw, uncompressed -- the auto-mode fallback PixiePacker emits
            // when RLE didn't actually help at the 16-color tier. Never
            // RLE/Huffman-wrapped by construction (passCount 0). 2 indices
            // per byte, high nibble first (matches pack_image's packing).
            if (passCount != 0) {
                return false;
            }
            size_t packedLen = (pixelCount + 1) / 2;
            if (pos + 4 + packedLen > PIXIE_RAM_SIZE) {
                return false;
            }
            pos += 4;  // dataLength -- redundant with packedLen, not re-validated against it
            const unsigned char* packed = p->ram + pos;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    size_t i = (size_t)y * width + x;
                    unsigned char byte = packed[i / 2];
                    unsigned char idx = (i % 2 == 0) ? (unsigned char)(byte >> 4) : (unsigned char)(byte & 0x0F);

                    pixie_blit_pixel(p, dstX + x, dstY + y, GetPalette(paletteIndex, idx));
                }
            }
            break;
        }

        case PIXIE_FORMAT_RLE_RGBA8: {
            // The RLE stream here isn't one flat run of pixels -- it's 4
            // independent per-channel RLE streams, laid out as [4x
            // uint32 plane length][plane0][plane1][plane2][plane3].
            // Huffman (if present) wraps that *whole* assembled layout as
            // one unit, generic to its internal shape -- so once
            // pixie_undo_huffman_if_present hands back the RLE stream,
            // this case still has to parse the 4-plane structure out of
            // it itself, same as it always did directly from RAM.
            const unsigned char* rleData; size_t rleLen; bool owned;
            if (!pixie_undo_huffman_if_present(p, &pos, useHuffman, &rleData, &rleLen, &owned)) {
                return false;
            }
            if (rleLen < 16) {
                if (owned) { free((void*)rleData); }
                return false;
            }
            uint32_t lens[4];
            for (int c = 0; c < 4; c++) {
                lens[c] = pixie_read_u32(rleData + (size_t)c * 4);
            }
            size_t totalLen = (size_t)lens[0] + lens[1] + lens[2] + lens[3];
            if (16 + totalLen > rleLen) {
                if (owned) { free((void*)rleData); }
                return false;
            }
            unsigned char* planes[4];
            size_t planeOffset = 16;
            for (int c = 0; c < 4; c++) {
                planes[c] = malloc(pixelCount);
                pixie_decode_primary(useLZ, rleData + planeOffset, lens[c], planes[c], pixelCount);
                if (useDelta) {
                    // DELTA was applied before RLE/LZ at encode time, so
                    // it's undone after here -- decode always reverses
                    // passes in the opposite order they were applied.
                    unsigned char* undelta = malloc(pixelCount);

                    pixie_delta_decode(planes[c], pixelCount, undelta);
                    free(planes[c]);
                    planes[c] = undelta;
                }
                planeOffset += lens[c];
            }
            if (owned) { free((void*)rleData); }
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    size_t i = (size_t)y * width + x;
                    Color c = { planes[0][i], planes[1][i], planes[2][i], planes[3][i] };

                    pixie_blit_pixel(p, dstX + x, dstY + y, c);
                }
            }
            for (int c = 0; c < 4; c++) { free(planes[c]); }
            break;
        }

        case PIXIE_FORMAT_RGBA8: {
            // Raw, uncompressed -- never RLE/Huffman-wrapped (passCount 0).
            if (passCount != 0) {
                return false;
            }
            size_t rawLen = pixelCount * 4;
            if (pos + 4 + rawLen > PIXIE_RAM_SIZE) {
                return false;
            }
            pos += 4;  // dataLength -- redundant with rawLen, not re-validated
            const unsigned char* raw = p->ram + pos;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    size_t i = (size_t)y * width + x;
                    Color c = { raw[i*4+0], raw[i*4+1], raw[i*4+2], raw[i*4+3] };

                    pixie_blit_pixel(p, dstX + x, dstY + y, c);
                }
            }
            break;
        }

        default:
            return false;
    }

    p->dirty = true;

    return true;
}

bool InitPixie(int id, PixieMode mode, int outputWidth, int outputHeight) {
    if (!pixie_id_valid(id) || outputWidth <= 0 || outputHeight <= 0) {
        return false;
    }
    if (mode != PIXIE_MODE_TEXTURE && mode != PIXIE_MODE_LIVE) {
        return false;
    }

    if (g_pixies[id].initialized) {
        ShutdownPixie(id);
    }

    Pixie* p = &g_pixies[id];
    memset(p, 0, sizeof(*p));

    p->ram = (unsigned char*)calloc(1, PIXIE_RAM_SIZE);
    if (!p->ram) {
        memset(p, 0, sizeof(*p));
        return false;
    }

    if (mode == PIXIE_MODE_TEXTURE) {
        p->output = (Color*)calloc((size_t)outputWidth * (size_t)outputHeight, sizeof(Color));
        if (!p->output) {
            free(p->ram);
            memset(p, 0, sizeof(*p));
            return false;
        }
        p->dirty = true;
    } else {
        p->liveVertices = (PixieLiveVertex*)calloc(PIXIE_LIVE_MAX_VERTICES, sizeof(PixieLiveVertex));
        if (!p->liveVertices) {
            free(p->ram);
            memset(p, 0, sizeof(*p));
            return false;
        }
        p->liveVertexCapacity = PIXIE_LIVE_MAX_VERTICES;
    }

    p->mode = mode;
    p->outputW = outputWidth;
    p->outputH = outputHeight;
    p->w = outputWidth;
    p->h = outputHeight;
    p->enabled = false;
    p->shown = false;
    p->initialized = true;

    return true;
}

// Tears down the resources shared by every pixie -- pipeline, pipeline
// layout, descriptor set layout/pool, sampler, per-frame vertex buffers.
// Called once from ShutdownPixies() (after every individual pixie has
// already freed its own descriptor set below -- see ShutdownPixie), never
// per-pixie. Mirrors cleanup_sprite_render_resources in gdmf_sprites.c.
static void cleanup_pixie_render_resources(void) {
    VkDevice dev = gdmf_get_device();

    if (dev == VK_NULL_HANDLE) { return; }
    vkDeviceWaitIdle(dev);

    for (uint32_t i = 0; i < g_pixie_frame_count; i++) {
        PixieFrameResources* frame = &g_pixie_frames[i];

        if (frame->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
            frame->vertexBuffer = VK_NULL_HANDLE;
        }
        if (frame->vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->vertexMemory, NULL);
            frame->vertexMemory = VK_NULL_HANDLE;
        }
    }
    free(g_pixie_frames);
    g_pixie_frames      = NULL;
    g_pixie_frame_count = 0;

    if (g_pixie_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, g_pixie_vk_pipeline, NULL);
        g_pixie_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_pixie_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, g_pixie_vk_layout, NULL);
        g_pixie_vk_layout = VK_NULL_HANDLE;
    }
    // Destroying the pool implicitly frees any descriptor sets still
    // allocated from it -- there shouldn't be any left at this point
    // (ShutdownPixies frees each pixie's set individually first), but
    // this is also reached directly if Vulkan is torn down with pixies
    // still initialized.
    if (g_pixie_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, g_pixie_descriptor_pool, NULL);
        g_pixie_descriptor_pool = VK_NULL_HANDLE;
    }
    if (g_pixie_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, g_pixie_descriptor_set_layout, NULL);
        g_pixie_descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (g_pixie_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, g_pixie_sampler, NULL);
        g_pixie_sampler = VK_NULL_HANDLE;
    }
    g_pixie_pipeline_ready = false;

    // Mode 1's resources -- much shorter list than Mode 0's above (no
    // descriptor pool/set layout/sampler to tear down, since it never
    // created any).
    for (uint32_t i = 0; i < g_pixie_live_frame_count; i++) {
        PixieLiveFrameResources* frame = &g_pixie_live_frames[i];

        if (frame->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
            frame->vertexBuffer = VK_NULL_HANDLE;
        }
        if (frame->vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->vertexMemory, NULL);
            frame->vertexMemory = VK_NULL_HANDLE;
        }
    }
    free(g_pixie_live_frames);
    g_pixie_live_frames      = NULL;
    g_pixie_live_frame_count = 0;

    if (g_pixie_live_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, g_pixie_live_vk_pipeline, NULL);
        g_pixie_live_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_pixie_live_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, g_pixie_live_vk_layout, NULL);
        g_pixie_live_vk_layout = VK_NULL_HANDLE;
    }
    g_pixie_live_pipeline_ready = false;

    return;
}

void ShutdownPixie(int id) {
    if (!pixie_ready(id)) {
        return;
    }
    Pixie*              p   = &g_pixies[id];
    PixieGPUResources*  gpu = &g_pixie_gpu[id];

    VkDevice dev = gdmf_get_device();
    if (dev != VK_NULL_HANDLE) {
        // Freed individually here, not left for cleanup_pixie_render_
        // resources to reclaim via the pool -- ShutdownPixie can be
        // called for one pixie while others stay alive, and the pool was
        // created with FREE_DESCRIPTOR_SET_BIT specifically so that
        // works without disturbing any other pixie's set.
        if (gpu->descriptorSet != VK_NULL_HANDLE && g_pixie_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(dev, g_pixie_descriptor_pool, 1, &gpu->descriptorSet);
        }
        if (gpu->imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, gpu->imageView, NULL);
        }
        if (gpu->image != VK_NULL_HANDLE) {
            vkDestroyImage(dev, gpu->image, NULL);
        }
        if (gpu->imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, gpu->imageMemory, NULL);
        }
    }
    memset(gpu, 0, sizeof(*gpu));

    free(p->ram);
    free(p->output);       // Mode 0 only; NULL and a no-op in Mode 1
    free(p->liveVertices); // Mode 1 only; NULL and a no-op in Mode 0
    memset(p, 0, sizeof(*p));

    return;
}

void ShutdownPixies(void) {
    for (int i = 0; i < MAX_PIXIES; i++) {
        ShutdownPixie(i);
    }
    cleanup_pixie_render_resources();

    return;
}

// PIXIE_OP_SET_ATTR packet layout -- the only opcode currently implemented.
// args[0] = x, args[1] = y, args[2] = w, args[3] = h (all four required --
// this sets the full display rect every call, it does not patch a subset).
// flags low byte = priority (0-255), flags bit 8 = enabled.
#define PIXIE_SET_ATTR_ENABLED_BIT 0x0100u

// PIXIE_OP_CLEAR packet layout: flags bit 0 set = use args[0] (packed
// RGBA8) as the fill color; clear = fill with transparent black.
#define PIXIE_CLEAR_USE_COLOR_BIT 0x0001u

// Safety clamp for PIXIE_OP_DRAW's width arg -- see the case for why.
#define PIXIE_DRAW_MAX_WIDTH 256

bool PixieCommand(int id, PixieOpcode opcode, uint16_t flags, const uint32_t args[4]) {
    if (!pixie_ready(id)) {
        return false;
    }
    Pixie* p = &g_pixies[id];

    switch (opcode) {
        case PIXIE_OP_SET_ATTR:
            if (!args || (int)args[2] <= 0 || (int)args[3] <= 0) {
                return false;
            }
            p->x = (int)args[0];

            p->y = (int)args[1];
            p->w = (int)args[2];
            p->h = (int)args[3];
            p->priority = (unsigned char)(flags & 0xFFu);
            p->enabled  = (flags & PIXIE_SET_ATTR_ENABLED_BIT) != 0;
            return true;

        // PIXIE_OP_UNPACK: flags = PixieUnpackFormat. args[0] = RAM byte
        // offset where a tools/PixiePacker blob begins (see
        // pixie_unpack_blob above and pixiepackertool.txt for the exact
        // layout). args[1] = (dst_x<<16)|dst_y (see pixie_unpack_xy) --
        // where in the output buffer's local space to blit the decoded
        // image's top-left corner. args[2] = palette slot index (0-255,
        // see colors.c) -- only meaningful for PIXIE_FORMAT_PALETTE4BPP/
        // RLE_PALETTE_SHARED, ignored by formats that embed or don't need
        // their own palette. args[3] reserved. Width/height are NOT passed
        // as args -- they're read out of the blob's own header, so a
        // caller can't claim a size that doesn't match what was packed.
        //
        // Mode 0 only for now: there's no persistent output buffer in
        // Mode 1 to blit into, and Mode 1's pipeline has no texture
        // sampling at all (plain per-vertex color) -- displaying an
        // unpacked image in Mode 1 needs its own design and is
        // deliberately not tackled here.
        case PIXIE_OP_UNPACK: {
            if (!args || p->mode != PIXIE_MODE_TEXTURE) {
                return false;
            }
            if (flags > PIXIE_FORMAT_RLE_RGBA8) {
                return false;
            }
            int dstX, dstY;
            pixie_unpack_xy(args[1], &dstX, &dstY);
            unsigned char paletteIndex = (unsigned char)(args[2] & 0xFFu);
            return pixie_unpack_blob(p, (size_t)args[0], (PixieUnpackFormat)flags, dstX, dstY, paletteIndex);
        }

        // PIXIE_OP_DRAW: args[0] = (x0<<16)|y0, args[1] = (x1<<16)|y1
        // (see pixie_unpack_xy), args[2] = packed RGBA8 color, args[3] =
        // line width in pixels (0 defaults to 1). Clamped to
        // PIXIE_DRAW_MAX_WIDTH -- not a real spec limit, just a guard
        // against a garbage/malicious args[3]. Valid in both modes:
        // Mode 0 walks pixel-by-pixel (cheap for a CPU buffer write);
        // Mode 1 emits the whole line as one rotated-rectangle quad
        // instead (see pixie_live_push_line's comment for why that's not
        // just "the same algorithm, ported").
        case PIXIE_OP_DRAW: {
            if (!args) {
                return false;
            }
            int x0, y0, x1, y1;
            pixie_unpack_xy(args[0], &x0, &y0);
            pixie_unpack_xy(args[1], &x1, &y1);
            int width = (args[3] == 0) ? 1 : (int)args[3];
            if (width > PIXIE_DRAW_MAX_WIDTH) {
                width = PIXIE_DRAW_MAX_WIDTH;
            }
            Color color = pixie_unpack_color(args[2]);
            if (p->mode == PIXIE_MODE_TEXTURE) {
                pixie_draw_line(p, x0, y0, x1, y1, color, width);
                p->dirty = true;
            } else {
                pixie_live_push_line(p, (float)x0, (float)y0, (float)x1, (float)y1, (float)width, color);
            }
            return true;
        }

        // PIXIE_OP_PLOT: args[0] = x, args[1] = y, args[2] = packed RGBA8
        // color. Mode 0: out-of-bounds x/y fails rather than clamping --
        // silently writing to a clamped position would be a worse
        // surprise than a returned false. Mode 1: no buffer to be
        // "in bounds" of, so out-of-canvas points are simply accepted and
        // clipped visually by the pixie's own display rect at render time
        // (harmless -- an off-canvas quad just doesn't intersect it).
        case PIXIE_OP_PLOT: {
            if (!args) {
                return false;
            }
            int x = (int)args[0];
            int y = (int)args[1];
            Color color = pixie_unpack_color(args[2]);
            if (p->mode == PIXIE_MODE_TEXTURE) {
                if (x < 0 || x >= p->outputW || y < 0 || y >= p->outputH) {
                    return false;
                }
                p->output[y * p->outputW + x] = color;
                p->dirty = true;
            } else {
                pixie_live_push_quad(p, (float)x, (float)y, 0.5f, 0.5f, color);
            }
            return true;
        }

        // PIXIE_OP_CLEAR: flags bit 0 set = fill with args[0] (packed
        // RGBA8); bit 0 clear = fill with fully transparent black,
        // ignoring args entirely (args may be NULL in that case). Mode 0:
        // fills the persistent output buffer. Mode 1: resets this frame's
        // accumulated primitives to empty (discarding anything already
        // drawn earlier this same frame -- a real reset, not a no-op,
        // even though nothing carries over to the *next* frame either
        // way), then optionally pushes one full-area quad as the new
        // first primitive so later PLOT/DRAW calls layer on top of it.
        case PIXIE_OP_CLEAR: {
            bool useColor = (flags & PIXIE_CLEAR_USE_COLOR_BIT) != 0;
            if (useColor && !args) {
                return false;
            }
            Color fill = useColor ? pixie_unpack_color(args[0]) : (Color){ 0, 0, 0, 0 };

            if (p->mode == PIXIE_MODE_TEXTURE) {
                int pixelCount = p->outputW * p->outputH;

                for (int i = 0; i < pixelCount; i++) {
                    p->output[i] = fill;
                }
                p->dirty = true;
            } else {
                p->liveVertexCount = 0;
                if (useColor) {
                    pixie_live_push_quad(p, (float)p->outputW / 2.0f, (float)p->outputH / 2.0f,
                        (float)p->outputW / 2.0f, (float)p->outputH / 2.0f, fill);
                }
            }
            return true;
        }

        case PIXIE_OP_SHOW:
            p->shown = true;
            return true;

        case PIXIE_OP_HIDE:
            p->shown = false;
            return true;

        case PIXIE_OP_EXECUTE:
            // Stub -- VPU bridge. No-op in Mode 0.
            return false;

        default:
            return false;
    }
}

bool PixieWrite(int id, size_t offset, const void* data, size_t size) {
    if (!pixie_ready(id) || !data) {
        return false;
    }
    if (offset > PIXIE_RAM_SIZE || size > PIXIE_RAM_SIZE - offset) {
        return false;
    }
    memcpy(g_pixies[id].ram + offset, data, size);

    return true;
}

size_t GetPixieRAMSize(int id) {
    return pixie_ready(id) ? PIXIE_RAM_SIZE : 0;
}

size_t PixieReadString(int id, char* buf, size_t maxlen) {
    if (!pixie_ready(id) || !buf || maxlen == 0) {
        return 0;
    }
    Pixie* p = &g_pixies[id];
    size_t len = 0;
    while (len < PIXIE_OUTPUT_STRING_LEN && p->output_string[len] != '\0') {
        len++;
    }
    if (len >= maxlen) {
        len = maxlen - 1;
    }
    memcpy(buf, p->output_string, len);
    buf[len] = '\0';

    return len;
}

// --- Ergonomic wrappers -----------------------------------------------
// Each builds a command packet and dispatches through PixieCommand, per
// the "everything is a command" design decision -- these are not a
// separate mutation path.

// SET_ATTR sets the full display rect + priority + enabled every call (see
// the packet layout comment above PixieCommand) -- these wrappers read
// whatever they're not changing back out of the pixie first, so e.g.
// SetPixiePosition can't clobber a size set earlier by SetPixieDisplaySize.
static uint16_t pixie_current_attr_flags(int id) {
    return (uint16_t)(GetPixiePriority(id) | (GetPixieEnabled(id) ? PIXIE_SET_ATTR_ENABLED_BIT : 0u));
}

bool SetPixiePosition(int id, int x, int y) {
    uint32_t args[4] = {
        (uint32_t)x, (uint32_t)y,
        (uint32_t)GetPixieDisplayWidth(id), (uint32_t)GetPixieDisplayHeight(id)
    };

    return PixieCommand(id, PIXIE_OP_SET_ATTR, pixie_current_attr_flags(id), args);
}

bool SetPixieDisplaySize(int id, int w, int h) {
    uint32_t args[4] = {
        (uint32_t)GetPixieX(id), (uint32_t)GetPixieY(id),
        (uint32_t)w, (uint32_t)h
    };

    return PixieCommand(id, PIXIE_OP_SET_ATTR, pixie_current_attr_flags(id), args);
}

bool SetPixiePriority(int id, unsigned char priority) {
    uint32_t args[4] = {
        (uint32_t)GetPixieX(id), (uint32_t)GetPixieY(id),
        (uint32_t)GetPixieDisplayWidth(id), (uint32_t)GetPixieDisplayHeight(id)
    };
    uint16_t flags = (uint16_t)(priority | (GetPixieEnabled(id) ? PIXIE_SET_ATTR_ENABLED_BIT : 0u));
    return PixieCommand(id, PIXIE_OP_SET_ATTR, flags, args);
}

bool SetPixieEnabled(int id, bool enabled) {
    uint32_t args[4] = {
        (uint32_t)GetPixieX(id), (uint32_t)GetPixieY(id),
        (uint32_t)GetPixieDisplayWidth(id), (uint32_t)GetPixieDisplayHeight(id)
    };
    uint16_t flags = (uint16_t)(GetPixiePriority(id) | (enabled ? PIXIE_SET_ATTR_ENABLED_BIT : 0u));
    return PixieCommand(id, PIXIE_OP_SET_ATTR, flags, args);
}

bool ShowPixie(int id) {
    return PixieCommand(id, PIXIE_OP_SHOW, 0, NULL);
}

bool HidePixie(int id) {
    return PixieCommand(id, PIXIE_OP_HIDE, 0, NULL);
}

// --- Read-only accessors ------------------------------------------------
bool GetPixieInitialized(int id) {
    return pixie_ready(id);
}

PixieMode GetPixieMode(int id) {
    return pixie_ready(id) ? g_pixies[id].mode : PIXIE_MODE_TEXTURE;
}

int GetPixieOutputWidth(int id) {
    return pixie_ready(id) ? g_pixies[id].outputW : 0;
}

int GetPixieOutputHeight(int id) {
    return pixie_ready(id) ? g_pixies[id].outputH : 0;
}

int GetPixieX(int id) {
    return pixie_ready(id) ? g_pixies[id].x : 0;
}

int GetPixieY(int id) {
    return pixie_ready(id) ? g_pixies[id].y : 0;
}

int GetPixieDisplayWidth(int id) {
    return pixie_ready(id) ? g_pixies[id].w : 0;
}

int GetPixieDisplayHeight(int id) {
    return pixie_ready(id) ? g_pixies[id].h : 0;
}

unsigned char GetPixiePriority(int id) {
    return pixie_ready(id) ? g_pixies[id].priority : 0;
}

bool GetPixieEnabled(int id) {
    return pixie_ready(id) ? g_pixies[id].enabled : false;
}

bool GetPixieShown(int id) {
    return pixie_ready(id) ? g_pixies[id].shown : false;
}

// --- Vulkan-side image creation ------------------------------------------
// Not called from anywhere yet -- gdmf_pixies_prepare() below is still a
// no-op. This is the piece it will call next: given an initialized pixie
// with gpu->ready still false, get its VkImage/view into a valid,
// sample-able state. Mirrors create_vulkan_sprite_atlas() in
// gdmf_sprites.c closely, minus the array-layers/atlas parts that don't
// apply here (a pixie owns exactly one image, not a shared set of slots).

// RGBA8, not the sprite atlas's raw palette-index format -- a pixie's
// output buffer is already resolved color, so this is a normal color
// image, not an integer one.
#define PIXIE_IMAGE_FORMAT VK_FORMAT_R8G8B8A8_UNORM

// One-time command callback: transitions a freshly created pixie image
// straight to SHADER_READ_ONLY_OPTIMAL before any pixel data exists, same
// reasoning as record_sprite_atlas_init_layout in gdmf_sprites.c -- the
// descriptor set will reference this image, and Vulkan requires a valid
// layout for that even with nothing uploaded yet. Sampling before the
// first real dirty upload just reads whatever garbage the fresh
// allocation happened to contain.
static void record_pixie_init_layout(VkCommandBuffer cmd, void* user_data) {
    VkImageMemoryBarrier* barrier = (VkImageMemoryBarrier*)user_data;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, barrier);

    return;
}

// Sampler shared by every pixie's descriptor set. NEAREST filtering is a
// deliberate choice, not a default carried over from copying sprites:
// pixie_plans.txt's v1 success criterion is stretching a low-res buffer
// to fill the screen for a VGA-framebuffer look, and that only reads as
// intentional retro pixel art with blocky nearest-neighbor scaling --
// linear filtering would just blur it.
static int ensure_pixie_sampler(void) {
    if (g_pixie_sampler != VK_NULL_HANDLE) { return 0; }

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
    if (vkCreateSampler(gdmf_get_device(), &samp_info, NULL, &g_pixie_sampler) != VK_SUCCESS) {
        printf("[Pixies] Failed to create pixie sampler\n");
        return -1;
    }

    return 0;
}

// One binding: the pixie's own image, sampled in the fragment shader. No
// second binding for a palette buffer like sprites/tiles carry -- a
// pixie's output is already resolved color, nothing left to look up.
static int ensure_pixie_descriptor_set_layout(void) {
    if (g_pixie_descriptor_set_layout != VK_NULL_HANDLE) { return 0; }

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
            &g_pixie_descriptor_set_layout) != VK_SUCCESS) {
        printf("[Pixies] Failed to create descriptor set layout\n");
        return -1;
    }

    return 0;
}

// Sized for MAX_PIXIES sets total -- one per pixie, not one per swapchain
// image like sprites' per-frame sets. A pixie's image isn't rewritten
// every frame the way sprites' palette buffer is, so there's no
// in-flight-frame hazard requiring per-image duplication here. Created
// with FREE_DESCRIPTOR_SET_BIT so a single pixie's set can eventually be
// released on its own (e.g. from ShutdownPixie) without invalidating
// every other pixie's set -- that teardown path isn't wired up yet, but
// the pool's creation flags can't be changed after the fact, so it's
// worth getting right now rather than recreating the whole pool later.
static int ensure_pixie_descriptor_pool(void) {
    if (g_pixie_descriptor_pool != VK_NULL_HANDLE) { return 0; }

    VkDescriptorPoolSize pool_size = {
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_PIXIES
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .poolSizeCount = 1,
        .pPoolSizes    = &pool_size,
        .maxSets       = MAX_PIXIES
    };
    if (vkCreateDescriptorPool(gdmf_get_device(), &pool_info, NULL, &g_pixie_descriptor_pool) != VK_SUCCESS) {
        printf("[Pixies] Failed to create descriptor pool\n");
        return -1;
    }

    return 0;
}

// Allocates and writes gpu->descriptorSet for one pixie, pointing at its
// own image view + the shared sampler. Called once per pixie right after
// its image is created -- unlike sprites, nothing here needs redoing per
// swapchain image or per frame.
static bool create_pixie_descriptor_set(int id) {
    PixieGPUResources* gpu = &g_pixie_gpu[id];
    VkDevice dev = gdmf_get_device();

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = g_pixie_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &g_pixie_descriptor_set_layout
    };

    if (vkAllocateDescriptorSets(dev, &alloc_info, &gpu->descriptorSet) != VK_SUCCESS) {
        printf("[Pixies] Failed to allocate pixie %d descriptor set\n", id);
        return false;
    }

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView   = gpu->imageView,
        .sampler     = g_pixie_sampler
    };
    VkWriteDescriptorSet write = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = gpu->descriptorSet,
        .dstBinding      = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo      = &image_info
    };
    vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    return true;
}

// Creates gpu->image/imageMemory/imageView sized to pixie id's output
// buffer and leaves it in SHADER_READ_ONLY_OPTIMAL, ready to be sampled
// (with undefined/garbage contents until the first dirty upload). Cleans
// up anything partially created on failure, same pattern as
// create_vulkan_sprite_atlas.
static bool create_pixie_image(int id) {
    Pixie* p = &g_pixies[id];
    PixieGPUResources* gpu = &g_pixie_gpu[id];
    VkDevice dev = gdmf_get_device();

    VkImageCreateInfo image_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = PIXIE_IMAGE_FORMAT,
        .extent        = { (uint32_t)p->outputW, (uint32_t)p->outputH, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (vkCreateImage(dev, &image_info, NULL, &gpu->image) != VK_SUCCESS) {
        printf("[Pixies] Failed to create pixie %d image\n", id);
        return false;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(dev, gpu->image, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &gpu->imageMemory) != VK_SUCCESS) {
        printf("[Pixies] Failed to allocate pixie %d image memory\n", id);
        vkDestroyImage(dev, gpu->image, NULL);
        gpu->image = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(dev, gpu->image, gpu->imageMemory, 0);

    VkImageViewCreateInfo view_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = gpu->image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = PIXIE_IMAGE_FORMAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    if (vkCreateImageView(dev, &view_info, NULL, &gpu->imageView) != VK_SUCCESS) {
        printf("[Pixies] Failed to create pixie %d image view\n", id);
        vkDestroyImage(dev, gpu->image, NULL);
        vkFreeMemory(dev, gpu->imageMemory, NULL);
        gpu->image       = VK_NULL_HANDLE;
        gpu->imageMemory = VK_NULL_HANDLE;
        return false;
    }

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = gpu->image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT
    };
    if (gdmfExecuteOneTimeCommands(record_pixie_init_layout, &barrier) != 0) {
        printf("[Pixies] Failed to initialize pixie %d image layout\n", id);
        vkDestroyImageView(dev, gpu->imageView, NULL);
        vkDestroyImage(dev, gpu->image, NULL);
        vkFreeMemory(dev, gpu->imageMemory, NULL);
        gpu->imageView   = VK_NULL_HANDLE;
        gpu->image       = VK_NULL_HANDLE;
        gpu->imageMemory = VK_NULL_HANDLE;
        return false;
    }

    if (ensure_pixie_sampler() != 0 ||
        ensure_pixie_descriptor_set_layout() != 0 ||
        ensure_pixie_descriptor_pool() != 0 ||
        !create_pixie_descriptor_set(id)) {
        printf("[Pixies] Failed to set up pixie %d descriptor set\n", id);
        vkDestroyImageView(dev, gpu->imageView, NULL);
        vkDestroyImage(dev, gpu->image, NULL);
        vkFreeMemory(dev, gpu->imageMemory, NULL);
        gpu->imageView   = VK_NULL_HANDLE;
        gpu->image       = VK_NULL_HANDLE;
        gpu->imageMemory = VK_NULL_HANDLE;
        return false;
    }

    gpu->ready = true;
    printf("[Pixies] Pixie %d image + descriptor set created (%dx%d)\n", id, p->outputW, p->outputH);

    return true;
}

// One-time command callback: uploads a pixie's full output buffer from a
// staging buffer into its image. Same transition dance as
// record_sprite_bitmap_upload in gdmf_sprites.c -- SHADER_READ_ONLY_OPTIMAL
// -> TRANSFER_DST_OPTIMAL -> copy -> back to SHADER_READ_ONLY_OPTIMAL --
// except there's only ever one array layer (index 0), since a pixie's
// image isn't an atlas.
typedef struct {
    VkBuffer stagingBuffer;
    VkImage  image;
    int      outputW;
    int      outputH;
} PixieUploadData;

static void record_pixie_upload(VkCommandBuffer cmd, void* user_data) {
    PixieUploadData* upload = (PixieUploadData*)user_data;

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = upload->image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset       = { 0, 0, 0 },
        .imageExtent       = { (uint32_t)upload->outputW, (uint32_t)upload->outputH, 1 }
    };
    vkCmdCopyBufferToImage(cmd, upload->stagingBuffer, upload->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    return;
}

// Copies a pixie's full CPU output buffer to its GPU image via a one-shot
// staging buffer, then clears dirty. The staging buffer is created and
// torn down on the spot rather than kept around persistently -- matches
// the "call once, display forever" philosophy from pixie_plans.txt:
// uploads are rare, so there's no reason to hold a standing staging
// allocation for the common case of nothing needing to upload this frame.
static void upload_pixie_output(int id) {
    Pixie*             p   = &g_pixies[id];
    PixieGPUResources* gpu = &g_pixie_gpu[id];
    VkDevice           dev = gdmf_get_device();

    VkDeviceSize size = (VkDeviceSize)p->outputW * (VkDeviceSize)p->outputH * sizeof(Color);

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(dev, &buf_info, NULL, &stagingBuffer) != VK_SUCCESS) {
        printf("[Pixies] Pixie %d: failed to create upload staging buffer\n", id);
        return;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(dev, stagingBuffer, &mem_req);
    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = gdmfFindMemoryType(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (alloc_info.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &alloc_info, NULL, &stagingMemory) != VK_SUCCESS) {
        printf("[Pixies] Pixie %d: failed to allocate upload staging memory\n", id);
        vkDestroyBuffer(dev, stagingBuffer, NULL);
        return;
    }
    vkBindBufferMemory(dev, stagingBuffer, stagingMemory, 0);

    void* mapped;
    if (vkMapMemory(dev, stagingMemory, 0, size, 0, &mapped) != VK_SUCCESS) {
        printf("[Pixies] Pixie %d: failed to map upload staging memory\n", id);
        vkFreeMemory(dev, stagingMemory, NULL);
        vkDestroyBuffer(dev, stagingBuffer, NULL);
        return;
    }
    memcpy(mapped, p->output, (size_t)size);
    vkUnmapMemory(dev, stagingMemory);

    PixieUploadData upload = {
        .stagingBuffer = stagingBuffer,
        .image         = gpu->image,
        .outputW       = p->outputW,
        .outputH       = p->outputH
    };
    if (gdmfExecuteOneTimeCommands(record_pixie_upload, &upload) != 0) {
        printf("[Pixies] Pixie %d: failed to upload output buffer\n", id);
    } else {
        p->dirty = false;
    }

    vkDestroyBuffer(dev, stagingBuffer, NULL);
    vkFreeMemory(dev, stagingMemory, NULL);

    return;
}

// Pipeline. Created lazily from gdmf_pixies_prepare(); a cheap no-op
// (single flag check) once ready. Unlike sprites, this doesn't depend on
// any particular pixie's image existing first -- the pipeline only needs
// its shaders/layout/blend state, since each draw call in record_band
// binds whichever pixie's own descriptor set it needs. Shader modules are
// temporary and destroyed right after building the pipeline, same as
// gdmf_sprites.c does.
static int ensure_pixie_pipeline(void) {
    if (g_pixie_pipeline_ready) { return 0; }

    VkDevice dev = gdmf_get_device();

    uint32_t frameCount = gdmf_get_swapchain_image_count();
    if (frameCount == 0) { return -1; }
    if (g_pixie_frames == NULL) {
        g_pixie_frames = calloc(frameCount, sizeof(PixieFrameResources));
        if (!g_pixie_frames) {
            printf("[Pixies] Failed to allocate per-frame resource array\n");
            return -1;
        }
        g_pixie_frame_count = frameCount;
    }

    if (ensure_pixie_descriptor_set_layout() != 0) { return -1; }

    VkShaderModuleCreateInfo vert_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = pixie_vert_spv_len,
        .pCode    = (const uint32_t*)pixie_vert_spv
    };
    VkShaderModuleCreateInfo frag_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = pixie_frag_spv_len,
        .pCode    = (const uint32_t*)pixie_frag_spv
    };
    VkShaderModule vert_mod, frag_mod;
    if (vkCreateShaderModule(dev, &vert_ci, NULL, &vert_mod) != VK_SUCCESS) {
        printf("[Pixies] Failed to create vertex shader module\n");
        return -1;
    }
    if (vkCreateShaderModule(dev, &frag_ci, NULL, &frag_mod) != VK_SUCCESS) {
        printf("[Pixies] Failed to create fragment shader module\n");
        vkDestroyShaderModule(dev, vert_mod, NULL);
        return -1;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_mod, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_mod, .pName = "main" }
    };

    // Just pos + uv -- no per-vertex color/palette/transparency like
    // sprites/tiles carry (7 attributes there vs. 2 here), since a
    // pixie's own output buffer already is the final color.
    VkVertexInputBindingDescription binding = {
        .binding   = 0,
        .stride    = sizeof(PixieVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attrs[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = (uint32_t)offsetof(PixieVertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = (uint32_t)offsetof(PixieVertex, uv) }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 2,
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

    // Same straight alpha blending as sprites/tiles -- a pixie's alpha
    // comes from whatever it wrote via CLEAR/PLOT/DRAW, so it composites
    // the same way anything else translucent does.
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
        .pSetLayouts    = &g_pixie_descriptor_set_layout
    };
    if (vkCreatePipelineLayout(dev, &layout_ci, NULL, &g_pixie_vk_layout) != VK_SUCCESS) {
        printf("[Pixies] Failed to create pipeline layout\n");
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
        .layout              = g_pixie_vk_layout,
        .renderPass          = gdmf_get_render_pass(),
        .subpass             = 0
    };
    VkResult result = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &g_pixie_vk_pipeline);

    vkDestroyShaderModule(dev, vert_mod, NULL);
    vkDestroyShaderModule(dev, frag_mod, NULL);

    if (result != VK_SUCCESS) {
        printf("[Pixies] Failed to create graphics pipeline\n");
        vkDestroyPipelineLayout(dev, g_pixie_vk_layout, NULL);
        g_pixie_vk_layout = VK_NULL_HANDLE;
        return -1;
    }

    g_pixie_pipeline_ready = true;
    printf("[Pixies] Pipeline ready\n");

    return 0;
}

// Mode 1's pipeline. Much simpler than ensure_pixie_pipeline: no
// descriptor set layout at all (pipeline layout has zero descriptor
// sets -- valid in Vulkan, since there's nothing to sample), 2 vertex
// attributes (pos, color) instead of pos+uv, no dependency on any
// per-pixie GPU resource existing (Mode 1 pixies have none).
static int ensure_pixie_live_pipeline(void) {
    if (g_pixie_live_pipeline_ready) { return 0; }

    VkDevice dev = gdmf_get_device();

    uint32_t frameCount = gdmf_get_swapchain_image_count();
    if (frameCount == 0) { return -1; }
    if (g_pixie_live_frames == NULL) {
        g_pixie_live_frames = calloc(frameCount, sizeof(PixieLiveFrameResources));
        if (!g_pixie_live_frames) {
            printf("[Pixies] Failed to allocate live per-frame resource array\n");
            return -1;
        }
        g_pixie_live_frame_count = frameCount;
    }

    VkShaderModuleCreateInfo vert_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = pixie_live_vert_spv_len,
        .pCode    = (const uint32_t*)pixie_live_vert_spv
    };
    VkShaderModuleCreateInfo frag_ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = pixie_live_frag_spv_len,
        .pCode    = (const uint32_t*)pixie_live_frag_spv
    };
    VkShaderModule vert_mod, frag_mod;
    if (vkCreateShaderModule(dev, &vert_ci, NULL, &vert_mod) != VK_SUCCESS) {
        printf("[Pixies] Failed to create live vertex shader module\n");
        return -1;
    }
    if (vkCreateShaderModule(dev, &frag_ci, NULL, &frag_mod) != VK_SUCCESS) {
        printf("[Pixies] Failed to create live fragment shader module\n");
        vkDestroyShaderModule(dev, vert_mod, NULL);
        return -1;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_mod, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_mod, .pName = "main" }
    };

    VkVertexInputBindingDescription binding = {
        .binding   = 0,
        .stride    = sizeof(PixieLiveVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attrs[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = (uint32_t)offsetof(PixieLiveVertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = (uint32_t)offsetof(PixieLiveVertex, color) }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = 2,
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

    // No descriptor sets at all -- this is the whole point of Mode 1.
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts    = NULL
    };
    if (vkCreatePipelineLayout(dev, &layout_ci, NULL, &g_pixie_live_vk_layout) != VK_SUCCESS) {
        printf("[Pixies] Failed to create live pipeline layout\n");
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
        .layout              = g_pixie_live_vk_layout,
        .renderPass          = gdmf_get_render_pass(),
        .subpass             = 0
    };
    VkResult result = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &g_pixie_live_vk_pipeline);

    vkDestroyShaderModule(dev, vert_mod, NULL);
    vkDestroyShaderModule(dev, frag_mod, NULL);

    if (result != VK_SUCCESS) {
        printf("[Pixies] Failed to create live graphics pipeline\n");
        vkDestroyPipelineLayout(dev, g_pixie_live_vk_layout, NULL);
        g_pixie_live_vk_layout = VK_NULL_HANDLE;
        return -1;
    }

    g_pixie_live_pipeline_ready = true;
    printf("[Pixies] Live pipeline ready\n");

    return 0;
}

// Vertex buffer sized once for the true worst case (MAX_PIXIES * 6
// vertices, ~1.5KB at 16 pixies) -- unlike sprites'/tiles' grow-only
// buffers, a pixie's per-frame vertex count has a small fixed ceiling, so
// there's no reason for a resizing scheme here.
static int ensure_pixie_vertex_buffer(PixieFrameResources* frame) {
    if (frame->vertexBuffer != VK_NULL_HANDLE) { return 0; }

    VkDevice dev = gdmf_get_device();
    frame->vertexCapacity = MAX_PIXIES * 6;

    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = frame->vertexCapacity * sizeof(PixieVertex),
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &frame->vertexBuffer) != VK_SUCCESS) {
        printf("[Pixies] Failed to create vertex buffer\n");
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
        printf("[Pixies] Failed to allocate vertex buffer memory\n");
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        frame->vertexBuffer   = VK_NULL_HANDLE;
        frame->vertexCapacity = 0;
        return -1;
    }
    vkBindBufferMemory(dev, frame->vertexBuffer, frame->vertexMemory, 0);

    return 0;
}

// Grow-only, same pattern as gdmf_sprites.c's ensure_sprite_vertex_buffer
// -- Mode 1's total per-frame vertex count (summed across every live
// pixie's accumulated primitives) isn't bounded the small fixed way Mode
// 0's quad-per-pixie buffer is.
static int ensure_pixie_live_vertex_buffer(PixieLiveFrameResources* frame, uint32_t required_vertices) {
    if (frame->vertexBuffer != VK_NULL_HANDLE && required_vertices <= frame->vertexCapacity) { return 0; }

    VkDevice dev = gdmf_get_device();
    if (frame->vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        vkFreeMemory(dev, frame->vertexMemory, NULL);
        frame->vertexBuffer = VK_NULL_HANDLE;
        frame->vertexMemory = VK_NULL_HANDLE;
    }

    frame->vertexCapacity = required_vertices + 256;  // headroom, same margin sprites use
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = frame->vertexCapacity * sizeof(PixieLiveVertex),
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(dev, &buf_info, NULL, &frame->vertexBuffer) != VK_SUCCESS) {
        printf("[Pixies] Failed to create live vertex buffer\n");
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
        printf("[Pixies] Failed to allocate live vertex buffer memory\n");
        vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
        frame->vertexBuffer   = VK_NULL_HANDLE;
        frame->vertexCapacity = 0;
        return -1;
    }
    vkBindBufferMemory(dev, frame->vertexBuffer, frame->vertexMemory, 0);

    return 0;
}

// Emits 6 vertices (two triangles) for one pixie's display quad. No flip,
// no rotation -- a pixie is a plain axis-aligned rect defined by its
// display x/y/w/h, closer to a tile quad than a sprite in that sense.
static void emit_pixie_quad(PixieVertex* verts, uint32_t* count,
                             float screenX, float screenY,
                             float screenW, float screenH) {
    float x0 = pixie_world_to_ndc_x(screenX);
    float y0 = pixie_world_to_ndc_y(screenY);
    float x1 = pixie_world_to_ndc_x(screenX + screenW);
    float y1 = pixie_world_to_ndc_y(screenY + screenH);

    PixieVertex q[6] = {
        { {x0, y0}, {0.0f, 0.0f} },
        { {x1, y0}, {1.0f, 0.0f} },
        { {x0, y1}, {0.0f, 1.0f} },
        { {x1, y0}, {1.0f, 0.0f} },
        { {x1, y1}, {1.0f, 1.0f} },
        { {x0, y1}, {0.0f, 1.0f} }
    };

    memcpy(&verts[*count], q, sizeof(q));
    *count += 6;

    return;
}

// --- Vulkan hooks (called from gdmf_vulkan.c) ---------------------------
// prepare() creates images, uploads dirty pixel data for Mode 0 pixies,
// keeps both pipelines alive, and builds this frame's vertex data for
// whichever mode(s) are actually in use. record_band draws each pixie
// whose priority falls in the requested band, dispatching per-pixie on
// its own mode.
void gdmf_pixies_prepare(uint32_t imageIndex) {
    // Lazily (re)creates each mode's pipeline/frame resources if they
    // don't exist yet; a cheap no-op (single flag check) once ready.
    // Independent per mode -- a scene using only one mode isn't blocked
    // by the other mode's pipeline, though in practice either failing
    // indicates a deeper Vulkan problem.
    int texPipelineResult  = ensure_pixie_pipeline();
    int livePipelineResult = ensure_pixie_live_pipeline();

    // Lazily create each initialized-but-not-yet-GPU-ready Mode 0 pixie's
    // image + descriptor set, and upload any dirty pixel data. Mode 1
    // pixies have no GPU resource to create here at all.
    for (int id = 0; id < MAX_PIXIES; id++) {
        Pixie* p = &g_pixies[id];

        if (!p->initialized || p->mode != PIXIE_MODE_TEXTURE) {
            continue;
        }
        if (!g_pixie_gpu[id].ready) {
            create_pixie_image(id);
        }
        // InitPixie leaves a fresh pixie dirty, so this also covers the
        // very first upload (even an all-transparent buffer) -- not just
        // re-uploads after a later CLEAR/PLOT/DRAW.
        if (g_pixie_gpu[id].ready && p->dirty) {
            upload_pixie_output(id);
        }
    }

    // Build this frame's draw list: initialized, enabled, shown, and (Mode
    // 0 only) its GPU image ready -- Mode 1 needs no such readiness. No
    // priority sort needed here (unlike sprites) -- record_band filters by
    // each pixie's own priority directly rather than drawing a
    // precomputed contiguous band slice.
    memset(g_pixie_drawn_this_frame, 0, sizeof(g_pixie_drawn_this_frame));

    int drawOrder[MAX_PIXIES];
    int drawCount = 0;
    for (int id = 0; id < MAX_PIXIES; id++) {
        Pixie* p = &g_pixies[id];

        if (!p->initialized || !p->enabled || !p->shown) { continue; }
        if (p->mode == PIXIE_MODE_TEXTURE && !g_pixie_gpu[id].ready) { continue; }
        drawOrder[drawCount++] = id;
    }

    // Mode 0: one quad per pixie into the fixed-size shared buffer.
    if (texPipelineResult == 0 && imageIndex < g_pixie_frame_count && drawCount > 0) {
        PixieFrameResources* frame = &g_pixie_frames[imageIndex];

        if (ensure_pixie_vertex_buffer(frame) == 0) {
            VkDevice dev = gdmf_get_device();
            PixieVertex* vertices;

            if (vkMapMemory(dev, frame->vertexMemory, 0, VK_WHOLE_SIZE, 0, (void**)&vertices) == VK_SUCCESS) {
                uint32_t vertex_index = 0;

                for (int k = 0; k < drawCount; k++) {
                    int id = drawOrder[k];
                    Pixie* p = &g_pixies[id];

                    if (p->mode != PIXIE_MODE_TEXTURE) { continue; }

                    g_pixie_vertex_offset[id] = vertex_index;
                    emit_pixie_quad(vertices, &vertex_index, (float)p->x, (float)p->y, (float)p->w, (float)p->h);
                    g_pixie_drawn_this_frame[id] = true;
                }
                vkUnmapMemory(dev, frame->vertexMemory);
            } else {
                printf("[Pixies] Failed to map vertex buffer\n");
            }
        }
    }

    // Mode 1: concatenate every live pixie's already-NDC-resolved
    // accumulated vertices into the grow-only shared buffer.
    if (livePipelineResult == 0 && imageIndex < g_pixie_live_frame_count && drawCount > 0) {
        uint32_t totalLiveVertices = 0;

        for (int k = 0; k < drawCount; k++) {
            Pixie* p = &g_pixies[drawOrder[k]];

            if (p->mode == PIXIE_MODE_LIVE) {
                totalLiveVertices += (uint32_t)p->liveVertexCount;
            }
        }

        if (totalLiveVertices > 0) {
            PixieLiveFrameResources* liveFrame = &g_pixie_live_frames[imageIndex];

            if (ensure_pixie_live_vertex_buffer(liveFrame, totalLiveVertices) == 0) {
                VkDevice dev = gdmf_get_device();
                PixieLiveVertex* vertices;

                if (vkMapMemory(dev, liveFrame->vertexMemory, 0, VK_WHOLE_SIZE, 0, (void**)&vertices) == VK_SUCCESS) {
                    uint32_t vertex_index = 0;

                    for (int k = 0; k < drawCount; k++) {
                        int id = drawOrder[k];
                        Pixie* p = &g_pixies[id];

                        if (p->mode != PIXIE_MODE_LIVE) { continue; }

                        g_pixie_vertex_offset[id]     = vertex_index;
                        g_pixie_live_vertex_count[id] = (uint32_t)p->liveVertexCount;
                        if (p->liveVertexCount > 0) {
                            memcpy(&vertices[vertex_index], p->liveVertices,
                                (size_t)p->liveVertexCount * sizeof(PixieLiveVertex));
                            vertex_index += (uint32_t)p->liveVertexCount;
                        }
                        g_pixie_drawn_this_frame[id] = true;
                    }
                    vkUnmapMemory(dev, liveFrame->vertexMemory);
                } else {
                    printf("[Pixies] Failed to map live vertex buffer\n");
                }
            }
        }
    }

    // Every Mode 1 pixie's accumulator is reset here, unconditionally --
    // regardless of whether it was drawn this frame (disabled/hidden live
    // pixies still shouldn't carry stale primitives forward) and
    // regardless of whether the copy above even ran (a pipeline hiccup
    // shouldn't let old primitives resurface later). "Nothing survives to
    // the next frame" has to be an unconditional guarantee, not one that
    // depends on rendering having gone smoothly this frame.
    for (int id = 0; id < MAX_PIXIES; id++) {
        if (g_pixies[id].mode == PIXIE_MODE_LIVE) {
            g_pixies[id].liveVertexCount = 0;
        }
    }

    return;
}

// Render hook for one priority band (called from the interleaved render
// loop in gdmf_vulkan.c). Unlike gdmf_sprites_record_band, this can't
// batch every pixie in the band into one draw call -- see the comment
// above g_pixie_vertex_offset/g_pixie_drawn_this_frame for why. A band
// can contain a mix of Mode 0 and Mode 1 pixies, each needing a different
// pipeline/vertex buffer (and Mode 0 additionally needs its own
// descriptor set rebound per pixie) -- pipeline/viewport/scissor/vertex-
// buffer are rebound only when the mode actually changes from the
// previous pixie drawn in this call, not unconditionally per pixie.
void gdmf_pixies_record_band(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t band) {
    if (band >= PIXIE_PRIORITY_BANDS) { return; }

    bool haveTexFrame  = g_pixie_pipeline_ready      && imageIndex < g_pixie_frame_count;
    bool haveLiveFrame = g_pixie_live_pipeline_ready && imageIndex < g_pixie_live_frame_count;
    PixieFrameResources*     texFrame  = haveTexFrame  ? &g_pixie_frames[imageIndex]      : NULL;
    PixieLiveFrameResources* liveFrame = haveLiveFrame ? &g_pixie_live_frames[imageIndex] : NULL;

    int lastBoundMode = -1;  // -1 = nothing bound yet this call

    for (int id = 0; id < MAX_PIXIES; id++) {
        if (!g_pixie_drawn_this_frame[id]) { continue; }
        if ((g_pixies[id].priority >> 4) != band) { continue; }

        PixieMode mode = g_pixies[id].mode;
        if (mode == PIXIE_MODE_TEXTURE && !texFrame)  { continue; }
        if (mode == PIXIE_MODE_LIVE    && !liveFrame) { continue; }

        if (lastBoundMode != (int)mode) {
            VkRect2D render_rect = gdmf_get_render_viewport_rect();
            VkViewport viewport = {
                .x = (float)render_rect.offset.x, .y = (float)render_rect.offset.y,
                .width = (float)render_rect.extent.width, .height = (float)render_rect.extent.height,
                .minDepth = 0.0f, .maxDepth = 1.0f
            };
            VkRect2D scissor = render_rect;

            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkBuffer     vertex_buffers[1];
            VkDeviceSize offsets[1] = { 0 };
            if (mode == PIXIE_MODE_TEXTURE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pixie_vk_pipeline);
                vertex_buffers[0] = texFrame->vertexBuffer;
            } else {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pixie_live_vk_pipeline);
                vertex_buffers[0] = liveFrame->vertexBuffer;
            }
            vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);

            lastBoundMode = (int)mode;
        }

        if (mode == PIXIE_MODE_TEXTURE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                g_pixie_vk_layout, 0, 1, &g_pixie_gpu[id].descriptorSet, 0, NULL);
            vkCmdDraw(cmd, 6, 1, g_pixie_vertex_offset[id], 0);
        } else {
            uint32_t count = g_pixie_live_vertex_count[id];

            if (count > 0) {
                vkCmdDraw(cmd, count, 1, g_pixie_vertex_offset[id], 0);
            }
        }
    }

    return;
}

// Tears down only what's genuinely tied to the render pass / swapchain
// image count, for both modes: each mode's pipeline (bakes in a specific
// VkRenderPass at creation) and per-swapchain-image vertex buffers (sized
// to the old image count). Deliberately narrower than
// cleanup_pixie_render_resources (full shutdown, where everything really
// does need to go) -- does NOT touch the descriptor pool/set layout/
// sampler or any per-pixie GPU resource (image/view/descriptor set),
// because none of those reference the render pass or image count at all;
// they stay perfectly valid across a swapchain recreation.
//
// This is why gdmf_sprites_on_swapchain_recreated can get away with
// reusing its full shutdown-style cleanup and pixies can't: a sprite's
// descriptor set lives in its per-frame resources (SpriteFrameResources),
// rebuilt from scratch either way with no per-instance state to lose. A
// pixie's descriptor set is its own, non-shared resource tied to that one
// pixie -- destroying the pool here would leave every live pixie holding
// a dangling descriptor set with gpu->ready still true and no code path
// that would ever notice and rebuild just that, since create_pixie_image
// is only called for pixies where ready is currently false.
static void cleanup_pixie_swapchain_dependent_resources(void) {
    VkDevice dev = gdmf_get_device();

    if (dev == VK_NULL_HANDLE) { return; }
    vkDeviceWaitIdle(dev);

    for (uint32_t i = 0; i < g_pixie_frame_count; i++) {
        PixieFrameResources* frame = &g_pixie_frames[i];

        if (frame->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
            frame->vertexBuffer = VK_NULL_HANDLE;
        }
        if (frame->vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->vertexMemory, NULL);
            frame->vertexMemory = VK_NULL_HANDLE;
        }
    }
    free(g_pixie_frames);
    g_pixie_frames      = NULL;
    g_pixie_frame_count = 0;

    if (g_pixie_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, g_pixie_vk_pipeline, NULL);
        g_pixie_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_pixie_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, g_pixie_vk_layout, NULL);
        g_pixie_vk_layout = VK_NULL_HANDLE;
    }
    g_pixie_pipeline_ready = false;

    for (uint32_t i = 0; i < g_pixie_live_frame_count; i++) {
        PixieLiveFrameResources* frame = &g_pixie_live_frames[i];

        if (frame->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(dev, frame->vertexBuffer, NULL);
            frame->vertexBuffer = VK_NULL_HANDLE;
        }
        if (frame->vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(dev, frame->vertexMemory, NULL);
            frame->vertexMemory = VK_NULL_HANDLE;
        }
    }
    free(g_pixie_live_frames);
    g_pixie_live_frames      = NULL;
    g_pixie_live_frame_count = 0;

    if (g_pixie_live_vk_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, g_pixie_live_vk_pipeline, NULL);
        g_pixie_live_vk_pipeline = VK_NULL_HANDLE;
    }
    if (g_pixie_live_vk_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, g_pixie_live_vk_layout, NULL);
        g_pixie_live_vk_layout = VK_NULL_HANDLE;
    }
    g_pixie_live_pipeline_ready = false;

    return;
}

void gdmf_pixies_on_swapchain_recreated(void) {
    cleanup_pixie_swapchain_dependent_resources();

    return;
}