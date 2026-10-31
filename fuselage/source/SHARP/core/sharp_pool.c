#include "sharp_facade_private.h"
#include <string.h>

void SHARP_SetPoolMB(int pool_mib) {
    if (sharp_facade_state.initialized) {
        return;
    }
    sharp_facade_state.pool_mib = pool_mib;
}

uint32_t SHARP_GetPoolSize(void) {
    return sharp_facade_state.pool_size;
}

bool SHARP_WritePool(uint32_t offset, const void* data, uint32_t bytes) {
    if (!sharp_facade_state.pool || !data) {
        return false;
    }

    if (offset > sharp_facade_state.pool_size || bytes > sharp_facade_state.pool_size - offset) {
        return false;
    }
    memcpy(sharp_facade_state.pool + offset, data, bytes);
    return true;
}
