// Synchronous batch dispatch. SEE: (GDMF_INTERACTIONS.md, Batches)
#include "gdmf_interactions_private.h"
#include "../workers/gdmf_workers.h"
#define GDMF_INTERACTIONS_INLINE_PIXELS 32768.0

typedef struct {
    const short* sprites;
    unsigned char layer, types;
    unsigned char* results;
} gdmf_interactions_batch;

static void gdmf_interactions_batch_work(int begin, int end, void* context) {
    const gdmf_interactions_batch* batch = (const gdmf_interactions_batch*)context;
    for (int index = begin; index < end; index++) {
        batch->results[index] =
            CheckInteractionsSpriteTileCollision(batch->sprites[index], batch->layer, batch->types);
    }
}

int CheckInteractionsSpriteTileCollisionBatch(const short* sprites, int count, unsigned char layer,
                                              unsigned char types_to_test, unsigned char* results) {
    if (!sprites || !results || count <= 0) {
        return 0;
    }

    (void)gdmf_interactions_refuses_rotation(layer, "CheckInteractionsSpriteTileCollisionBatch");
    double estimated_pixels = 0.0;
    for (int index = 0; index < count; index++) {
        float minimum_x, minimum_y, maximum_x, maximum_y;
        estimated_pixels += 8.0;
        if (GetSpriteBoundingBox(sprites[index], &minimum_x, &minimum_y, &maximum_x, &maximum_y)) {
            estimated_pixels += ((double)maximum_x - minimum_x) * ((double)maximum_y - minimum_y);
        }
    }

    gdmf_interactions_batch batch = {sprites, layer, types_to_test, results};
    if (estimated_pixels < GDMF_INTERACTIONS_INLINE_PIXELS) {
        gdmf_interactions_batch_work(0, count, &batch);
    } else {
        int chunk_size = count / (gdmf_workers_threads() * 8);
        if (chunk_size < 1) {
            chunk_size = 1;
        }
        if (chunk_size > 32) {
            chunk_size = 32;
        }
        gdmf_workers_run(count, chunk_size, gdmf_interactions_batch_work, &batch);
    }

    int matched = 0;
    for (int index = 0; index < count; index++) {
        if (results[index]) {
            matched++;
        }
    }

    return matched;
}
