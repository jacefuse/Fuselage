// Sweep candidates, parallel pair tests, then results in caller-list order.
// SEE: (GDMF_SPRITES.md, Batch queries)
#include "gdmf_sprites_collision_private.h"
#include "../workers/gdmf_workers.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t first_position, second_position;
    unsigned char types;
} gdmf_sprite_candidate;

_Static_assert(MAX_SPRITES - 1 <= INT16_MAX, "SpritePairRef must represent every sprite index");
_Static_assert((size_t)MAX_SPRITES*(MAX_SPRITES - 1) / 2 <= INT_MAX,
               "Pair counts must fit the public int result");

static int gdmf_sprites_search_indices[MAX_SPRITES];
static int gdmf_sprites_search_order[MAX_SPRITES];
static gdmf_sprite_bounds gdmf_sprites_search_bounds[MAX_SPRITES];
static uint32_t gdmf_sprites_search_seen[MAX_SPRITES], gdmf_sprites_search_stamp;
static gdmf_sprite_candidate* gdmf_sprites_candidates;
static size_t gdmf_sprites_candidate_capacity;

static int gdmf_sprites_compare_minimum_x(const void* first, const void* second) {
    int first_position = *(const int*)first, second_position = *(const int*)second;
    float first_x = gdmf_sprites_search_bounds[first_position].minimum_x;
    float second_x = gdmf_sprites_search_bounds[second_position].minimum_x;
    if (first_x < second_x) {
        return -1;
    }
    if (first_x > second_x) {
        return 1;
    }
    return (first_position > second_position) - (first_position < second_position);
}

static int gdmf_sprites_compare_candidates(const void* first, const void* second) {
    const gdmf_sprite_candidate* first_pair = first;
    const gdmf_sprite_candidate* second_pair = second;
    if (first_pair->first_position != second_pair->first_position) {
        return (first_pair->first_position > second_pair->first_position) -
               (first_pair->first_position < second_pair->first_position);
    }
    return (first_pair->second_position > second_pair->second_position) -
           (first_pair->second_position < second_pair->second_position);
}

static bool gdmf_sprites_candidate_add(int* count, int first_position, int second_position) {
    size_t needed = (size_t)*count + 1;
    if (needed > gdmf_sprites_candidate_capacity) {
        size_t capacity = gdmf_sprites_candidate_capacity ? gdmf_sprites_candidate_capacity : 4096;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2) {
                return false;
            }
            capacity *= 2;
        }
        if (capacity > SIZE_MAX / sizeof(gdmf_sprite_candidate)) {
            return false;
        }
        gdmf_sprite_candidate* replacement =
            realloc(gdmf_sprites_candidates, capacity * sizeof(*replacement));
        if (!replacement) {
            return false;
        }
        gdmf_sprites_candidates = replacement;
        gdmf_sprites_candidate_capacity = capacity;
    }
    int first = first_position < second_position ? first_position : second_position;
    int second = first_position < second_position ? second_position : first_position;
    gdmf_sprites_candidates[*count] = (gdmf_sprite_candidate){(uint16_t)first, (uint16_t)second, 0};
    ++*count;
    return true;
}

static void gdmf_sprites_candidate_work(int begin, int end, void* context) {
    unsigned char types = *(const unsigned char*)context;
    for (int pair = begin; pair < end; ++pair) {
        gdmf_sprite_candidate* candidate = &gdmf_sprites_candidates[pair];
        candidate->types =
            CheckSpritePairCollision(gdmf_sprites_search_indices[candidate->first_position],
                                     gdmf_sprites_search_indices[candidate->second_position], types);
    }
}

int FindSpriteCollisionPairs(const short* sprite_indices, int count, unsigned char types_to_test,
                             SpritePairRef* output_pairs, int capacity, unsigned char* output_types) {
    if (!sprite_indices || count <= 0) {
        return 0;
    }
    if (capacity < 0) {
        capacity = 0;
    }
    if (++gdmf_sprites_search_stamp == 0) {
        memset(gdmf_sprites_search_seen, 0, sizeof(gdmf_sprites_search_seen));
        gdmf_sprites_search_stamp = 1;
    }
    int unique_count = 0;
    for (int position = 0; position < count; ++position) {
        int sprite_index = sprite_indices[position];
        if (!gdmf_sprites_index_valid(sprite_index) ||
            gdmf_sprites_search_seen[sprite_index] == gdmf_sprites_search_stamp) {
            continue;
        }
        gdmf_sprites_search_seen[sprite_index] = gdmf_sprites_search_stamp;
        const gdmf_sprite_bounds* bounds = gdmf_sprites_bounds_get(sprite_index);
        // Pair tests reject nonfinite bounds too, so they need no sweep candidates.
        if (!gdmf_sprites_bounds_finite(bounds)) {
            continue;
        }
        gdmf_sprites_search_indices[unique_count] = sprite_index;
        gdmf_sprites_search_bounds[unique_count] = *bounds;
        gdmf_sprites_search_order[unique_count] = unique_count;
        ++unique_count;
    }
    if (unique_count < 2) {
        return 0;
    }
    qsort(gdmf_sprites_search_order, (size_t)unique_count, sizeof(gdmf_sprites_search_order[0]),
          gdmf_sprites_compare_minimum_x);
    double estimate = 0.0;
    int candidate_count = 0;
    for (int first = 0; first < unique_count; ++first) {
        int first_position = gdmf_sprites_search_order[first];
        const gdmf_sprite_bounds* first_bounds = &gdmf_sprites_search_bounds[first_position];
        for (int second = first + 1; second < unique_count; ++second) {
            int second_position = gdmf_sprites_search_order[second];
            const gdmf_sprite_bounds* second_bounds = &gdmf_sprites_search_bounds[second_position];
            if (second_bounds->minimum_x >= first_bounds->maximum_x) {
                break;
            }
            if (first_bounds->minimum_x >= second_bounds->maximum_x ||
                first_bounds->minimum_y >= second_bounds->maximum_y ||
                first_bounds->maximum_y <= second_bounds->minimum_y) {
                continue;
            }
            if (!gdmf_sprites_candidate_add(&candidate_count, first_position, second_position)) {
                return -1;
            }
            estimate += (double)(fminf(first_bounds->maximum_x, second_bounds->maximum_x) -
                                 fmaxf(first_bounds->minimum_x, second_bounds->minimum_x)) *
                        (double)(fminf(first_bounds->maximum_y, second_bounds->maximum_y) -
                                 fmaxf(first_bounds->minimum_y, second_bounds->minimum_y));
        }
    }
    if (!candidate_count) {
        return 0;
    }
    if (estimate < GDMF_SPRITES_PAIR_INLINE_PIXELS) {
        gdmf_sprites_candidate_work(0, candidate_count, &types_to_test);
    } else {
        int chunk = candidate_count / (gdmf_workers_threads() * 8);
        if (chunk < 1) {
            chunk = 1;
        }
        if (chunk > 64) {
            chunk = 64;
        }
        gdmf_workers_run(candidate_count, chunk, gdmf_sprites_candidate_work, &types_to_test);
    }
    int hits = 0;
    for (int pair = 0; pair < candidate_count; ++pair) {
        if (gdmf_sprites_candidates[pair].types) {
            gdmf_sprites_candidates[hits++] = gdmf_sprites_candidates[pair];
        }
    }
    if (hits > 1) {
        qsort(gdmf_sprites_candidates, (size_t)hits, sizeof(gdmf_sprites_candidates[0]),
              gdmf_sprites_compare_candidates);
    }
    int written = hits < capacity ? hits : capacity;
    for (int pair = 0; pair < written; ++pair) {
        const gdmf_sprite_candidate* candidate = &gdmf_sprites_candidates[pair];
        if (output_pairs) {
            output_pairs[pair].a = (short)gdmf_sprites_search_indices[candidate->first_position];
            output_pairs[pair].b = (short)gdmf_sprites_search_indices[candidate->second_position];
        }
        if (output_types) {
            output_types[pair] = candidate->types;
        }
    }
    return hits;
}

void gdmf_sprites_collision_search_shutdown(void) {
    free(gdmf_sprites_candidates);
    gdmf_sprites_candidates = NULL;
    gdmf_sprites_candidate_capacity = 0;
}
