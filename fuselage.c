// Fuselage - top-level orchestrator
// Owns engine lifecycle. Coordinates GDMF, CAKE, DICE, and SHARP.
// Subsystems are unaware of each other; only fuselage.c imports all of them.

#include "fuselage.h"
#include "fuselage_sync.h"

#include "DICE/dice.h"
#include "GDMF/textlayer/gdmf_textlayer.h"
// #include "SHARP/sharp.h" -- not yet

#include <stdio.h>

// Internal state machine

typedef enum {
    STATE_UNINIT,
    STATE_RUNNING,
    STATE_QUIT_REQUESTED,
    STATE_SHUTDOWN,
} FuselageState;

static FuselageState g_state = STATE_UNINIT;

// DICE timing
// Sim (input+logic) runs on its own dedicated thread via DICE's callback
// style, at a configurable fixed rate independent of rendering -- see
// fuselage_sim_tick_callback and FuselageSetSimRate(). Rendering has no
// DICE timer of its own by default: with vsync on, Present() paces it to
// the display's real refresh; with vsync off, it runs fully unthrottled.
// FuselageSetTargetFPS() opts into a third timer that caps rendering to an
// explicit rate regardless of vsync/display refresh -- see fuselage()'s
// render section. FuselageWaitFrame() gets its own timer too, independent of
// all of the above, so game code that uses it as a frame-pacing primitive
// is throttled to Fuselage's own clock rather than however fast rendering
// happens to be running.
#define FUSELAGE_DICE_SIM_TIMER       0
#define FUSELAGE_DICE_WAITFRAME_TIMER   1
#define FUSELAGE_DICE_TARGETFPS_TIMER 2
#define FUSELAGE_DEFAULT_SIM_HZ       60.0
#define FUSELAGE_DEFAULT_WAITFRAME_HZ   60.0

// Guards GDMF/game state shared between the sim-tick thread (CAKE_Poll +
// the game's tick callback) and fuselage()'s render call. Scoped narrowly
// around GDMF_PrepareFrame() only -- see fuselage()'s render section for why
// it must never also wrap GDMF_SubmitFrame().
static FuselageMutex g_state_lock;

// Game-supplied logic, invoked from fuselage_sim_tick_callback on its own
// thread. Conventionally a function named game() -- see FuselageSetTickCallback's
// doc comment.
static FuselageTickCallback g_tick_callback = NULL;

// Signaled by the WaitFrame timer's DICE callback -- see
// fuselage_frame_tick_callback. FuselageWaitFrame() blocks on this. Independent
// of g_state_lock -- never touches shared game state.
static FuselageEvent g_frame_event;

// Sim rate -- see FuselageSetSimRate(). Read once, in fuselage_init(), when
// the sim timer is created.
static double g_sim_hz = FUSELAGE_DEFAULT_SIM_HZ;

// FuselageInputRequiresFocus -- see its own doc comment in fuselage.h.
// g_wasFocusedForInput tracks the last focus state CAKE was set to match,
// so fuselage_sim_tick_callback only calls CAKE_Silence/CAKE_Resume on an
// actual transition rather than every single tick.
static bool g_inputRequiresFocus  = true;   // on by default -- see fuselage.h
static bool g_wasFocusedForInput  = true;

// Target FPS cap -- see FuselageSetTargetFPS(). <= 0 means uncapped (the
// default): fuselage()'s render section doesn't wait on g_targetfps_event
// at all in that case. g_targetfps_timer_active tracks whether
// FUSELAGE_DICE_TARGETFPS_TIMER is actually running right now, since the
// timer is only created/torn down on demand rather than always running.
static double g_target_fps            = 0.0;
static bool   g_targetfps_timer_active = false;
static FuselageEvent g_targetfps_event;

// Pending signals (set by game logic, consumed by fuselage tick)

static volatile bool g_signal_quit                     = false;
static volatile bool g_signal_windowed                 = false;
static volatile bool g_signal_borderless               = false;
static volatile bool g_signal_fullscreen_exclusive     = false;
static volatile bool g_signal_apply_target_fps         = false;

// Forward declarations

static int  fuselage_init(void);
static void fuselage_shutdown(void);
static void fuselage_process_signals(void);
static void fuselage_sim_tick_callback(uint8_t handle, void* userdata);
static void fuselage_frame_tick_callback(uint8_t handle, void* userdata);
static void fuselage_targetfps_tick_callback(uint8_t handle, void* userdata);

// Public API
void FuselageSignal(FuselageSignalType signal) {
    switch (signal) {
    case FUSELAGE_QUIT:                  g_signal_quit                   = true;

 break;
    case FUSELAGE_WINDOWED:              g_signal_windowed               = true; break;
    case FUSELAGE_BORDERLESS:            g_signal_borderless             = true; break;
    case FUSELAGE_FULLSCREEN_EXCLUSIVE:  g_signal_fullscreen_exclusive   = true; break;
    case FUSELAGE_APPLY_TARGET_FPS:      g_signal_apply_target_fps       = true; break;
    }

    return;
}

void FuselageSetTitle(const char* title)          { GDMF_SetTitle(title);

    return;
}
void FuselageSetResolution(int width, int height)  { GDMF_SetResolution(width, height);

    return;
}
void FuselageSetCanvasResolution(int width, int height) { GDMF_SetCanvasResolution(width, height);

    return;
}
void FuselageSetAspectRatio(int num, int den)      { GDMF_SetAspectRatio(num, den);

    return;
}

void FuselageGetCanvasResolution(int* width, int* height) {
    if (width)  { *width  = GDMF_GetCanvasWidth();  }
    if (height) { *height = GDMF_GetCanvasHeight(); }

    return;
}
void FuselageGetResolution(int* width, int* height) {
    if (width)  { *width  = GDMF_GetWidth();  }
    if (height) { *height = GDMF_GetHeight(); }

    return;
}
void FuselageGetAspectRatio(int* num, int* den) {
    if (num) { *num = GDMF_GetAspectRatioNum(); }
    if (den) { *den = GDMF_GetAspectRatioDen(); }

    return;
}

void FuselageSetWindowIcon(int width, int height, const unsigned char* rgba) { GDMF_SetWindowIcon(width, height, rgba);

    return;
}

float FuselageGetCurrentFPS(void) { return GDMF_GetCurrentFPS(); }

void FuselageSetMouseCapture(bool capture)  { GDMF_SetMouseCapture(capture);

    return;
}
bool FuselageGetMouseCapture(void)          { return GDMF_GetMouseCapture(); }
bool FuselageToggleMouseCapture(void)       { return GDMF_ToggleMouseCapture(); }

void FuselageSetCursorVisible(bool visible) { GDMF_SetCursorVisible(visible);

    return;
}
bool FuselageGetCursorVisible(void)         { return GDMF_GetCursorVisible(); }

// Applies immediately (see fuselage.h's own comment) rather than waiting for
// fuselage_sim_tick_callback's next focus-transition check -- calling this
// while already unfocused should silence right away, not sit un-silenced
// until the window happens to lose focus again.
void FuselageInputRequiresFocus(bool enabled) {
    g_inputRequiresFocus = enabled;

    bool focused = GDMF_IsFocused();
    if (enabled && !focused) { CAKE_Silence(); }
    else                     { CAKE_Resume();  }
    g_wasFocusedForInput = focused;

    return;
}
bool FuselageGetInputRequiresFocus(void)    { return g_inputRequiresFocus; }

void FuselageSetVSync(bool enabled) { GDMF_SetVSync(enabled);

    return;
}
bool FuselageGetVSync(void)         { return GDMF_GetVSync(); }

void FuselageSetTickCallback(FuselageTickCallback callback) {
    g_tick_callback = callback;

    return;
}

void FuselageWaitFrame(void) {
    fuselage_event_wait(&g_frame_event);

    return;
}

void FuselageSetSimRate(double hz) {
    g_sim_hz = hz;

    return;
}
double FuselageGetSimRate(void) { return g_sim_hz; }

void FuselageSetTargetFPS(double fps) {
    // Setter only -- just records the desired cap. Pre-init, fuselage_init()
    // reads g_target_fps and starts the timer if it's > 0. Post-init, the
    // change is applied by FuselageSignal(FUSELAGE_APPLY_TARGET_FPS), which
    // runs the timer lifecycle on the render thread (see
    // fuselage_process_signals) rather than mutating it from the caller's
    // thread.
    g_target_fps = fps;

    return;
}
double FuselageGetTargetFPS(void) { return g_target_fps; }

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
    if (!GDMF_Tick()) {
        g_state = STATE_QUIT_REQUESTED;
        return fuselage();  // run shutdown immediately
    }

    // Rendering. Input polling and the game's logic tick run entirely
    // independently of this call now, on DICE's sim-timer thread (see
    // fuselage_sim_tick_callback) -- fuselage() never waits on them, so
    // render throughput is fully decoupled from the sim rate.
    //
    // GDMF_PrepareFrame() is the only part of rendering that reads live game
    // state (sprites/tiles/pixies/text layer), so it's the only part that
    // needs g_state_lock. GDMF_SubmitFrame() only touches per-image GPU
    // buffers PrepareFrame already filled, plus the GPU queue/present --
    // and it's where the vsync-blocking wait lives. It must run outside the
    // lock: holding a shared lock across a blocking vsync wait, paired with
    // this now-unthrottled loop, is exactly what starved the sim thread out
    // entirely the first time this was tried (game setup never ran, input
    // never got polled -- see conversation history). Scoping the lock to
    // just the fast, non-blocking half is the actual fix.
    //
    // With vsync on, Present() paces this loop to the display's real
    // refresh rate on its own -- no DICE timer needed to throttle it. With
    // vsync off, Present() doesn't block at all, so this runs fully
    // unthrottled, as fast as the system can prepare and submit frames.
    //
    // FuselageSetTargetFPS() opts into an explicit cap on top of either of
    // those, via a real blocking wait here -- not a non-blocking check --
    // so an uncapped-by-vsync loop doesn't busy-spin at 100% CPU on every
    // tick it decides not to render yet. This wait happens outside the
    // lock, same as GDMF_SubmitFrame()'s vsync wait -- it's just another
    // blocking wait on this thread, nothing about it touches shared state.
    if (g_targetfps_timer_active) {
        fuselage_event_wait(&g_targetfps_event);
    }

    fuselage_mutex_lock(&g_state_lock);
    GDMF_PrepareFrame();
    fuselage_mutex_unlock(&g_state_lock);

    GDMF_SubmitFrame();

    return true;
}

// Internal: init and shutdown
static int fuselage_init(void) {
#if defined(_WIN32)
    // Tell Windows this process handles DPI itself, before any window is created.
    // With PER_MONITOR_AWARE_V2, all Win32 coordinate APIs (including those Vulkan
    // calls internally when querying surface capabilities) return physical pixels.
    // Without this, GetClientRect returns logical pixels and the swapchain ends up
    // at 1/4 the window area on a 200% DPI system.
    // (macOS needs no equivalent: GDMF's backend reports backing pixels
    // directly and keeps the CAMetalLayer's drawableSize matched -- see
    // gdmf_window_macos_stub.m.)
    {
        typedef BOOL (WINAPI* SPDAC)(DPI_AWARENESS_CONTEXT);
        SPDAC fn = (SPDAC)(void*)GetProcAddress(GetModuleHandleA("user32.dll"),
                                                 "SetProcessDpiAwarenessContext");
        if (fn) { fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); }
    }
#endif

    printf("[Fuselage] Version %s\n", FUSELAGE_VERSION);

    if (GDMF_Init() != 0) {
        //printf("[Fuselage] GDMF init failed\n");
        //tlPrint("[Fuselage] GDMF init failed");tlNewLine();
        return -1;
    }

    // Hand CAKE the application window -- unconditionally, on every
    // platform; what CAKE does with it is per-platform necessity (see
    // CAKE_AttachWindow's doc comment). This wiring lives here and only
    // here: GDMF doesn't know who takes its handle, CAKE doesn't know who
    // made its window. Must happen on this (main) thread, right after the
    // window exists -- macOS installs the event monitor during this call
    // and AppKit demands the main thread for it.
    CAKE_AttachWindow(GDMF_GetNativeWindowHandle());

    // Palettes must be populated before InitSprites uploads its first frame's
    // worth of color data to the GPU.
    //InitColorsPalettes();

    // Init before use to make sure the atlas image is created before use.
    InitSprites();

    // CAKE initializes on the first CAKE_Poll() call, which now happens on
    // DICE's sim-timer thread so cake_hwnd ends up bound to that thread,
    // consistently polled from there every tick.

    fuselage_mutex_init(&g_state_lock);

    DICE_Init();

    fuselage_event_init(&g_frame_event);
    fuselage_event_init(&g_targetfps_event);

    DICE_InitTimer(FUSELAGE_DICE_SIM_TIMER, g_sim_hz, true,
                    fuselage_sim_tick_callback, NULL);
    DICE_InitTimer(FUSELAGE_DICE_WAITFRAME_TIMER, FUSELAGE_DEFAULT_WAITFRAME_HZ, true,
                    fuselage_frame_tick_callback, NULL);

    // Only start the target-FPS timer if FuselageSetTargetFPS() was called
    // before this point (pre-init) with a positive value. It's opt-in and
    // uncapped by default. A post-init change is applied via the
    // FUSELAGE_APPLY_TARGET_FPS signal (see fuselage_process_signals).
    if (g_target_fps > 0.0) {
        DICE_InitTimer(FUSELAGE_DICE_TARGETFPS_TIMER, g_target_fps, true,
                        fuselage_targetfps_tick_callback, NULL);
        g_targetfps_timer_active = true;
    }

    // SHARPinit();

    printf("\nREADY!\n");
    tlPrintFormattedC(WHITE, "\nREADY!\n");

    return 0;
}

static void fuselage_shutdown(void) {
    printf("[Fuselage] Shutdown\n");
    //tlPrint("[Fuselage] Shutdown");tlNewLine();

    // DICE first, always -- this joins the sim-timer thread, so
    // fuselage_sim_tick_callback is guaranteed to have exited before CAKE,
    // sprites, tiles, or pixies (everything it can reach through the game's
    // tick callback) get torn down below.
    DICE_Shutdown();

    CAKE_Shutdown();
    ShutdownSprites();
    ShutdownTiles();
    ShutdownPixies();
    GDMF_Shutdown();

    fuselage_event_destroy(&g_frame_event);
    fuselage_event_destroy(&g_targetfps_event);
    g_targetfps_timer_active = false;
    fuselage_mutex_destroy(&g_state_lock);

    return;
}

// Internal: DICE callbacks
// Fire on their own DICE timer's dedicated worker thread (see
// DICE/dice_timers.h), never on the thread that calls fuselage().

// CAKE's poll and the logic that reads what it polled must happen
// back-to-back on the same thread (see cake.c) -- that's why polling lives
// here rather than in fuselage(). The lock keeps this from overlapping
// fuselage()'s GDMF_PrepareFrame() call, which touches the same GDMF state
// concurrently on a different thread. Deliberately does NOT wrap
// GDMF_SubmitFrame() -- that call isn't made from here at all, and never
// should be; see fuselage()'s render section.
static void fuselage_sim_tick_callback(uint8_t handle, void* userdata) {
    (void)handle; (void)userdata;

    fuselage_mutex_lock(&g_state_lock);

    // Auto-silence: only acts on an actual focus transition, and only when
    // FuselageInputRequiresFocus(true) is in effect -- see its own doc
    // comment in fuselage.h. Checked before CAKE_Poll() so a transition this
    // same tick already applies to whatever WM_INPUT messages Poll is about
    // to pump.
    if (g_inputRequiresFocus) {
        bool focused = GDMF_IsFocused();
        if (focused != g_wasFocusedForInput) {
            if (focused) { CAKE_Resume();  }
            else         { CAKE_Silence(); }
            g_wasFocusedForInput = focused;
        }
    }

    CAKE_Poll();
    if (g_tick_callback) { g_tick_callback(); }
    fuselage_mutex_unlock(&g_state_lock);

    return;
}

static void fuselage_frame_tick_callback(uint8_t handle, void* userdata) {
    (void)handle; (void)userdata;

    fuselage_event_set(&g_frame_event);

    return;
}

static void fuselage_targetfps_tick_callback(uint8_t handle, void* userdata) {
    (void)handle; (void)userdata;

    fuselage_event_set(&g_targetfps_event);

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
        GDMF_SetDisplayMode(GDMF_MODE_WINDOWED);
    }

    if (g_signal_borderless) {
        g_signal_borderless = false;
        GDMF_SetDisplayMode(GDMF_MODE_BORDERLESS);
    }

    if (g_signal_fullscreen_exclusive) {
        g_signal_fullscreen_exclusive = false;
        GDMF_SetDisplayMode(GDMF_MODE_FULLSCREEN_EXCLUSIVE);
    }

    if (g_signal_apply_target_fps) {
        g_signal_apply_target_fps = false;
        // Applied here, on the render thread, so the target-FPS DICE timer and
        // its event are only ever created/destroyed from the same thread
        // fuselage() waits on g_targetfps_event from. DICE_InitTimer tears down
        // and recreates an already-active handle, so this is safe for both the
        // first apply and a runtime rate change.
        if (g_target_fps > 0.0) {
            DICE_InitTimer(FUSELAGE_DICE_TARGETFPS_TIMER, g_target_fps, true,
                            fuselage_targetfps_tick_callback, NULL);
            g_targetfps_timer_active = true;
        } else if (g_targetfps_timer_active) {
            DICE_ReleaseTimer(FUSELAGE_DICE_TARGETFPS_TIMER);
            g_targetfps_timer_active = false;
        }
    }

    return;
}
