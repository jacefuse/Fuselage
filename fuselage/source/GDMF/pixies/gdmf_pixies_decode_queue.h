#ifndef GDMF_PIXIES_DECODE_QUEUE_H
#define GDMF_PIXIES_DECODE_QUEUE_H

#include "gdmf_pixies_decode_requests_private.h"
#include "../compute/gdmf_compute.h"

#define GDMF_PIXIES_DECODE_MAX_REQUESTS 64u
#define GDMF_PIXIES_DECODE_ARENA_MAX (64u * 1024u * 1024u)

typedef struct {
    int pixie_id;
    GdmfKernelId kernel;
    union {
        GdmfDecodeRawPush raw;
        GdmfDecodeRlePush rle;
    } push;
    uint32_t push_bytes;
    uint32_t rows;
    const unsigned char* bytes;
    size_t byte_count;
    const unsigned char* palette;
    size_t palette_bytes;
} gdmf_pixies_decode_request;

// Serialized with commands and frame preparation. Visitor views last for the
// call only; visitors must not reenter or mutate this queue. False retains the
// request and all later work. True consumes it after copying/recording inputs.
typedef bool (*gdmf_pixies_decode_visitor)(void* context, const gdmf_pixies_decode_request* request);
size_t gdmf_pixies_decode_requests_drain(gdmf_pixies_decode_visitor visitor, void* context);
void gdmf_pixies_decode_requests_drop(int id);
// Stop producers first; recorded commands retain staging, not queue storage.
void gdmf_pixies_decode_requests_shutdown(void);

#endif
