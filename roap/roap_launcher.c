#include "roap_launcher.h"
#include "VPU_roap.h"
#include "roap.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>

// The blob this launcher was pointed at, and the game() that plays it. Unlike
// the baked-in mode, the bytes are read from a file at startup and owned here.

#define ROAP_LAUNCHER_DEFAULT "GAME.ROAP"

static unsigned char *g_code;
static ROAP           g_roap;

// Read the whole file into a fresh buffer. Everything about the input is
// untrusted here -- it is a path a user typed -- so each failure is reported
// and refused rather than guessed at.
static int LoadBlobFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "MISLlauncher: cannot open '%s'\n", path);
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "MISLlauncher: cannot size '%s'\n", path);
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0) {
        fprintf(stderr, "MISLlauncher: cannot size '%s'\n", path);
        fclose(f);
        return 0;
    }
    rewind(f);

    if (size == 0) {
        fprintf(stderr, "MISLlauncher: '%s' is empty -- nothing to run\n", path);
        fclose(f);
        return 0;
    }
    // Checked here as well as in the VPU: refusing before the read means a
    // bogus multi-gigabyte file never gets allocated in the first place.
    if ((unsigned long)size > (unsigned long)ROAP_MAX_BLOB_BYTES) {
        fprintf(stderr, "MISLlauncher: '%s' is %ld bytes -- larger than VPU "
                        "memory (%u max)\n", path, size, (unsigned)ROAP_MAX_BLOB_BYTES);
        fclose(f);
        return 0;
    }

    g_code = malloc((size_t)size);
    if (!g_code) {
        fprintf(stderr, "MISLlauncher: out of memory reading '%s'\n", path);
        fclose(f);
        return 0;
    }

    size_t got = fread(g_code, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        fprintf(stderr, "MISLlauncher: short read on '%s' (%zu of %ld bytes)\n",
                path, got, size);
        free(g_code);
        g_code = NULL;
        return 0;
    }

    g_roap.code    = g_code;
    g_roap.size    = (uint32_t)size;
    g_roap.entryPC = 0;   // blobs are linked flat at 0 (roap.ld)
    return 1;
}

int RoapLauncherInit(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : ROAP_LAUNCHER_DEFAULT;
    if (!LoadBlobFile(path)) {
        if (argc <= 1) {
            fprintf(stderr, "MISLlauncher: no ROAP given and no %s here.\n"
                            "usage: %s [game.roap]\n",
                    ROAP_LAUNCHER_DEFAULT, argv[0] ? argv[0] : "MISLlauncher");
        }
        return 0;
    }
    return 1;
}

void RoapLauncherShutdown(void) {
    free(g_code);
    g_code = NULL;
}

// The launcher's whole game loop: publish this tick's input where the guest
// can read it, then hand the blob to the VPU. The blob drives everything else
// itself, through the MMIO page and ecalls.
void game(void) {
    RoapPumpInput();
    VPU(&g_roap);
}
