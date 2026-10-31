/* Native executable path and no-replacement publication. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../core/sram_internal.h"
#include <limits.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

bool SRAM_GetProgramPath(char* output, size_t capacity, const char* argv0) {
    if (!output || !capacity) {
        return false;
    }
    output[0] = '\0';
#if defined(_WIN32)
    if (capacity <= UINT32_MAX) {
        DWORD length = GetModuleFileNameA(NULL, output, (DWORD)capacity);
        if (length && length < capacity) {
            return true;
        }
    }
#elif defined(__APPLE__)
    if (capacity <= UINT32_MAX) {
        uint32_t required = (uint32_t)capacity;
        if (_NSGetExecutablePath(output, &required) == 0) {
            return true;
        }
    }
#else
    if (capacity > 1) {
        ssize_t length = readlink("/proc/self/exe", output, capacity - 1);
        if (length > 0 && (size_t)length < capacity - 1) {
            output[length] = '\0';
            return true;
        }
    }
#endif
    output[0] = '\0';
    if (argv0 && *argv0 && strlen(argv0) < capacity) {
        memmove(output, argv0, strlen(argv0) + 1);
        return true;
    }
    return false;
}

bool sram_publish_new(const char* staging_path, const char* destination_path) {
#if defined(_WIN32)
    return MoveFileExA(staging_path, destination_path, MOVEFILE_WRITE_THROUGH) != 0;
#else
    /* link refuses an existing destination; staging stays in the same directory. */
    return link(staging_path, destination_path) == 0;
#endif
}
