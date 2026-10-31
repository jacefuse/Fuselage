// Command-time input snapshots. SEE: (GDMF_PIXIES_FORMATS.md, GPU decode requests)
#include "gdmf_pixies_decode_queue.h"
#include "gdmf_pixies_codec_private.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    gdmf_pixies_decode_request request;
    size_t bytes_offset, palette_offset;
} gdmf_pixies_queued_decode;

static gdmf_pixies_queued_decode gdmf_pixies_requests[GDMF_PIXIES_DECODE_MAX_REQUESTS];
static size_t gdmf_pixies_request_count;
static unsigned char* gdmf_pixies_decode_arena;
static size_t gdmf_pixies_decode_used, gdmf_pixies_decode_capacity;

_Static_assert(GDMF_DECODE_RAW_FORMAT_RGBA8 == PIXIE_FORMAT_RGBA8, "raw RGBA format");
_Static_assert(GDMF_DECODE_RAW_FORMAT_PALETTE4BPP == PIXIE_FORMAT_PALETTE4BPP, "raw four-bit format");
_Static_assert(GDMF_DECODE_RAW_FORMAT_PALETTE8BPP == PIXIE_FORMAT_PALETTE8BPP, "raw eight-bit format");

static int gdmf_pixies_decode_target(const gdmf_pixies_state* state, int width, int height) {
    if (!state || state->mode != PIXIE_MODE_TEXTURE || width <= 0 || height <= 0 ||
        width > GDMF_PIXIES_MAX_UNPACK_DIM || height > GDMF_PIXIES_MAX_UNPACK_DIM)
        return -1;
    // Avoid pointer subtraction on a caller-supplied pointer outside the state array.
    for (int id = 0; id < MAX_PIXIES; id++)
        if (gdmf_pixies_state_get(id) == state)
            return id;
    return -1;
}

static bool gdmf_pixies_decode_reserve(size_t bytes) {
    if (bytes > GDMF_PIXIES_DECODE_ARENA_MAX - gdmf_pixies_decode_used)
        return false;
    size_t needed = gdmf_pixies_decode_used + bytes;
    if (needed <= gdmf_pixies_decode_capacity)
        return true;
    size_t capacity = gdmf_pixies_decode_capacity ? gdmf_pixies_decode_capacity : 65536;
    while (capacity < needed)
        capacity *= 2;
    unsigned char* grown = realloc(gdmf_pixies_decode_arena, capacity);
    if (!grown)
        return false;
    gdmf_pixies_decode_arena = grown;
    gdmf_pixies_decode_capacity = capacity;
    return true;
}

static bool gdmf_pixies_decode_enqueue(gdmf_pixies_decode_request* request, const unsigned char* first,
                                       size_t first_bytes, const unsigned char* second, size_t second_bytes,
                                       const Color* palette, uint32_t palette_count) {
    if (!gdmf_compute_ready() || gdmf_pixies_request_count == GDMF_PIXIES_DECODE_MAX_REQUESTS || !first ||
        (second_bytes && !second) || palette_count > 256 || (palette_count && !palette))
        return false;
    if (first_bytes > GDMF_PIXIES_DECODE_ARENA_MAX ||
        second_bytes > GDMF_PIXIES_DECODE_ARENA_MAX - first_bytes)
        return false;
    size_t bytes = first_bytes + second_bytes;
    if (!bytes)
        return false;
    size_t rounded_bytes = (bytes + 3u) & ~(size_t)3u;
    size_t palette_bytes = (size_t)palette_count * sizeof(uint32_t);
    if (palette_bytes > GDMF_PIXIES_DECODE_ARENA_MAX - rounded_bytes ||
        !gdmf_pixies_decode_reserve(rounded_bytes + palette_bytes))
        return false;
    gdmf_pixies_queued_decode pending = {0};
    pending.request = *request;
    pending.request.byte_count = rounded_bytes;
    pending.request.palette_bytes = palette_bytes;
    pending.bytes_offset = gdmf_pixies_decode_used;
    pending.palette_offset = pending.bytes_offset + rounded_bytes;
    memcpy(gdmf_pixies_decode_arena + pending.bytes_offset, first, first_bytes);
    if (second_bytes)
        memcpy(gdmf_pixies_decode_arena + pending.bytes_offset + first_bytes, second, second_bytes);
    memset(gdmf_pixies_decode_arena + pending.bytes_offset + bytes, 0, rounded_bytes - bytes);
    for (uint32_t entry = 0; entry < palette_count; entry++) {
        uint32_t packed = PackColorsRGBA8(palette[entry]);
        memcpy(gdmf_pixies_decode_arena + pending.palette_offset + entry * 4u, &packed, sizeof packed);
    }
    gdmf_pixies_requests[gdmf_pixies_request_count++] = pending;
    gdmf_pixies_decode_used += rounded_bytes + palette_bytes;
    return true;
}

bool gdmf_pixies_decode_on_gpu(gdmf_pixies_state* state, uint32_t format, const unsigned char* data,
                               size_t length, int width, int height, int destination_x, int destination_y,
                               const Color* palette, uint32_t palette_count) {
    int id = gdmf_pixies_decode_target(state, width, height);
    if (id < 0)
        return false;
    size_t pixels = (size_t)width * (size_t)height;
    size_t required;
    uint32_t required_palette;
    switch (format) {
    case PIXIE_FORMAT_RGBA8:
        required = pixels * 4u;
        required_palette = 0;
        break;
    case PIXIE_FORMAT_PALETTE4BPP:
        required = (pixels + 1u) / 2u;
        required_palette = 16;
        break;
    case PIXIE_FORMAT_PALETTE8BPP:
        required = pixels;
        required_palette = 256;
        break;
    default:
        return false;
    }
    if (length != required || palette_count != required_palette)
        return false;
    gdmf_pixies_decode_request request = {
        .pixie_id = id, .kernel = GDMF_KERNEL_DECODE_RAW, .push_bytes = sizeof(GdmfDecodeRawPush)};
    request.push.raw = (GdmfDecodeRawPush){.width = (uint32_t)width,
                                           .height = (uint32_t)height,
                                           .destination_x = destination_x,
                                           .destination_y = destination_y,
                                           .format = format,
                                           .image_width = (uint32_t)state->output_width,
                                           .image_height = (uint32_t)state->output_height};
    if (!gdmf_pixies_decode_enqueue(&request, data, length, NULL, 0, palette, palette_count))
        return false;
    state->gpu_resident = true;
    return true;
}

bool gdmf_pixies_decode_rle_on_gpu(gdmf_pixies_state* state, const unsigned char* row_table,
                                   const unsigned char* data, size_t length, int width, int height,
                                   int destination_x, int destination_y, const Color* palette,
                                   uint32_t palette_count) {
    int id = gdmf_pixies_decode_target(state, width, height);
    if (id < 0 || !data || !length || length > PIXIE_RAM_SIZE ||
        (palette_count != 16 && palette_count != 256))
        return false;
    size_t table_bytes = (size_t)height * 4u;
    gdmf_pixies_decode_request request = {.pixie_id = id,
                                          .kernel = GDMF_KERNEL_DECODE_RLE,
                                          .push_bytes = sizeof(GdmfDecodeRlePush),
                                          .rows = (uint32_t)height};
    request.push.rle = (GdmfDecodeRlePush){.data_offset = (uint32_t)table_bytes,
                                           .data_length = (uint32_t)length,
                                           .width = (uint32_t)width,
                                           .height = (uint32_t)height,
                                           .destination_x = destination_x,
                                           .destination_y = destination_y,
                                           .image_width = (uint32_t)state->output_width,
                                           .image_height = (uint32_t)state->output_height};
    if (!gdmf_pixies_decode_enqueue(&request, row_table, table_bytes, data, length, palette, palette_count))
        return false;
    state->gpu_resident = true;
    return true;
}

static void gdmf_pixies_decode_compact(void) {
    size_t used = 0;
    for (size_t index = 0; index < gdmf_pixies_request_count; index++) {
        gdmf_pixies_queued_decode* queued = &gdmf_pixies_requests[index];
        size_t bytes = queued->request.byte_count + queued->request.palette_bytes;
        memmove(gdmf_pixies_decode_arena + used, gdmf_pixies_decode_arena + queued->bytes_offset, bytes);
        queued->bytes_offset = used;
        queued->palette_offset = used + queued->request.byte_count;
        used += bytes;
    }
    gdmf_pixies_decode_used = used;
}

size_t gdmf_pixies_decode_requests_drain(gdmf_pixies_decode_visitor visitor, void* context) {
    if (!visitor)
        return 0;
    size_t consumed = 0;
    while (consumed < gdmf_pixies_request_count) {
        const gdmf_pixies_queued_decode* queued = &gdmf_pixies_requests[consumed];
        gdmf_pixies_decode_request view = queued->request;
        view.bytes = gdmf_pixies_decode_arena + queued->bytes_offset;
        view.palette = view.palette_bytes ? gdmf_pixies_decode_arena + queued->palette_offset : NULL;
        if (!visitor(context, &view))
            break;
        consumed++;
    }
    if (consumed) {
        gdmf_pixies_request_count -= consumed;
        memmove(gdmf_pixies_requests, gdmf_pixies_requests + consumed,
                gdmf_pixies_request_count * sizeof gdmf_pixies_requests[0]);
        gdmf_pixies_decode_compact();
    }
    return consumed;
}

void gdmf_pixies_decode_requests_drop(int id) {
    size_t retained = 0;
    for (size_t index = 0; index < gdmf_pixies_request_count; index++) {
        if (gdmf_pixies_requests[index].request.pixie_id != id)
            gdmf_pixies_requests[retained++] = gdmf_pixies_requests[index];
    }
    gdmf_pixies_request_count = retained;
    gdmf_pixies_decode_compact();
}

void gdmf_pixies_decode_requests_shutdown(void) {
    free(gdmf_pixies_decode_arena);
    gdmf_pixies_decode_arena = NULL;
    gdmf_pixies_decode_used = gdmf_pixies_decode_capacity = gdmf_pixies_request_count = 0;
}
