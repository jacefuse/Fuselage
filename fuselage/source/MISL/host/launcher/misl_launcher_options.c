/* Command-line admission before changing engine configuration. */
#include "misl_launcher_internal.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool fuselage_launcher_copy_option(char* output, size_t capacity, const char* value) {
    if (!value || strlen(value) >= capacity) {
        return false;
    }
    memcpy(output, value, strlen(value) + 1);
    return true;
}
static bool fuselage_launcher_memory_option(const char* value, uint32_t* output) {
    if (!value || !*value) {
        return false;
    }
    uint32_t result = 0;
    for (const unsigned char* digit = (const unsigned char*)value; *digit; digit++) {
        if (*digit < '0' || *digit > '9' || result > (UINT32_MAX - (*digit - '0')) / 10) {
            return false;
        }
        result = result * 10 + (*digit - '0');
    }
    if (!result) {
        return false;
    }
    *output = result;
    return true;
}
int fuselage_launcher_parse(int argc, char** argv, const char** path, bool* info_only) {
    *path = NULL;
    *info_only = false;
    if (argc < 0 || (argc && !argv)) {
        return FUSELAGE_LAUNCH_FAILED;
    }
    bool options = true;
    for (int index = 1; index < argc; index++) {
        const char* option = argv[index];
        if (!option || !*option) {
            continue;
        }
        if (options && !strcmp(option, "--")) {
            options = false;
            continue;
        }
        if (!options || option[0] != '-') {
            if (*path) {
                fprintf(stderr, "MISLlauncher: provide one ROAP path.\n");
                return FUSELAGE_LAUNCH_FAILED;
            }
            *path = option;
            continue;
        }
        if (!strcmp(option, "--help") || !strcmp(option, "-h")) {
            fuselage_launcher_print_usage(stdout, argc ? argv[0] : NULL);
            return FUSELAGE_LAUNCH_DONE;
        }
        if (!strcmp(option, "--version") || !strcmp(option, "-v")) {
            fuselage_launcher_print_versions();
            return FUSELAGE_LAUNCH_DONE;
        }
        if (!strcmp(option, "--info")) {
            *info_only = true;
            continue;
        }
        if (!strcmp(option, "--exit-on-halt")) {
            fuselage_launcher.exit_on_halt = true;
            continue;
        }
        if (!strcmp(option, "--fullscreen") || !strcmp(option, "--fullscreen-exclusive") ||
            !strcmp(option, "--windowed")) {
            fuselage_launcher.display_mode = !strcmp(option, "--fullscreen") ? FUSELAGE_BORDERLESS
                                             : !strcmp(option, "--fullscreen-exclusive")
                                                 ? FUSELAGE_FULLSCREEN_EXCLUSIVE
                                                 : FUSELAGE_WINDOWED;
            fuselage_launcher.display_chosen = true;
            continue;
        }
        bool capture = !strcmp(option, "--capture-dir"), save = !strcmp(option, "--save-dir");
        bool memory = !strcmp(option, "--max-memory"), seconds = !strcmp(option, "--exit-after");
        if (!capture && !save && !memory && !seconds) {
            fuselage_launcher_ensure_console();
            fprintf(stderr, "MISLlauncher: unknown option '%s'.\n", option);
            return FUSELAGE_LAUNCH_FAILED;
        }
        if (index + 1 >= argc || !argv[index + 1]) {
            fprintf(stderr, "MISLlauncher: %s needs a value.\n", option);
            return FUSELAGE_LAUNCH_FAILED;
        }
        const char* value = argv[++index];
        bool accepted = true;
        if (capture) {
            accepted = fuselage_launcher_copy_option(fuselage_launcher.capture_directory,
                                                     sizeof fuselage_launcher.capture_directory, value);
        }
        if (save) {
            accepted = fuselage_launcher_copy_option(fuselage_launcher.save_directory,
                                                     sizeof fuselage_launcher.save_directory, value);
        }
        if (memory) {
            accepted = fuselage_launcher_memory_option(value, &fuselage_launcher.memory_cap_mib);
        }
        if (seconds) {
            char* end;
            errno = 0;
            double duration = strtod(value, &end);
            accepted = *value && !isspace((unsigned char)*value) && end != value && !*end &&
                       errno != ERANGE && isfinite(duration) && duration > 0;
            if (accepted) {
                fuselage_launcher.exit_after = duration;
            }
        }
        if (!accepted) {
            fprintf(stderr, "MISLlauncher: invalid value for %s: '%s'.\n", option, value);
            return FUSELAGE_LAUNCH_FAILED;
        }
    }
    return FUSELAGE_LAUNCH_RUN;
}
