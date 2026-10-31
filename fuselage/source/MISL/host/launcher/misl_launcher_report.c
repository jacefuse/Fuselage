/* Console output and optional development log. SEE: (MISL_LAUNCHER.md, Reports) */
#include "misl_launcher_internal.h"
#include "../../../BASE/core/fuselage_versions.h"
#include "../../misl_abi.h"
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif
void fuselage_launcher_ensure_console(void) {
#if defined(_WIN32)
    static bool tried;
    if (tried) {
        return;
    }
    tried = true;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE), error = GetStdHandle(STD_ERROR_HANDLE);
    bool output_free = !output || output == INVALID_HANDLE_VALUE || GetFileType(output) == FILE_TYPE_UNKNOWN;
    bool error_free = !error || error == INVALID_HANDLE_VALUE || GetFileType(error) == FILE_TYPE_UNKNOWN;
    if ((output_free || error_free) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* stream;
        if (output_free) {
            freopen_s(&stream, "CONOUT$", "w", stdout);
        }
        if (error_free) {
            freopen_s(&stream, "CONOUT$", "w", stderr);
        }
    }
#endif
}
void fuselage_launcher_print_usage(FILE* stream, const char* program) {
    fuselage_launcher_ensure_console();
    fprintf(stream,
            "Usage: %s [options] [game.roap]\n"
            "With no path, loads GAME.ROAP beside the executable.\n"
            "  --fullscreen             Borderless fullscreen\n"
            "  --fullscreen-exclusive   Exclusive fullscreen\n"
            "  --windowed               Windowed display\n"
            "  --save-dir <dir>         Existing save directory\n"
            "  --capture-dir <dir>      Allow frame captures in this directory\n"
            "  --max-memory <MiB>       Positive decimal guest RAM cap\n"
            "  --exit-on-halt           Exit 0 finished, 3 fault, 4 save refusal\n"
            "  --exit-after <seconds>   Exit after a finite positive run duration\n"
            "  --info                   Print accepted file configuration\n"
            "  --version, -v            Print component identities\n"
            "  --help, -h               Print this help\n"
            "  --                       Treat remaining arguments as paths\n",
            program && *program ? program : "MISLlauncher");
}
void fuselage_launcher_print_versions(void) {
    fuselage_launcher_ensure_console();
    size_t count;
    const FuselageComponentVersion* versions = fuselage_get_component_versions(&count);
    for (size_t index = 0; index < count; index++) {
        printf("%-13s %s\n", versions[index].name, versions[index].version ? versions[index].version : "");
    }
    printf("MISL ABI      %u\nROAP manifest %u\nBuild         %s\n", MISL_ABI_VERSION, ROAP_MANIFEST_VERSION,
#if defined(DEBUG)
           "debug"
#else
           "release"
#endif
    );
}
void fuselage_launcher_log_halt(const MislGuestHaltInfo* halt) {
#if defined(DEBUG)
    FILE* file = fopen("roap_halt.log", "a");
    if (!file) {
        return;
    }
    time_t now = time(NULL);
    fprintf(file, "MISLlauncher halt\nwhen %lld UTC seconds\nroap %s\nkind %u\nreason %s\n", (long long)now,
            fuselage_launcher.path, (unsigned)halt->kind, halt->reason ? halt->reason : "");
    fprintf(file, "pc 0x%08X", halt->program_counter);
    if (halt->kind != MISL_GUEST_HALT_FINISHED) {
        fprintf(file, " opcode 0x%08X", halt->instruction);
    }
    fprintf(file, " sp 0x%08X ra 0x%08X\n\n", halt->stack_pointer, halt->return_address);
    fclose(file);
#else
    (void)halt;
#endif
}
