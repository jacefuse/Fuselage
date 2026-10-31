/* Native project defaults. SEE: (FUSELAGE_BUILD.md, Project configuration) */
#ifndef FUSELAGE_PROJECT_H
#define FUSELAGE_PROJECT_H
#include <stddef.h>
#include <stdint.h>
#include "project_config.h"
#ifndef PROJECT_TITLE
#define PROJECT_TITLE "Fuselage"
#endif
#ifndef PROJECT_CANVAS_WIDTH
#define PROJECT_CANVAS_WIDTH 0
#endif
#ifndef PROJECT_CANVAS_HEIGHT
#define PROJECT_CANVAS_HEIGHT 0
#endif
#ifndef PROJECT_WINDOW_WIDTH
#define PROJECT_WINDOW_WIDTH 0
#endif
#ifndef PROJECT_WINDOW_HEIGHT
#define PROJECT_WINDOW_HEIGHT 0
#endif
#ifndef PROJECT_ASPECT_NUM
#define PROJECT_ASPECT_NUM 0
#endif
#ifndef PROJECT_ASPECT_DEN
#define PROJECT_ASPECT_DEN 0
#endif
#ifndef PROJECT_SIM_RATE
#define PROJECT_SIM_RATE 0.0
#endif
#ifndef PROJECT_TARGET_FPS
#define PROJECT_TARGET_FPS 0.0
#endif
#ifndef PROJECT_CURSOR_VISIBLE
#define PROJECT_CURSOR_VISIBLE 1
#endif
#ifndef PROJECT_BANNER
#define PROJECT_BANNER 1
#endif
#ifndef PROJECT_AUDIO_POOL_MB
#define PROJECT_AUDIO_POOL_MB 0
#endif
#ifndef PROJECT_ICON_RGBA
#define PROJECT_ICON_RGBA NULL
#endif
#ifndef PROJECT_ICON_WIDTH
#define PROJECT_ICON_WIDTH 0
#endif
#ifndef PROJECT_ICON_HEIGHT
#define PROJECT_ICON_HEIGHT 0
#endif
#ifndef PROJECT_SRAM_ID
#define PROJECT_SRAM_ID ""
#endif
#ifndef PROJECT_SRAM_VERSION
#define PROJECT_SRAM_VERSION 0
#endif
#ifndef PROJECT_SRAM_BYTES
#define PROJECT_SRAM_BYTES 0
#endif
typedef struct {
    const char* title;
    int canvas_width, canvas_height, window_width, window_height;
    int aspect_numerator, aspect_denominator;
    double simulation_rate, target_fps;
    int cursor_visible, banner, audio_pool_mib;
    const unsigned char* icon_rgba;
    int icon_width, icon_height;
    const char* save_id;
    uint32_t save_version, save_bytes;
} FuselageProjectConfig;
static const FuselageProjectConfig fuselage_project = {.title = PROJECT_TITLE,
                                                       .canvas_width = PROJECT_CANVAS_WIDTH,
                                                       .canvas_height = PROJECT_CANVAS_HEIGHT,
                                                       .window_width = PROJECT_WINDOW_WIDTH,
                                                       .window_height = PROJECT_WINDOW_HEIGHT,
                                                       .aspect_numerator = PROJECT_ASPECT_NUM,
                                                       .aspect_denominator = PROJECT_ASPECT_DEN,
                                                       .simulation_rate = PROJECT_SIM_RATE,
                                                       .target_fps = PROJECT_TARGET_FPS,
                                                       .cursor_visible = PROJECT_CURSOR_VISIBLE,
                                                       .banner = PROJECT_BANNER,
                                                       .audio_pool_mib = PROJECT_AUDIO_POOL_MB,
                                                       .icon_rgba = PROJECT_ICON_RGBA,
                                                       .icon_width = PROJECT_ICON_WIDTH,
                                                       .icon_height = PROJECT_ICON_HEIGHT,
                                                       .save_id = PROJECT_SRAM_ID,
                                                       .save_version = PROJECT_SRAM_VERSION,
                                                       .save_bytes = PROJECT_SRAM_BYTES};
#endif
