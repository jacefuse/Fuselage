/* Own file bytes until execution mappings are detached. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#endif
#include "misl_launcher_internal.h"
#include "../../../SRAM/sram.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

bool fuselage_launcher_default_path(char* output, size_t capacity, const char* argv0) {
    char executable[1024];
    if (!SRAM_GetProgramPath(executable, sizeof executable, argv0)) {
        return false;
    }
    size_t basename = strlen(executable);
    while (basename && executable[basename - 1] != '/' && executable[basename - 1] != '\\') {
        basename--;
    }
    if (!basename) { return false; }
    const char filename[] = "GAME.ROAP";
    if (basename >= capacity || sizeof filename > capacity - basename) {
        return false;
    }
    memcpy(output, executable, basename);
    memcpy(output + basename, filename, sizeof filename);
    return true;
}
bool fuselage_launcher_load(const char* path) {
    if (!path || !*path || strlen(path) >= sizeof fuselage_launcher.path) {
        fprintf(stderr, "MISLlauncher: missing or overlong ROAP path.\n");
        return false;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        fuselage_launcher.load_error = errno;
        fprintf(stderr, "MISLlauncher: cannot open '%s'.\n", path);
        return false;
    }
#if defined(_WIN32)
    bool sized = _fseeki64(file, 0, SEEK_END) == 0;
    int64_t length = sized ? _ftelli64(file) : -1;
    bool rewound = _fseeki64(file, 0, SEEK_SET) == 0;
#else
    bool sized = fseeko(file, 0, SEEK_END) == 0;
    int64_t length = sized ? (int64_t)ftello(file) : -1;
    bool rewound = fseeko(file, 0, SEEK_SET) == 0;
#endif
    if (!rewound || length <= 0 || (uint64_t)length > FUSELAGE_GUEST_MAX_FILE_BYTES ||
        (uint64_t)length > SIZE_MAX) {
        fclose(file);
        fprintf(stderr, "MISLlauncher: empty, unreadable or oversized ROAP '%s'.\n", path);
        return false;
    }
    uint8_t* bytes = malloc((size_t)length);
    if (!bytes) {
        fclose(file);
        fprintf(stderr, "MISLlauncher: cannot allocate ROAP file storage.\n");
        return false;
    }
    size_t loaded = fread(bytes, 1, (size_t)length, file);
    bool read_failed = ferror(file) != 0;
    int close_result = fclose(file);
    if (loaded != (size_t)length || read_failed || close_result) {
        free(bytes);
        fprintf(stderr, "MISLlauncher: failed to read complete ROAP '%s'.\n", path);
        return false;
    }
    MislGuestImage image = {bytes, (uint32_t)length, 0};
    MislHostImagePlan plan;
    MislHostImageResult result = fuselage_guest_plan_image(&image, fuselage_launcher.memory_cap_mib, &plan);
    if (result != MISL_HOST_IMAGE_OK) {
        free(bytes);
        fprintf(stderr, "MISLlauncher: %s.\n", fuselage_guest_get_image_result_message(result));
        return false;
    }
    fuselage_launcher.bytes = bytes;
    fuselage_launcher.image = image;
    fuselage_launcher.plan = plan;
    memcpy(fuselage_launcher.path, path, strlen(path) + 1);
    return true;
}
