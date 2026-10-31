/* Save names derive from a host-supplied program path. SEE: (SRAM.md, Paths) */
#include "../core/sram_internal.h"
#include <string.h>

static bool sram_is_separator(char character) {
    return character == '/' || character == '\\';
}
bool sram_build_path(char* output, size_t capacity, const char* program_path, const char* extension) {
    if (!program_path || !*program_path) {
        return false;
    }
    size_t length = strlen(program_path), basename = length, end = length;
    while (basename && !sram_is_separator(program_path[basename - 1])) {
        basename--;
    }
    if (basename == length) {
        return false;
    }
    for (size_t index = length; index > basename; index--) {
        if (program_path[index - 1] == '.') {
            end = index - 1;
            break;
        }
    }
    if (end == basename) {
        end = length;
    }
    size_t prefix = sram_state.directory[0] ? strlen(sram_state.directory) : basename;
    bool separator = sram_state.directory[0] && !sram_is_separator(sram_state.directory[prefix - 1]);
    size_t name_bytes = end - basename, extension_bytes = strlen(extension);
    if (prefix >= capacity || name_bytes >= capacity - prefix ||
        extension_bytes + 1u + separator > capacity - prefix - name_bytes) {
        return false;
    }
    if (sram_state.directory[0]) {
        memcpy(output, sram_state.directory, prefix);
        if (separator) {
            output[prefix++] = '/';
        }
    } else {
        memcpy(output, program_path, prefix);
    }
    memcpy(output + prefix, program_path + basename, name_bytes);
    memcpy(output + prefix + name_bytes, extension, extension_bytes + 1);
    return true;
}
