/* Load validation and staged file replacement. SEE: (SRAM.md, Disk writes) */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../core/sram_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

static bool sram_is_regular_file(const struct stat* entry) {
#if defined(_WIN32)
    return (entry->st_mode & _S_IFMT) == _S_IFREG;
#else
    return S_ISREG(entry->st_mode);
#endif
}

static void sram_format_id(char output[SRAM_ID_BYTES + 1], const uint8_t* id) {
    unsigned length = 0;
    while (length < SRAM_ID_BYTES && id[length]) {
        output[length] = id[length] >= 32 && id[length] < 127 ? (char)id[length] : '?';
        length++;
    }
    output[length] = '\0';
}

bool SRAM_Open(const char* program_path) {
    /* GetPath may be passed as input; keep it alive across Close and path updates. */
    char copied_path[SRAM_FILE_PATH_CAPACITY];
    if (program_path && strlen(program_path) < sizeof copied_path) {
        memmove(copied_path, program_path, strlen(program_path) + 1);
        program_path = copied_path;
    }
    SRAM_Close();
    sram_state.reason[0] = '\0';
    sram_state.path[0] = '\0';
    if (!sram_state.configured) {
        if (!sram_state.byte_count) {
            return true;
        }
        sram_set_failure("The SRAM declaration is invalid.");
        return false;
    }
    if (!sram_build_path(sram_state.path, sizeof sram_state.path, program_path, ".save") ||
        !sram_build_path(sram_state.previous_path, sizeof sram_state.previous_path, program_path, ".prev")) {
        sram_set_failure("The program's path is too long to name a save beside it.");
        return false;
    }
    FILE* file = fopen(sram_state.path, "rb");
    int open_error = errno;
    if (!file && open_error != ENOENT) {
        sram_set_failure("The save file could not be opened for reading.");
        return false;
    }
    if (!file) {
        struct stat previous;
        if (stat(sram_state.previous_path, &previous) == 0) {
            sram_set_failure("The main save is missing; a previous save is available for recovery.");
            return false;
        }
        if (errno != ENOENT) {
            sram_set_failure("The previous save could not be inspected.");
            return false;
        }
    }
    sram_state.data = calloc(sram_state.byte_count, 1);
    if (!sram_state.data) {
        if (file) {
            fclose(file);
        }
        sram_set_failure("The declared SRAM size could not be allocated.");
        return false;
    }
    if (!file) {
        sram_state.created = sram_get_utc_seconds();
        if (!sram_write_file(true)) {
            SRAM_Close();
            return false;
        }
        sram_state.open = true;
        return true;
    }
    uint8_t header[SRAM_HEADER_BYTES];
    size_t header_bytes = fread(header, 1, sizeof header, file);
    char reason[160] = {0};
    bool valid = false;
    if (header_bytes < sizeof header) {
        snprintf(reason, sizeof reason, "The file is %zu bytes, shorter than a save header.", header_bytes);
    } else if (memcmp(header, "FSAV", 4)) {
        snprintf(reason, sizeof reason, "The file is not a Fuselage save.");
    } else if (sram_read_word(header + 4) != SRAM_FILE_FORMAT) {
        snprintf(reason, sizeof reason, "Save format %u, this Fuselage reads format %u.",
                 (unsigned)sram_read_word(header + 4), SRAM_FILE_FORMAT);
    } else if (memcmp(header + 8, sram_state.id, SRAM_ID_BYTES)) {
        char theirs[SRAM_ID_BYTES + 1], ours[SRAM_ID_BYTES + 1];
        sram_format_id(theirs, header + 8);
        sram_format_id(ours, (const uint8_t*)sram_state.id);
        snprintf(reason, sizeof reason, "Save id is %s, this program is %s.", theirs, ours);
    } else if (sram_read_word(header + 24) != sram_state.version) {
        snprintf(reason, sizeof reason, "Save version %u, this program is version %u.",
                 (unsigned)sram_read_word(header + 24), (unsigned)sram_state.version);
    } else if (sram_read_word(header + 28) != sram_state.byte_count) {
        snprintf(reason, sizeof reason, "Size is %u bytes, this program declares %u.",
                 (unsigned)sram_read_word(header + 28), (unsigned)sram_state.byte_count);
    } else {
        size_t loaded = fread(sram_state.data, 1, sram_state.byte_count, file);
        if (loaded != sram_state.byte_count) {
            snprintf(reason, sizeof reason, "The file holds %zu of %u bytes.", loaded,
                     (unsigned)sram_state.byte_count);
        } else if (sram_checksum(sram_state.data, sram_state.byte_count) != sram_read_word(header + 32)) {
            snprintf(reason, sizeof reason, "Checksum does not match, the file is damaged.");
        } else {
            sram_state.created = sram_read_wide_word(header + 40);
            valid = true;
        }
    }
    bool read_failed = ferror(file) != 0;
    int close_result = fclose(file);
    if (read_failed || close_result) {
        valid = false;
        snprintf(reason, sizeof reason, "The file could not be read.");
    }
    if (!valid) {
        sram_set_failure(reason);
        SRAM_Close();
        return false;
    }
    sram_state.open = true;
    return true;
}

bool sram_write_file(bool first_run) {
    static uint32_t sequence;
    char staging_path[SRAM_FILE_PATH_CAPACITY + 48];
#if defined(_WIN32)
    unsigned process = (unsigned)GetCurrentProcessId();
#else
    unsigned process = (unsigned)getpid();
#endif
    FILE* file = NULL;
    for (unsigned attempt = 0; attempt < 128; attempt++) {
        snprintf(staging_path, sizeof staging_path, "%s.tmp.%u.%u", sram_state.path, process, sequence++);
        file = fopen(staging_path, "wbx");
        if (file || errno != EEXIST) {
            break;
        }
    }
    if (!file) {
        sram_set_failure("The save staging file could not be created.");
        return false;
    }
    uint8_t header[SRAM_HEADER_BYTES];
    sram_encode_header(header);
    bool written = fwrite(header, 1, sizeof header, file) == sizeof header &&
                   fwrite(sram_state.data, 1, sram_state.byte_count, file) == sram_state.byte_count &&
                   fflush(file) == 0;
    if (fclose(file)) {
        written = false;
    }
    if (!written) {
        remove(staging_path);
        sram_set_failure("The save staging file could not be written completely.");
        return false;
    }
    bool moved_previous = false;
    if (!first_run) {
        struct stat entry;
        if (stat(sram_state.path, &entry) == 0) {
            if (!sram_is_regular_file(&entry)) {
                remove(staging_path);
                sram_set_failure("The save path is not a regular file.");
                return false;
            }
            if (stat(sram_state.previous_path, &entry) == 0) {
                if (!sram_is_regular_file(&entry) || remove(sram_state.previous_path)) {
                    remove(staging_path);
                    sram_set_failure("The previous save could not be replaced.");
                    return false;
                }
            } else if (errno != ENOENT) {
                remove(staging_path);
                sram_set_failure("The previous save could not be inspected.");
                return false;
            }
            if (rename(sram_state.path, sram_state.previous_path)) {
                remove(staging_path);
                sram_set_failure("The current save could not become the previous save.");
                return false;
            }
            moved_previous = true;
        } else if (errno != ENOENT) {
            remove(staging_path);
            sram_set_failure("The current save could not be inspected.");
            return false;
        }
    }
    if (!sram_publish_new(staging_path, sram_state.path)) {
        if (moved_previous && sram_publish_new(sram_state.previous_path, sram_state.path)) {
            remove(sram_state.previous_path);
        }
        remove(staging_path);
        sram_set_failure("The save could not be published; inspect the save and previous paths.");
        return false;
    }
    remove(staging_path);
    return true;
}
