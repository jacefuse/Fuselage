// Fuselage - top-level orchestrator
// Owns engine lifecycle. Coordinates GDMF, CAKE, DICE, and SHARP.
// Subsystems are unaware of each other; only fuselage.c imports all of them.

#include "fuselage.h"
//#include "fuselage_log.h"

// #include "DICE/dice.h"   -- not yet
// #include "SHARP/sharp.h" -- not yet

#include <stdio.h>
#include <stdarg.h>
//#include <string.h>

// Logging
void fuselage_log(const char* fmt, ...) {
    char buf[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifdef DEBUG
    fputs(buf, stdout);
#endif

#ifdef FUSELAGE_HAS_GDMF
    // Strip trailing newline -- text layer line feeds via tlNewLine()
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') { buf[len - 1] = '\0'; }
    tlPrint(buf);
    tlNewLine();
#endif
}

// Internal state machine

typedef enum {
    STATE_UNINIT,
    STATE_RUNNING,
    STATE_QUIT_REQUESTED,
    STATE_SHUTDOWN,
} FuselageState;

static FuselageState g_state = STATE_UNINIT;

// Pending signals (set by game logic, consumed by fuselage tick)

static volatile bool g_signal_quit                     = false;
static volatile bool g_signal_windowed                 = false;
static volatile bool g_signal_borderless               = false;
static volatile bool g_signal_fullscreen_exclusive     = false;

// Forward declarations

static int  fuselage_init(void);
static void fuselage_shutdown(void);
static void fuselage_process_signals(void);

// Public API
void FuselageSignal(FuselageSignalType signal) {
    switch (signal) {
    case FUSELAGE_QUIT:                  g_signal_quit                   = true;

 break;
    case FUSELAGE_WINDOWED:              g_signal_windowed               = true; break;
    case FUSELAGE_BORDERLESS:            g_signal_borderless             = true; break;
    case FUSELAGE_FULLSCREEN_EXCLUSIVE:  g_signal_fullscreen_exclusive   = true; break;
    }

    return;
}

void FuselageSetTitle(const char* title)          { GDMFsetTitle(title);

    return;
}
void FuselageSetResolution(int width, int height)  { GDMFsetResolution(width, height);

    return;
}
void FuselageSetAspectRatio(int num, int den)      { GDMFsetAspectRatio(num, den);

    return;
}
void FuselageSetWindowIcon(int width, int height, const unsigned char* rgba) { GDMFsetWindowIcon(width, height, rgba);

    return;
}

float FuselageGetCurrentFPS(void) { return GDMFGetCurrentFPS(); }

void FuselageSetMouseCapture(bool capture)  { GDMFSetMouseCapture(capture);

    return;
}
bool FuselageGetMouseCapture(void)          { return GDMFGetMouseCapture(); }
bool FuselageToggleMouseCapture(void)       { return GDMFToggleMouseCapture(); }

void FuselageSetCursorVisible(bool visible) { GDMFSetCursorVisible(visible);

    return;
}
bool FuselageGetCursorVisible(void)         { return GDMFGetCursorVisible(); }

// Main loop
bool fuselage(void) {
    if (g_state == STATE_UNINIT) {
        if (fuselage_init() != 0) {
            g_state = STATE_SHUTDOWN;
            return false;
        }
        g_state = STATE_RUNNING;
    }

    // Shutdown path
    if (g_state == STATE_QUIT_REQUESTED) {
        fuselage_shutdown();
        g_state = STATE_SHUTDOWN;
        return false;
    }

    if (g_state == STATE_SHUTDOWN) {
        return false;
    }

    // Normal tick

    // Process any signals from the previous game logic frame
    fuselage_process_signals();

    // Check whether the window is still alive
    if (!GDMFtick()) {
        g_state = STATE_QUIT_REQUESTED;
        return fuselage();  // run shutdown immediately
    }

    // Input poll
    // DICE will eventually control when this fires.
    // For now: every tick.
    CAKE_Poll();

    // Rendering
    // DICE will eventually control when this fires.
    GDMFrenderFrame();

    // Placeholder pacing -- replace with DICE when available
    Sleep(1);

    return true;
}

// Internal: init and shutdown
static int fuselage_init(void) {
    // Tell Windows this process handles DPI itself, before any window is created.
    // With PER_MONITOR_AWARE_V2, all Win32 coordinate APIs (including those Vulkan
    // calls internally when querying surface capabilities) return physical pixels.
    // Without this, GetClientRect returns logical pixels and the swapchain ends up
    // at 1/4 the window area on a 200% DPI system.
    {
        typedef BOOL (WINAPI* SPDAC)(DPI_AWARENESS_CONTEXT);
        SPDAC fn = (SPDAC)(void*)GetProcAddress(GetModuleHandleA("user32.dll"),
                                                 "SetProcessDpiAwarenessContext");
        if (fn) { fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); }
    }

    //FLOG("[Fuselage] Version %s\n", FUSELAGE_VERSION);
    //FLOG("[Fuselage] Init\n");

    printf("[Fuselage] Version %s\n", FUSELAGE_VERSION);
    printf("[Fuselage] Init\n");
    tlPrintFormatted("[Fuselage] Version %s", WHITE, FUSELAGE_VERSION);tlNewLine();
    tlPrint("[Fuselage] Init");tlNewLine();

    if (GDMFinit() != 0) {
        //FLOG("[Fuselage] GDMF init failed\n");
        printf("[Fuselage] GDMF init failed\n");
        tlPrint("[Fuselage] GDMF init failed");tlNewLine();
        return -1;
    }

    // Palettes must be populated before InitSprites uploads its first frame's
    // worth of color data to the GPU.
    InitPalettes();
    InitSprites();
    // Setup and Shutdown a tile layer just to print the version.
    // Commented out because it isn't functionally necessary, but
    // it is retained for testing.
    //InitTileLayer(0, 1, 1, 32, 32, 1, 1.0);
    //ReleaseTileLayer(0);
  
    // CAKE initializes on the first CAKE_Poll() call -- nothing to do here.

    // DICE_init();
    // SHARPinit();

    //FLOG("[Fuselage] Ready\n");
    printf("[Fuselage] Ready\n");
    tlPrint("[Fuselage] Ready");tlNewLine();

    return 0;
}

static void fuselage_shutdown(void) {
    //FLOG("[Fuselage] Shutdown\n");
    printf("[Fuselage] Shutdown\n");
    tlPrint("[Fuselage] Shutdown");tlNewLine();

    CAKE_Shutdown();
    ShutdownSprites();
    ShutdownTiles();
    GDMFshutdown();

    return;
}

// Internal: signal processing
static void fuselage_process_signals(void) {
    if (g_signal_quit) {
        g_signal_quit = false;
        g_state = STATE_QUIT_REQUESTED;
        return;  // remaining signals are irrelevant
    }

    if (g_signal_windowed) {
        g_signal_windowed = false;
        GDMFsetDisplayMode(GDMF_MODE_WINDOWED);
    }

    if (g_signal_borderless) {
        g_signal_borderless = false;
        GDMFsetDisplayMode(GDMF_MODE_BORDERLESS);
    }

    if (g_signal_fullscreen_exclusive) {
        g_signal_fullscreen_exclusive = false;
        GDMFsetDisplayMode(GDMF_MODE_FULLSCREEN_EXCLUSIVE);
    }

    return;
}