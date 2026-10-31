/* Staged output and bounded input. SEE: (ROAPPACK.md, Output replacement) */
#include "roappack_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

uint32_t roappack_max_bytes = UINT32_MAX;
static char* roappack_temporary_path;
static FILE* roappack_temporary_file;
static int roappack_owns_temporary;
static uint64_t roappack_written_bytes;

_Noreturn void roappack_die(const char* context, const char* reason) {
    fprintf(stderr, "roappack: %s: %s\n", context, reason);
    exit(1);
}

unsigned char* roappack_read_file(const char* path, uint32_t* size, uint32_t limit) {
    FILE* input = fopen(path, "rb");
    if (!input) {
        roappack_die(path, strerror(errno));
    }
    unsigned char block[65536];
    unsigned char* bytes = NULL;
    size_t capacity = 0, length = 0;
    for (;;) {
        size_t received = fread(block, 1, sizeof block, input);
        uint64_t required = (uint64_t)length + received;
        if (required > limit || required > SIZE_MAX) {
            roappack_die(path, "exceeds the byte limit");
        }
        if (required > capacity) {
            uint64_t grown = capacity ? (uint64_t)capacity * 2 : sizeof block;
            if (grown < required) {
                grown = required;
            }
            if (grown > limit) {
                grown = limit;
            }
            if (grown > SIZE_MAX) {
                grown = SIZE_MAX;
            }
            unsigned char* replacement = realloc(bytes, (size_t)grown);
            if (!replacement) {
                roappack_die(path, "out of memory");
            }
            bytes = replacement;
            capacity = (size_t)grown;
        }
        if (received) {
            memcpy(bytes + length, block, received);
            length += received;
        }
        if (received < sizeof block) {
            if (ferror(input)) {
                roappack_die(path, "read failed");
            }
            break;
        }
    }
    if (fclose(input) != 0) {
        roappack_die(path, "close failed");
    }
    if (!bytes) {
        bytes = malloc(1);
        if (!bytes) {
            roappack_die(path, "out of memory");
        }
    } else if (length < capacity) {
        /* Packing retains inputs until validation completes; avoid retaining growth slack. */
        unsigned char* compact = realloc(bytes, length ? length : 1);
        if (compact) {
            bytes = compact;
        }
    }
    *size = (uint32_t)length;
    return bytes;
}

void roappack_cleanup_output(void) {
    if (roappack_temporary_file) {
        fclose(roappack_temporary_file);
        roappack_temporary_file = NULL;
    }
    if (roappack_owns_temporary) {
        if (remove(roappack_temporary_path) != 0 && errno != ENOENT) {
            fprintf(stderr, "roappack: %s: staging cleanup failed: %s\n",
                    roappack_temporary_path, strerror(errno));
        }
        roappack_owns_temporary = 0;
    }
    free(roappack_temporary_path);
    roappack_temporary_path = NULL;
}

FILE* roappack_open_output(const char* path) {
    size_t length = strlen(path);
    if (length > SIZE_MAX - 64) {
        roappack_die(path, "path too long");
    }
    roappack_temporary_path = malloc(length + 64);
    if (!roappack_temporary_path) {
        roappack_die(path, "out of memory");
    }
#if defined(_WIN32)
    unsigned long process_id = (unsigned long)_getpid();
#else
    unsigned long process_id = (unsigned long)getpid();
#endif
    for (unsigned attempt = 0; attempt < 128; attempt++) {
        snprintf(roappack_temporary_path, length + 64, "%s.tmp.%lu.%u", path, process_id, attempt);
        /* Exclusive creation never truncates a pre-existing temporary file. */
        roappack_temporary_file = fopen(roappack_temporary_path, "wbx");
        if (roappack_temporary_file) {
            roappack_owns_temporary = 1;
            roappack_written_bytes = 0;
            return roappack_temporary_file;
        }
        if (errno != EEXIST) {
            roappack_die(path, strerror(errno));
        }
    }
    roappack_die(path, "cannot create an unused staging file");
}

void roappack_write(FILE* output, const void* bytes, size_t size) {
    if ((uint64_t)size > roappack_max_bytes - roappack_written_bytes) {
        roappack_die("output", "exceeds the byte limit");
    }
    if (size && fwrite(bytes, 1, size, output) != size) {
        roappack_die("output", "short write");
    }
    roappack_written_bytes += size;
}

void roappack_commit_output(FILE* output, const char* path) {
    roappack_temporary_file = NULL;
    if (fclose(output) != 0) {
        roappack_die(path, "close failed");
    }
#if defined(_WIN32)
    if (!MoveFileExA(roappack_temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fprintf(stderr, "roappack: %s: replacement failed (Windows error %lu)\n", path, GetLastError());
        exit(1);
    }
#else
    if (rename(roappack_temporary_path, path) != 0) {
        roappack_die(path, strerror(errno));
    }
#endif
    roappack_owns_temporary = 0;
}
