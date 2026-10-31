#ifndef GDMF_COMPUTE_H
#define GDMF_COMPUTE_H

// Engine-owned kernels and backend-neutral resource descriptions.
// SEE: (GDMF_COMPUTE.md, Kernel table)

#include <stdbool.h>
#include <stdint.h>

#define GDMF_COMPUTE_VERSION "0.5.2026102501 ENTERON"

typedef enum {
    GDMF_KERNEL_DECODE_RAW = 0,
    GDMF_KERNEL_DECODE_RLE = 1,
    GDMF_KERNEL_DECODE_MASKTILE = 2,

    GDMF_KERNEL_COUNT
} GdmfKernelId;
typedef enum {
    GDMF_KERNEL_BIND_BYTES_PALETTE_IMAGE = 0,
    GDMF_KERNEL_BIND_BYTES_PROBS_PREV_IMAGE = 1,

    GDMF_KERNEL_BIND_KIND_COUNT
} GdmfKernelBindKind;

typedef struct {
    GdmfKernelId       id;
    const char*        name;
    GdmfKernelBindKind kind;
    uint32_t           push_bytes;
    uint32_t           group_width;
    uint32_t           group_height;
} GdmfKernelDescription;
// Returns NULL for an unknown ID; otherwise borrows an immutable table entry.
const GdmfKernelDescription* gdmf_kernel_desc(GdmfKernelId id);
// Ceiling workgroup counts. Requires a valid description and output pointers.
void gdmf_kernel_groups(const GdmfKernelDescription* description, uint32_t width, uint32_t height,
                        uint32_t* groups_x, uint32_t* groups_y);
#define GDMF_DECODE_RAW_FORMAT_RGBA8       0u
#define GDMF_DECODE_RAW_FORMAT_PALETTE4BPP 1u
#define GDMF_DECODE_RAW_FORMAT_PALETTE8BPP 5u

typedef struct {
    uint32_t source_offset; // Byte offset within the input buffer.
    uint32_t width;
    uint32_t height;
    int32_t  destination_x;
    int32_t  destination_y;
    uint32_t format;
    uint32_t palette_base; // First palette entry, measured in uint32_t values.
    uint32_t image_width;
    uint32_t image_height;
} GdmfDecodeRawPush;
typedef struct {
    uint32_t table_offset; // Byte offset; the row table must be four-byte aligned.
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t width;
    uint32_t height;
    int32_t  destination_x;
    int32_t  destination_y;
    uint32_t palette_base;
    uint32_t image_width;
    uint32_t image_height;
} GdmfDecodeRlePush;
typedef struct {
    uint32_t table_offset; // Byte offset of the dirty-tile table.
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t probability_base;
    uint32_t context_mask;
    uint32_t tile_size;
    uint32_t tiles_x;
    uint32_t tile_count;
    int32_t  destination_x;
    int32_t  destination_y;
    uint32_t image_width;
    uint32_t image_height;
} GdmfDecodeMaskTilePush;

typedef struct {
    uint64_t handle;
    uint64_t offset;
    uint64_t size;
} GdmfComputeBuffer;

typedef struct {
    uint64_t image;
    uint64_t view;
    uint32_t width;
    uint32_t height;
} GdmfComputeImage;

typedef struct GdmfComputeDispatch {
    GdmfKernelId      kernel;
    GdmfComputeBuffer bytes;
    GdmfComputeBuffer palette; // Probability table for the mask kernel.
    GdmfComputeImage  image;
    GdmfComputeImage  previous;
    uint32_t          groups_x;
    uint32_t          groups_y;
    const void*       push;
    uint32_t          push_bytes;
} GdmfComputeDispatch;
// Backend entry points; prepare/teardown are serialized by the renderer.
// Shutdown requires completed GPU work and discarded unsubmitted references.
bool gdmf_compute_backend_init(void);
void gdmf_compute_backend_shutdown(void);
// False on an invalid index, unavailable resources, or descriptor reset failure.
bool gdmf_compute_backend_begin_frame(uint32_t image_index);
// Required byte alignment for buffer offsets; conservative 256 before init.
uint32_t gdmf_compute_buffer_alignment(void);
// False when initialization or this frame's descriptor reset fails.
bool gdmf_compute_prepare(uint32_t image_index);
// True only after successful preparation of the current frame.
bool gdmf_compute_ready(void);

void gdmf_compute_on_swapchain_recreated(void);
void gdmf_compute_shutdown(void);

#endif // GDMF_COMPUTE_H
