// Caller-selected pairs, with all geometry caches warmed before parallel reads.
#include "gdmf_sprites_collision_private.h"
#include "../workers/gdmf_workers.h"

typedef struct {
    const SpritePairRef* pairs;
    unsigned char types;
    unsigned char* results;
} gdmf_sprite_pair_batch;

static void gdmf_sprites_pair_batch_work(int begin, int end, void* context) {
    const gdmf_sprite_pair_batch* batch = context;
    for (int pair = begin; pair < end; ++pair) {
        batch->results[pair] =
            CheckSpritePairCollision(batch->pairs[pair].a, batch->pairs[pair].b, batch->types);
    }
}

int CheckSpritePairCollisionBatch(const SpritePairRef* pairs, int count, unsigned char types_to_test,
                                  unsigned char* results) {
    if (!pairs || !results || count <= 0) {
        return 0;
    }
    double estimate = 0.0;
    for (int pair = 0; pair < count; ++pair) {
        int first = pairs[pair].a, second = pairs[pair].b;
        estimate += 8.0;
        if (!gdmf_sprites_index_valid(first) || !gdmf_sprites_index_valid(second) || first == second) {
            continue;
        }
        const gdmf_sprite_bounds* first_bounds = gdmf_sprites_bounds_get(first);
        const gdmf_sprite_bounds* second_bounds = gdmf_sprites_bounds_get(second);
        float width = fminf(first_bounds->maximum_x, second_bounds->maximum_x) -
                      fmaxf(first_bounds->minimum_x, second_bounds->minimum_x);
        float height = fminf(first_bounds->maximum_y, second_bounds->maximum_y) -
                       fmaxf(first_bounds->minimum_y, second_bounds->minimum_y);
        if (width > 0 && height > 0) {
            estimate += (double)width * (double)height;
        }
    }
    gdmf_sprite_pair_batch batch = {pairs, types_to_test, results};
    if (estimate < GDMF_SPRITES_PAIR_INLINE_PIXELS) {
        gdmf_sprites_pair_batch_work(0, count, &batch);
    } else {
        int chunk = count / (gdmf_workers_threads() * 8);
        if (chunk < 1) {
            chunk = 1;
        }
        if (chunk > 64) {
            chunk = 64;
        }
        gdmf_workers_run(count, chunk, gdmf_sprites_pair_batch_work, &batch);
    }
    int matched = 0;
    for (int pair = 0; pair < count; ++pair) {
        if (results[pair]) {
            ++matched;
        }
    }
    return matched;
}
