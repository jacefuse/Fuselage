/* Metadata is decoded only after shared host preflight accepts the image. */
#include "misl_launcher_internal.h"
#include <string.h>
static uint32_t fuselage_launcher_word(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}
static uint16_t fuselage_launcher_half(const uint8_t* bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}
static bool fuselage_launcher_read_manifest(void) {
    const uint8_t* bytes = fuselage_launcher.bytes;
    if (fuselage_launcher.plan.program_image_bytes < ROAP_MANIFEST_BYTES ||
        fuselage_launcher_word(bytes) != ROAP_MANIFEST_MAGIC) {
        return false;
    }
    RoapManifest* manifest = &fuselage_launcher.manifest;
    *manifest = (RoapManifest){0};
#define READ_HALF(field) manifest->field = fuselage_launcher_half(bytes + offsetof(RoapManifest, field))
#define READ_WORD(field) manifest->field = fuselage_launcher_word(bytes + offsetof(RoapManifest, field))
    READ_WORD(magic);
    READ_HALF(version);
    READ_HALF(codeOffset);
    READ_HALF(canvasWidth);
    READ_HALF(canvasHeight);
    READ_HALF(windowWidth);
    READ_HALF(windowHeight);
    READ_HALF(aspectNum);
    READ_HALF(aspectDen);
    READ_HALF(audioPoolMB);
    READ_HALF(flags);
    READ_WORD(simRateMilli);
    READ_WORD(targetFPSMilli);
    READ_WORD(romMinVersion);
    READ_WORD(sramVersion);
    READ_WORD(sramBytes);
    READ_HALF(programPoolMB);
    manifest->poolCount = bytes[offsetof(RoapManifest, poolCount)];
    manifest->poolReserved = bytes[offsetof(RoapManifest, poolReserved)];
    for (unsigned index = 0; index < ROAP_MANIFEST_MAX_POOLS; index++) {
        manifest->poolMB[index] = fuselage_launcher_half(bytes + offsetof(RoapManifest, poolMB) + 2 * index);
    }
    for (unsigned index = 0; index < 4; index++) {
        manifest->reserved[index] =
            fuselage_launcher_word(bytes + offsetof(RoapManifest, reserved) + 4 * index);
    }
    memcpy(manifest->title, bytes + offsetof(RoapManifest, title), sizeof manifest->title);
    manifest->title[sizeof manifest->title - 1] = '\0';
    memcpy(manifest->sramId, bytes + offsetof(RoapManifest, sramId), sizeof manifest->sramId);
#undef READ_HALF
#undef READ_WORD
    return true;
}
void fuselage_launcher_apply_manifest(void) {
    if (!fuselage_launcher.has_manifest) {
        return;
    }
    const RoapManifest* manifest = &fuselage_launcher.manifest;
    if (manifest->title[0]) {
        FuselageSetTitle(manifest->title);
    }
    if (manifest->canvasWidth && manifest->canvasHeight) {
        FuselageSetCanvasResolution(manifest->canvasWidth, manifest->canvasHeight);
    }
    if (manifest->windowWidth && manifest->windowHeight) {
        FuselageSetResolution(manifest->windowWidth, manifest->windowHeight);
    }
    if (manifest->aspectNum && manifest->aspectDen) {
        FuselageSetAspectRatio(manifest->aspectNum, manifest->aspectDen);
    }
    if (manifest->audioPoolMB) {
        FuselageSetAudioPoolMB(manifest->audioPoolMB);
    }
    if (manifest->simRateMilli) {
        FuselageSetSimRate((double)manifest->simRateMilli / ROAP_MANIFEST_RATE_SCALE);
    }
    if (manifest->targetFPSMilli) {
        FuselageSetTargetFPS((double)manifest->targetFPSMilli / ROAP_MANIFEST_RATE_SCALE);
    }
    if (manifest->flags & ROAP_MANIFEST_HIDE_CURSOR) {
        FuselageSetCursorVisible(false);
    }
    FuselageSetBanner((manifest->flags & ROAP_MANIFEST_BANNER) != 0);
}
void fuselage_launcher_print_info(void) {
    fuselage_launcher_ensure_console();
    fuselage_launcher.has_manifest = fuselage_launcher_read_manifest();
    const RoapManifest* manifest = &fuselage_launcher.manifest;
    printf("roap    : %s\nsize    : %u bytes\n", fuselage_launcher.path, fuselage_launcher.image.byte_count);
    if (!fuselage_launcher.has_manifest) {
        printf("manifest: none; entry offset 0\n");
        return;
    }
    printf("manifest: version %u, code starts at offset %u\n", manifest->version, manifest->codeOffset);
    printf("title   : %s\n", manifest->title[0] ? manifest->title : "(default)");
    if (manifest->canvasWidth && manifest->canvasHeight) {
        printf("canvas  : %ux%u\n", manifest->canvasWidth, manifest->canvasHeight);
    } else {
        puts("canvas  : default");
    }
    if (manifest->windowWidth && manifest->windowHeight) {
        printf("window  : %ux%u\n", manifest->windowWidth, manifest->windowHeight);
    } else {
        puts("window  : default");
    }
    if (manifest->aspectNum && manifest->aspectDen) {
        printf("aspect  : %u:%u\n", manifest->aspectNum, manifest->aspectDen);
    } else {
        puts("aspect  : default");
    }
    if (manifest->simRateMilli) {
        printf("sim rate: %.3f Hz\n", (double)manifest->simRateMilli / ROAP_MANIFEST_RATE_SCALE);
    } else {
        puts("sim rate: default");
    }
    if (manifest->targetFPSMilli) {
        printf("fps     : %.3f\n", (double)manifest->targetFPSMilli / ROAP_MANIFEST_RATE_SCALE);
    } else {
        puts("fps     : default");
    }
    printf("audio   : %u MiB pool (0 selects default)\n", manifest->audioPoolMB);
    printf("flags   : cursor %s, banner %s\n",
           manifest->flags & ROAP_MANIFEST_HIDE_CURSOR ? "hidden" : "visible",
           manifest->flags & ROAP_MANIFEST_BANNER ? "on" : "off");
    printf("memory  : %u program bytes, %u total RAM bytes\nsave    : %u bytes\n",
           fuselage_launcher.plan.program_bytes, fuselage_launcher.plan.allocation_bytes,
           fuselage_launcher.plan.save_bytes);
}
/* RUN needs the same decoder without printing informational output. */
void fuselage_launcher_decode_manifest(void) {
    fuselage_launcher.has_manifest = fuselage_launcher_read_manifest();
}
