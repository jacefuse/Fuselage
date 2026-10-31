/* Resident save storage. SEE: (SRAM.md, Lifecycle) */
#include "sram_internal.h"
#include <stdlib.h>
#include <string.h>

SramState sram_state;
void sram_set_failure(const char* reason) {
    char copy[sizeof sram_state.reason];
    snprintf(copy, sizeof copy, "%s", reason);
    memcpy(sram_state.reason, copy, strlen(copy) + 1);
}
void SRAM_Close(void) {
    free(sram_state.data);
    sram_state.data = NULL;
    sram_state.open = false;
    sram_state.abandoned = false;
    sram_state.dirty = false;
}
bool SRAM_Configure(const char* id, uint32_t version, uint32_t byte_count) {
    char copied_id[SRAM_ID_BYTES] = {0};
    size_t length = 0;
    if (byte_count && id) {
        while (length < SRAM_ID_BYTES && id[length]) {
            copied_id[length] = id[length];
            length++;
        }
    }
    bool overlong = byte_count && id && length == SRAM_ID_BYTES && id[length];
    SRAM_Close();
    memset(sram_state.id, 0, sizeof sram_state.id);
    sram_state.byte_count = byte_count;
    sram_state.version = version;
    sram_state.configured = false;
    sram_state.path[0] = '\0';
    sram_state.reason[0] = '\0';
    if (!byte_count) {
        return true;
    }
    if (!length) {
        sram_set_failure("The program declares SRAM but no save id.");
        return false;
    }
    if (overlong) {
        sram_set_failure("The program's save id is longer than 16 bytes.");
        return false;
    }
    memcpy(sram_state.id, copied_id, sizeof copied_id);
    sram_state.configured = true;
    return true;
}
bool SRAM_SetSaveDirectory(const char* directory) {
    size_t length = directory ? strlen(directory) : 0;
    if (length >= sizeof sram_state.directory) {
        sram_set_failure("The save directory path is too long.");
        return false;
    }
    if (length) {
        memmove(sram_state.directory, directory, length + 1);
    } else {
        sram_state.directory[0] = '\0';
    }
    return true;
}
uint32_t SRAM_GetSize(void) {
    return sram_state.open ? sram_state.byte_count : 0;
}
bool SRAM_IsEnabled(void) {
    return sram_state.open;
}
bool SRAM_IsDirty(void) {
    return sram_state.open && sram_state.dirty;
}
const char* SRAM_GetFailureReason(void) {
    return sram_state.reason;
}
const char* SRAM_GetPath(void) {
    return sram_state.path;
}
void SRAM_Abandon(void) {
    sram_state.abandoned = true;
}
const uint8_t* sram_get_host_span(uint32_t* byte_count) {
    if (byte_count) {
        *byte_count = SRAM_GetSize();
    }
    return sram_state.open ? sram_state.data : NULL;
}
static bool sram_in_range(uint32_t offset, uint32_t byte_count) {
    return sram_state.open && byte_count && byte_count <= sram_state.byte_count &&
           offset <= sram_state.byte_count - byte_count;
}
bool SRAM_Read(uint32_t offset, void* output, uint32_t byte_count) {
    if (!output || !sram_in_range(offset, byte_count)) {
        return false;
    }
    memmove(output, sram_state.data + offset, byte_count);
    return true;
}
bool SRAM_Write(uint32_t offset, const void* input, uint32_t byte_count) {
    if (!input || !sram_in_range(offset, byte_count)) {
        return false;
    }
    memmove(sram_state.data + offset, input, byte_count);
    sram_state.dirty = true;
    return true;
}
bool SRAM_Save(void) {
    if (!sram_state.open || sram_state.abandoned || !sram_state.dirty) {
        return true;
    }
    if (!sram_write_file(false)) {
        fprintf(stderr, "[SRAM] Could not publish %s; the working copy remains dirty.\n", sram_state.path);
        return false;
    }
    sram_state.dirty = false;
    sram_state.reason[0] = '\0';
    return true;
}
