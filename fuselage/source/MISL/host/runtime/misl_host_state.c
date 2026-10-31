/* One session for the current native host. */
#include "misl_host_state_internal.h"
#include <string.h>

/* Load preparation sets the initial pacing estimate before execution. */
MislHostSession fuselage_guest_session;

static void fuselage_guest_store_path(char* output, size_t capacity, const char* path) {
    if (!path || !*path) {
        output[0] = '\0';
        return;
    }
    size_t length = strlen(path);
    if (length >= capacity) {
        output[0] = '\0';
        return;
    }
    memmove(output, path, length + 1);
}

void FuselageSetGuestCaptureDirectory(const char* directory) {
    fuselage_guest_store_path(fuselage_guest_session.capture_directory,
                              sizeof fuselage_guest_session.capture_directory, directory);
}

void FuselageSetGuestProgramPath(const char* path) {
    fuselage_guest_store_path(fuselage_guest_session.program_path, sizeof fuselage_guest_session.program_path,
                              path);
}

void FuselageSetGuestMemoryCapMB(uint32_t memory_cap_mib) {
    fuselage_guest_session.memory_cap_mib = memory_cap_mib;
}

uint64_t fuselage_guest_get_step_count(void) {
    return fuselage_guest_session.steps_executed;
}
uint64_t FuselageGetGuestStepCount(void) {
    return fuselage_guest_get_step_count();
}
