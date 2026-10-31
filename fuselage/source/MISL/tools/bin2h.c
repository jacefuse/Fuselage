/* Embed a binary as C source. SEE: (BIN2H.md, Output and failure handling) */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef BIN2H_MAX_BYTES
#define BIN2H_MAX_BYTES UINT_MAX
#endif
_Static_assert(BIN2H_MAX_BYTES <= UINT_MAX, "array length must fit unsigned int");

static FILE* bin2h_input;
static FILE* bin2h_output;
static char* bin2h_staging_path;
static int bin2h_owns_staging;

static void bin2h_cleanup(void) {
    if (bin2h_input) {
        fclose(bin2h_input);
    }
    if (bin2h_output) {
        fclose(bin2h_output);
    }
    if (bin2h_owns_staging && remove(bin2h_staging_path) != 0) {
        fprintf(stderr, "bin2h: cannot remove staging file %s: %s\n", bin2h_staging_path, strerror(errno));
    }
    free(bin2h_staging_path);
}

static _Noreturn void bin2h_fail(const char* reason) {
    fprintf(stderr, "bin2h: %s\n", reason);
    exit(EXIT_FAILURE);
}

static int bin2h_is_letter(unsigned char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

static int bin2h_valid_name(const char* name) {
    /* Leading underscores are reserved for file-scope identifiers. */
    if (!bin2h_is_letter((unsigned char)*name)) {
        return 0;
    }
    for (const unsigned char* cursor = (const unsigned char*)name; *cursor; cursor++) {
        if (!bin2h_is_letter(*cursor) && !(*cursor >= '0' && *cursor <= '9') && *cursor != '_') {
            return 0;
        }
    }
    static const char* const keywords[] = {"alignas",
                                           "alignof",
                                           "asm",
                                           "auto",
                                           "bool",
                                           "break",
                                           "case",
                                           "catch",
                                           "char",
                                           "char8_t",
                                           "char16_t",
                                           "char32_t",
                                           "class",
                                           "compl",
                                           "concept",
                                           "const",
                                           "consteval",
                                           "constexpr",
                                           "constinit",
                                           "const_cast",
                                           "continue",
                                           "co_await",
                                           "co_return",
                                           "co_yield",
                                           "decltype",
                                           "default",
                                           "delete",
                                           "do",
                                           "double",
                                           "dynamic_cast",
                                           "else",
                                           "enum",
                                           "explicit",
                                           "export",
                                           "extern",
                                           "false",
                                           "float",
                                           "for",
                                           "friend",
                                           "goto",
                                           "if",
                                           "inline",
                                           "int",
                                           "long",
                                           "mutable",
                                           "namespace",
                                           "new",
                                           "noexcept",
                                           "nullptr",
                                           "operator",
                                           "private",
                                           "protected",
                                           "public",
                                           "register",
                                           "reinterpret_cast",
                                           "requires",
                                           "restrict",
                                           "return",
                                           "short",
                                           "signed",
                                           "sizeof",
                                           "static",
                                           "static_assert",
                                           "static_cast",
                                           "struct",
                                           "switch",
                                           "template",
                                           "this",
                                           "thread_local",
                                           "throw",
                                           "true",
                                           "try",
                                           "typedef",
                                           "typeid",
                                           "typename",
                                           "typeof",
                                           "typeof_unqual",
                                           "union",
                                           "unsigned",
                                           "using",
                                           "virtual",
                                           "void",
                                           "volatile",
                                           "wchar_t",
                                           "while",
                                           "and",
                                           "and_eq",
                                           "bitand",
                                           "bitor",
                                           "not",
                                           "not_eq",
                                           "or",
                                           "or_eq",
                                           "xor",
                                           "xor_eq"};
    if (strstr(name, "__")) {
        return 0;
    }
    for (size_t index = 0; index < sizeof keywords / sizeof keywords[0]; index++) {
        if (strcmp(name, keywords[index]) == 0) {
            return 0;
        }
    }
    return 1;
}

static void bin2h_check_distinct_files(const char* output_path) {
#if defined(_WIN32)
    HANDLE existing = CreateFileA(output_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (existing == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            bin2h_fail("cannot inspect output path");
        }
        return;
    }
    BY_HANDLE_FILE_INFORMATION input_info, output_info;
    int inspected = GetFileInformationByHandle((HANDLE)_get_osfhandle(_fileno(bin2h_input)), &input_info) &&
                    GetFileInformationByHandle(existing, &output_info);
    CloseHandle(existing);
    if (!inspected) {
        bin2h_fail("cannot inspect file identity");
    }
    int same = input_info.dwVolumeSerialNumber == output_info.dwVolumeSerialNumber &&
               input_info.nFileIndexHigh == output_info.nFileIndexHigh &&
               input_info.nFileIndexLow == output_info.nFileIndexLow;
#else
    struct stat input_info, output_info;
    if (stat(output_path, &output_info) != 0) {
        if (errno != ENOENT) {
            bin2h_fail("cannot inspect output path");
        }
        return;
    }
    if (fstat(fileno(bin2h_input), &input_info) != 0) {
        bin2h_fail("cannot inspect input identity");
    }
    int same = input_info.st_dev == output_info.st_dev && input_info.st_ino == output_info.st_ino;
#endif
    if (same) {
        bin2h_fail("input and output identify the same file");
    }
}

static void bin2h_open_staging(const char* output_path) {
    size_t length = strlen(output_path);
    if (length > SIZE_MAX - 64) {
        bin2h_fail("output path too long");
    }
    bin2h_staging_path = malloc(length + 64);
    if (!bin2h_staging_path) {
        bin2h_fail("out of memory");
    }
#if defined(_WIN32)
    unsigned long process_id = (unsigned long)_getpid();
#else
    unsigned long process_id = (unsigned long)getpid();
#endif
    for (unsigned attempt = 0; attempt < 128; attempt++) {
        snprintf(bin2h_staging_path, length + 64, "%s.tmp.%lu.%u", output_path, process_id, attempt);
        bin2h_output = fopen(bin2h_staging_path, "wbx");
        if (bin2h_output) {
            bin2h_owns_staging = 1;
            return;
        }
        if (errno != EEXIST) {
            bin2h_fail("cannot create staging file");
        }
    }
    bin2h_fail("cannot find unused staging filename");
}

static void bin2h_close_checked(FILE** stream) {
    FILE* closing = *stream;
    *stream = NULL;
    if (fclose(closing) != 0) {
        bin2h_fail("file close failed");
    }
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <input.bin> <output.h> <array_name>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char* input_path = argv[1];
    const char* output_path = argv[2];
    const char* array_name = argv[3];
    if (!bin2h_valid_name(array_name)) {
        bin2h_fail("array name must be an ASCII identifier without keywords or reserved underscores");
    }
    if (atexit(bin2h_cleanup) != 0) {
        bin2h_fail("cannot register cleanup");
    }
    bin2h_input = fopen(input_path, "rb");
    if (!bin2h_input) {
        bin2h_fail("cannot open input");
    }
    bin2h_check_distinct_files(output_path);
    bin2h_open_staging(output_path);
    fprintf(bin2h_output,
            "// Generated by bin2h. Do not edit.\n#pragma once\n\n"
            "static const unsigned char %s[] = {\n",
            array_name);
    unsigned int byte_count = 0;
    int byte;
    while ((byte = fgetc(bin2h_input)) != EOF) {
        if (byte_count == BIN2H_MAX_BYTES) {
            bin2h_fail("input exceeds the unsigned-int byte limit");
        }
        if (byte_count % 12 == 0) {
            fputs("   ", bin2h_output);
        }
        fprintf(bin2h_output, " 0x%02x,", (unsigned int)byte);
        if (byte_count % 12 == 11) {
            fputc('\n', bin2h_output);
        }
        byte_count++;
        if (ferror(bin2h_output)) {
            bin2h_fail("output write failed");
        }
    }
    if (ferror(bin2h_input)) {
        bin2h_fail("input read failed");
    }
    if (byte_count == 0) {
        fputs("    0x00,", bin2h_output);
    }
    if (byte_count % 12 != 0 || byte_count == 0) {
        fputc('\n', bin2h_output);
    }
    fprintf(bin2h_output, "};\nstatic const unsigned int %s_len = %u;\n", array_name, byte_count);
    if (ferror(bin2h_output)) {
        bin2h_fail("output write failed");
    }
    bin2h_close_checked(&bin2h_input);
    bin2h_close_checked(&bin2h_output);
#if defined(_WIN32)
    if (!MoveFileExA(bin2h_staging_path, output_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        bin2h_fail("output replacement failed");
    }
#else
    if (rename(bin2h_staging_path, output_path) != 0) {
        bin2h_fail("output replacement failed");
    }
#endif
    bin2h_owns_staging = 0;
    return EXIT_SUCCESS;
}
