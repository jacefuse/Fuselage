/* Standalone CLI. SEE: (ROAPPACK.md, Commands) */
#include "roappack_internal.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void roappack_usage(FILE* stream) {
    fprintf(stream, "usage: roappack -o <out> [--image <program.roap>] <id>=<file> ...\n"
                    "       roappack --list <file>\n"
                    "       roappack --flatten <program.elf> -o <program.roap>\n"
                    "       roappack --help\n\n"
                    "  --max-bytes <n>  bound each input and the complete output (default: 4294967295)\n"
                    "  Sections are stored raw. IDs accept decimal, hexadecimal or octal notation.\n");
}

static uint32_t roappack_parse_number(const char* text, const char* end, int allow_zero) {
    if (!isdigit((unsigned char)text[0])) {
        roappack_die(text, "expected an unsigned integer");
    }
    errno = 0;
    char* parsed_end;
    unsigned long long value = strtoull(text, &parsed_end, 0);
    if (errno || parsed_end != end || value > UINT32_MAX || (!allow_zero && value == 0)) {
        roappack_die(text, "invalid integer or value outside the supported range");
    }
    return (uint32_t)value;
}

int main(int argc, char** argv) {
    const char* output_path = NULL;
    const char* image_path = NULL;
    const char* flatten_path = NULL;
    const char* list_path = NULL;
    RoapPackSection sections[ROAPPACK_MAX_SECTIONS];
    uint32_t section_count = 0;
    if (atexit(roappack_cleanup_output) != 0) {
        roappack_die("startup", "cannot register output cleanup");
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        roappack_usage(stdout);
        return 0;
    }
    for (int index = 1; index < argc; index++) {
        const char* argument = argv[index];
        if (strcmp(argument, "--list") == 0 || strcmp(argument, "--flatten") == 0 ||
            strcmp(argument, "-o") == 0 || strcmp(argument, "--image") == 0 ||
            strcmp(argument, "--max-bytes") == 0) {
            if (++index >= argc) {
                roappack_usage(stderr);
                return 2;
            }
            const char* value = argv[index];
            const char** slot = strcmp(argument, "--list") == 0      ? &list_path
                                : strcmp(argument, "--flatten") == 0 ? &flatten_path
                                : strcmp(argument, "-o") == 0        ? &output_path
                                : strcmp(argument, "--image") == 0   ? &image_path
                                                                     : NULL;
            if (slot) {
                if (*slot || !value[0]) {
                    roappack_die(argument, "duplicate option or empty path");
                }
                *slot = value;
            } else {
                roappack_max_bytes = roappack_parse_number(value, value + strlen(value), 0);
            }
        } else if (argument[0] == '-') {
            roappack_usage(stderr);
            return 2;
        } else {
            const char* equals = strchr(argument, '=');
            if (!equals || !equals[1]) {
                roappack_die(argument, "expected <id>=<file>");
            }
            if (section_count == ROAPPACK_MAX_SECTIONS) {
                roappack_die(argument, "too many sections (maximum 256)");
            }
            sections[section_count].id = roappack_parse_number(argument, equals, 1);
            sections[section_count++].path = equals + 1;
        }
    }
    if (list_path && !output_path && !image_path && !flatten_path && !section_count) {
        return roappack_list(list_path);
    }
    if (flatten_path && output_path && !image_path && !list_path && !section_count) {
        return roappack_flatten(flatten_path, output_path);
    }
    if (output_path && section_count && !flatten_path && !list_path) {
        return roappack_pack(output_path, image_path, sections, section_count);
    }
    roappack_usage(stderr);
    return 2;
}
