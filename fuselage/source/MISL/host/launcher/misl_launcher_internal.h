#ifndef MISL_LAUNCHER_INTERNAL_H
#define MISL_LAUNCHER_INTERNAL_H
#include "../misl_launcher.h"
#include "../runtime/misl_host_image_internal.h"
#include "../../../BASE/fuselage_configuration.h"
#include "../../../BASE/fuselage_runtime.h"
#include "../../../ROAP/roap_manifest.h"
#include <stdbool.h>
#include <stdio.h>
typedef struct {
    uint8_t* bytes;
    MislGuestImage image;
    MislHostImagePlan plan;
    char path[1024];
    RoapManifest manifest;
    bool has_manifest;
    bool run_ready;
    bool no_program;
    int load_error;
    FuselageSignalType display_mode;
    bool display_chosen;
    char save_directory[1024];
    char capture_directory[400];
    uint32_t memory_cap_mib;
    bool exit_on_halt;
    double exit_after;
    int exit_code;
    bool verdict_given;
    bool logged;
    bool started;
    double start_time;
    double fps_sum, fps_min, fps_max, sample_start;
    uint64_t sample_steps, sample_count;
} FuselageLauncherState;
extern FuselageLauncherState fuselage_launcher;
void fuselage_launcher_tick_no_program(void);
int fuselage_launcher_parse(int argc, char** argv, const char** path, bool* info_only);
bool fuselage_launcher_load(const char* path);
bool fuselage_launcher_default_path(char* output, size_t capacity, const char* argv0);
void fuselage_launcher_apply_manifest(void);
void fuselage_launcher_decode_manifest(void);
void fuselage_launcher_print_info(void);
void fuselage_launcher_print_versions(void);
void fuselage_launcher_print_usage(FILE* stream, const char* program);
void fuselage_launcher_ensure_console(void);
void fuselage_launcher_log_halt(const MislGuestHaltInfo* halt);
#endif
