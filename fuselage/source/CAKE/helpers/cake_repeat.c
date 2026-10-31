// Poll-based key repeat. SEE: (CAKE.md, Key repeat)
#include "../cake_help.h"

static uint64_t cake_key_held_polls[CAKE_KEY_TABLE_SIZE];
static uint64_t cake_last_repeat_poll = 0;
static bool cake_repeat_ready = false;

// Advance only once per observed poll, regardless of the queried key.
static void cake_sync_key_repeat(void) {
    if (cake_repeat_ready && cake_last_repeat_poll == CAKE_PollCount) {
        return;
    }

    for (int key_index = 0; key_index < CAKE_KEY_TABLE_SIZE; key_index++) {
        if (!CAKE_Keys[key_index] || CAKE_KeysPressed[key_index]) {
            cake_key_held_polls[key_index] = 0;
        } else {
            cake_key_held_polls[key_index]++;
        }
    }

    cake_repeat_ready = true;
    cake_last_repeat_poll = CAKE_PollCount;

    return;
}

bool CAKE_CheckKeyRepeat(uint8_t key_code, unsigned delay_polls, unsigned interval_polls) {
    cake_sync_key_repeat();

    // A complete press/release within one poll still reports the press edge.
    if (CAKE_KeysPressed[key_code]) {
        return true;
    }
    if (!CAKE_Keys[key_code]) {
        return false;
    }

    uint64_t held_polls = cake_key_held_polls[key_code];
    if (held_polls < delay_polls) {
        return false;
    }
    if (interval_polls == 0) {
        return true;
    }

    return ((held_polls - delay_polls) % interval_polls) == 0;
}
