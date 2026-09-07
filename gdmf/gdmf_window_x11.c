// GDMF -- Linux/X11 window backend.
// The OS window itself: the Xlib Display connection, the window and its
// WM properties (EWMH), display mode switching, cursor capture/visibility,
// aspect ratio enforcement, and idle inhibition. Platform-neutral state and
// the public API live in gdmf.c; the interface between the two is
// gdmf_window.h, forwarded here through the Linux dispatch table
// (gdmf_window_linux.h).
//
// Threading: the macOS pattern, not the Wayland sibling's -- a main-thread
// pump (gdmf_window_pump drains XPending/XNextEvent once per GDMF_Tick),
// and NO reader thread. The Wayland sibling needs one because a compositor
// disconnects a client that stops reading its socket mid-tick; an X11
// server just buffers for slow readers, so a long tick costs nothing but
// latency. XInitThreads() is still the first call in create: the public
// setters (display mode, capture/visibility) run on whatever thread calls
// them and make Xlib calls of their own, so the connection must be locking.
// CAKE never rides this connection at all -- it opens its own in
// CAKE_AttachWindow (capture-on-poll-thread via XInput2 raw events; see
// cake.c's Linux section).
//
// Display modes, mirroring Win32's save/restore semantics: WINDOWED
// restores the geometry saved when it was left; BORDERLESS is EWMH
// fullscreen (_NET_WM_STATE_FULLSCREEN via ClientMessage to the root);
// FULLSCREEN_EXCLUSIVE is the same EWMH fullscreen plus an XRandR CRTC
// mode-set. Today that mode-set targets the monitor's current mode -- the
// same formality Win32's ChangeDisplaySettings at the current resolution is
// -- but the plumbing is real: a different RRMode in the one apply call is
// all a genuine mode change needs, and leaving exclusive restores what was
// saved. gdmf_st_displayMode updates when the switch actually applies: the
// _NET_WM_STATE PropertyNotify, or the request round-trip when the EWMH
// state doesn't change (BORDERLESS <-> EXCLUSIVE).
//
// A dead X server is not handled gracefully the way the Wayland sibling's
// dead compositor is: Xlib's IO error handler terminates the process by
// design (it may not return), and an X server outliving its clients is the
// normal order of things -- not worth a longjmp contraption to invert.

// -std=c11 is strict ISO; pthread is POSIX and must be asked for by name
// before any header is pulled in.
#define _POSIX_C_SOURCE 200809L

#include "gdmf.h"
#include "gdmf_window.h"
#include "gdmf_window_linux.h"
#include "../fuselage_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/scrnsaver.h>

// --- Connection and window ---------------------------------------------------

static Display* g_display = NULL;
static Window   g_window  = 0;
static int      g_screen  = 0;

static Atom g_atomWmProtocols       = None;
static Atom g_atomWmDelete          = None;
static Atom g_atomWmState           = None;   // ICCCM WM_STATE (iconic tracking)
static Atom g_atomNetWmName         = None;
static Atom g_atomUtf8String        = None;
static Atom g_atomNetWmState        = None;
static Atom g_atomNetWmFullscreen   = None;
static Atom g_atomNetWmIcon         = None;

static bool g_mapped = false;   // first MapNotify seen (create blocks on it)

// Windowed geometry, saved when leaving WINDOWED so coming back restores it
// (parity with the Win32 backend's g_savedWindowRect). Client-area origin in
// root coordinates -- the WM's frame can offset a restored window by its
// border width, the few pixels Win32 sidesteps by saving the frame rect;
// accepted rather than bringing _NET_FRAME_EXTENTS into it.
static int g_savedX = 0;
static int g_savedY = 0;
static int g_savedW = 0;
static int g_savedH = 0;

static bool g_fullscreen = false;   // what _NET_WM_STATE last confirmed
// The mode the caller asked for -- BORDERLESS and FULLSCREEN_EXCLUSIVE are
// the same EWMH state (the XRandR mode-set is invisible to the WM), so the
// requested mode is what a confirmed fullscreen state reports back as, the
// same rule as the Wayland sibling.
static GDMF_DisplayMode g_requestedMode = GDMF_MODE_WINDOWED;

// The CRTC whose mode FULLSCREEN_EXCLUSIVE changed, and the mode to put
// back on leaving it. None = nothing to restore.
static RRCrtc g_exclusiveCrtc = None;
static RRMode g_exclusiveMode = None;

// --- Input-adjacent state (capture, cursor) ----------------------------------

static Cursor g_blankCursor    = None;
static bool   g_pointerGrabbed = false;
static bool   g_cursorHidden   = false;

// Serializes gdmf_x11_apply_input_state: it runs from the pump (focus
// events, main thread) and from whatever thread calls the public capture/
// visibility setters, and its grab/hidden bookkeeping must not race. (Xlib
// itself is already locking via XInitThreads; this guards OUR state.)
static pthread_mutex_t g_inputStateLock = PTHREAD_MUTEX_INITIALIZER;

static void gdmf_x11_apply_input_state(void);
static void gdmf_window_x11_destroy(void);   // create()'s failure paths use it

// --- Aspect ratio enforcement ------------------------------------------------

// Same correction as the Win32 backend's gdmf_compute_aspect_corrected_size
// and the Wayland sibling's gdmf_wl_aspect_corrected: anchors on whichever
// dimension is already smaller relative to the target ratio and shrinks the
// other to match, so the corrected size never exceeds what arrived. The
// PAspect size hint makes the WM do this during interactive resize (the
// WM_SIZING counterpart); this function backstops the WMs that ignore it.
static void gdmf_x11_aspect_corrected(int* w, int* h) {
    if (*w <= 0 || *h <= 0) { return; }

    int heightForWidth = (*w * gdmf_cfg_aspectDen) / gdmf_cfg_aspectNum;
    if (heightForWidth <= *h) { *h = heightForWidth; }
    else { *w = (*h * gdmf_cfg_aspectNum) / gdmf_cfg_aspectDen; }

    return;
}

// --- WM property helpers -----------------------------------------------------

// Whether _NET_WM_STATE currently lists _NET_WM_STATE_FULLSCREEN -- the
// WM's confirmation, read on its PropertyNotify.
static bool gdmf_x11_net_state_fullscreen(void) {
    Atom type;
    int format;
    unsigned long count, after;
    unsigned char* data = NULL;
    bool fullscreen = false;

    if (XGetWindowProperty(g_display, g_window, g_atomNetWmState, 0, 64, False,
                           XA_ATOM, &type, &format, &count, &after, &data) == Success && data) {
        for (unsigned long i = 0; i < count; i++) {
            Atom a;
            memcpy(&a, data + i * sizeof(Atom), sizeof(Atom));
            if (a == g_atomNetWmFullscreen) { fullscreen = true; }
        }
        XFree(data);
    }

    return fullscreen;
}

// Whether ICCCM WM_STATE says Iconic -- the minimize signal PropertyNotify
// delivers (UnmapNotify/MapNotify bracket it; this is the authority).
static bool gdmf_x11_wm_state_iconic(void) {
    Atom type;
    int format;
    unsigned long count, after;
    unsigned char* data = NULL;
    bool iconic = false;

    if (XGetWindowProperty(g_display, g_window, g_atomWmState, 0, 2, False,
                           g_atomWmState, &type, &format, &count, &after, &data) == Success && data) {
        if (count >= 1) {
            unsigned long state;
            memcpy(&state, data, sizeof(state));
            iconic = (state == IconicState);
        }
        XFree(data);
    }

    return iconic;
}

// The EWMH way to change a mapped window's state: a ClientMessage to the
// root, which the WM intercepts -- setting the property directly is
// explicitly not how the spec works.
static void gdmf_x11_request_fullscreen(bool on) {
    XEvent e;

    memset(&e, 0, sizeof(e));
    e.xclient.type         = ClientMessage;
    e.xclient.window       = g_window;
    e.xclient.message_type = g_atomNetWmState;
    e.xclient.format       = 32;
    e.xclient.data.l[0]    = on ? 1 : 0;   // _NET_WM_STATE_ADD / _NET_WM_STATE_REMOVE
    e.xclient.data.l[1]    = (long)g_atomNetWmFullscreen;
    e.xclient.data.l[2]    = 0;
    e.xclient.data.l[3]    = 1;            // source: normal application

    XSendEvent(g_display, RootWindow(g_display, g_screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);

    return;
}

// --- Window icon -------------------------------------------------------------

// _NET_WM_ICON: a cardinal array of width, height, then height*width ARGB
// pixels -- real parity with Win32's HICON, unlike the Wayland sibling's
// documented gap. Format-32 property data travels as unsigned long per
// element regardless of the machine's long width; Xlib repacks on the wire.
static void gdmf_x11_apply_icon(void) {
    if (!gdmf_cfg_iconRGBA || gdmf_cfg_iconWidth <= 0 || gdmf_cfg_iconHeight <= 0) { return; }

    int w = gdmf_cfg_iconWidth;
    int h = gdmf_cfg_iconHeight;
    size_t count = 2 + (size_t)w * (size_t)h;

    unsigned long* data = malloc(count * sizeof(unsigned long));
    if (!data) { return; }

    data[0] = (unsigned long)w;
    data[1] = (unsigned long)h;
    const unsigned char* rgba = gdmf_cfg_iconRGBA;
    for (int i = 0; i < w * h; i++) {
        data[2 + i] = ((unsigned long)rgba[i * 4 + 3] << 24) |   // A
                      ((unsigned long)rgba[i * 4 + 0] << 16) |   // R
                      ((unsigned long)rgba[i * 4 + 1] <<  8) |   // G
                      ((unsigned long)rgba[i * 4 + 2]);          // B
    }

    XChangeProperty(g_display, g_window, g_atomNetWmIcon, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)data, (int)count);
    free(data);

    return;
}

// --- XRandR (FULLSCREEN_EXCLUSIVE's mode-set) --------------------------------

// The CRTC under the window's center, with its current config. The caller
// frees both; NULL when nothing matched (a window mid-drag between
// monitors, a disabled output).
static XRRCrtcInfo* gdmf_x11_find_crtc(XRRScreenResources* res, RRCrtc* outCrtc) {
    Window child;
    int cx = 0, cy = 0;

    XTranslateCoordinates(g_display, g_window, RootWindow(g_display, g_screen),
                          gdmf_st_width / 2, gdmf_st_height / 2, &cx, &cy, &child);

    for (int i = 0; i < res->ncrtc; i++) {
        XRRCrtcInfo* info = XRRGetCrtcInfo(g_display, res, res->crtcs[i]);
        if (!info) { continue; }
        if (info->mode != None &&
            cx >= info->x && cx < info->x + (int)info->width &&
            cy >= info->y && cy < info->y + (int)info->height) {
            *outCrtc = res->crtcs[i];
            return info;
        }
        XRRFreeCrtcInfo(info);
    }

    return NULL;
}

static void gdmf_x11_enter_exclusive(void) {
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(g_display,
                                  RootWindow(g_display, g_screen));
    if (!res) { return; }

    RRCrtc crtc = None;
    XRRCrtcInfo* info = gdmf_x11_find_crtc(res, &crtc);
    if (info) {
        // Today's target is the mode the CRTC is already in -- the same
        // formality Win32's ChangeDisplaySettings at the current resolution
        // is. A real mode change is a different RRMode right here; the
        // save/restore around it already handles the rest.
        RRMode target = info->mode;

        if (XRRSetCrtcConfig(g_display, res, crtc, CurrentTime,
                             info->x, info->y, target, info->rotation,
                             info->outputs, info->noutput) == RRSetConfigSuccess) {
            g_exclusiveCrtc = crtc;
            g_exclusiveMode = info->mode;
        }
        XRRFreeCrtcInfo(info);
    }
    XRRFreeScreenResources(res);

    return;
}

static void gdmf_x11_leave_exclusive(void) {
    if (g_exclusiveCrtc == None) { return; }

    XRRScreenResources* res = XRRGetScreenResourcesCurrent(g_display,
                                  RootWindow(g_display, g_screen));
    if (res) {
        XRRCrtcInfo* info = XRRGetCrtcInfo(g_display, res, g_exclusiveCrtc);
        if (info) {
            XRRSetCrtcConfig(g_display, res, g_exclusiveCrtc, CurrentTime,
                             info->x, info->y, g_exclusiveMode, info->rotation,
                             info->outputs, info->noutput);
            XRRFreeCrtcInfo(info);
        }
        XRRFreeScreenResources(res);
    }

    g_exclusiveCrtc = None;
    g_exclusiveMode = None;

    return;
}

// --- Cursor visibility / mouse capture ---------------------------------------

// Re-derives both effects from scratch every time, exactly like the Win32
// backend's gdmf_apply_input_state: cheap, and only runs on focus/toggle
// events, never per-frame. XGrabPointer's confine_to is the ClipCursor
// counterpart, and the server keeps the confinement current across
// moves/resizes on its own -- no WM_SIZE/WM_MOVE re-clip dance needed.
static void gdmf_x11_apply_input_state(void) {
    if (!g_display || !g_window) { return; }

    pthread_mutex_lock(&g_inputStateLock);

    bool wantGrab = gdmf_st_hasFocus && gdmf_st_mouseCaptureDesired;
    if (wantGrab && !g_pointerGrabbed) {
        // owner_events True: events keep routing to whatever window they
        // would have reached anyway -- the grab exists for confine_to only.
        // (CAKE's raw events survive the grab because it announces XI 2.3;
        // see cake.c's Linux section.)
        if (XGrabPointer(g_display, g_window, True,
                         ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                         GrabModeAsync, GrabModeAsync,
                         g_window, None, CurrentTime) == GrabSuccess) {
            g_pointerGrabbed = true;
        }
        // A refused grab (some other client holds one) stays off until the
        // next focus/toggle event retries -- same do-not-thrash disposition
        // as everything else here.
    } else if (!wantGrab && g_pointerGrabbed) {
        XUngrabPointer(g_display, CurrentTime);
        g_pointerGrabbed = false;
    }

    bool shouldHide = gdmf_st_hasFocus && !gdmf_st_cursorVisibleDesired;
    if (shouldHide != g_cursorHidden) {
        if (shouldHide) { XDefineCursor(g_display, g_window, g_blankCursor); }
        else            { XUndefineCursor(g_display, g_window); }
        g_cursorHidden = shouldHide;
    }

    // Setters run off the pump thread; their requests must not sit in the
    // buffer until the next tick's pump.
    XFlush(g_display);

    pthread_mutex_unlock(&g_inputStateLock);

    return;
}

// X11 has no "no cursor" -- hiding is defining a fully transparent one.
static Cursor gdmf_x11_create_blank_cursor(void) {
    char none = 0;
    XColor black;

    memset(&black, 0, sizeof(black));
    Pixmap p = XCreateBitmapFromData(g_display, g_window, &none, 1, 1);
    Cursor c = XCreatePixmapCursor(g_display, p, p, &black, &black, 0, 0);
    XFreePixmap(g_display, p);

    return c;
}

// --- Event handling (main thread, via the pump) ------------------------------

static void gdmf_x11_handle_event(const XEvent* ev) {
    switch (ev->type) {

    case ClientMessage:
        if (ev->xclient.message_type == g_atomWmProtocols &&
            (Atom)ev->xclient.data.l[0] == g_atomWmDelete) {
            gdmf_st_closeRequested = true;
        }
        break;

    case ConfigureNotify: {
        int w = ev->xconfigure.width;
        int h = ev->xconfigure.height;

        // Enforce the design aspect ratio on floating sizes -- the parity
        // rule with Win32's WM_SIZING/WM_WINDOWPOSCHANGING pair. The
        // PAspect hint normally makes the WM deliver correct sizes on its
        // own; some WMs ignore it, so a violating size gets a corrective
        // shrink-to-fit resize (its own ConfigureNotify records the result;
        // a WM that refuses simply sends no further event -- no loop).
        // Fullscreen sizes are the monitor's and deliberately left alone:
        // letterboxing is the renderer's job, not the window's.
        if (!g_fullscreen && g_requestedMode == GDMF_MODE_WINDOWED) {
            int cw = w;
            int ch = h;
            gdmf_x11_aspect_corrected(&cw, &ch);
            if (cw != w || ch != h) { XResizeWindow(g_display, g_window, (unsigned)cw, (unsigned)ch); }
        }

        if (w > 0 && h > 0 && (w != gdmf_st_width || h != gdmf_st_height)) {
            gdmf_st_width          = w;
            gdmf_st_height         = h;
            gdmf_st_resizeOccurred = true;
        }
        break;
    }

    case FocusIn:
        // Grab-transient focus events (the WM's own alt-tab keyboard grab)
        // are not real focus changes; reacting to them would flap the
        // capture state mid-switch.
        if (ev->xfocus.mode == NotifyGrab || ev->xfocus.mode == NotifyUngrab) { break; }
        gdmf_st_hasFocus = true;
        gdmf_x11_apply_input_state();
        break;

    case FocusOut:
        // Release capture/hidden-cursor while unfocused so alt-tabbing away
        // never leaves the user's mouse stuck -- the WM_KILLFOCUS rule.
        // Reapplied on FocusIn above if still desired.
        if (ev->xfocus.mode == NotifyGrab || ev->xfocus.mode == NotifyUngrab) { break; }
        gdmf_st_hasFocus = false;
        gdmf_x11_apply_input_state();
        break;

    case MapNotify: {
        bool wasMinimized = gdmf_st_minimized;
        g_mapped          = true;
        gdmf_st_minimized = false;
        // Restore-from-minimize counts as a resize event on Win32 (WM_SIZE
        // with wasMinimized); keep that signal identical here.
        if (wasMinimized) { gdmf_st_resizeOccurred = true; }
        break;
    }

    case UnmapNotify:
        // Iconified windows are unmapped; the WM_STATE PropertyNotify below
        // is the authority, this is the prompt first word.
        gdmf_st_minimized = true;
        break;

    case PropertyNotify:
        if (ev->xproperty.atom == g_atomNetWmState) {
            // The WM's confirmation of a display mode switch -- the
            // "backend updates on actual switch" contract's apply point.
            g_fullscreen = gdmf_x11_net_state_fullscreen();
            if (!g_fullscreen) { g_requestedMode = GDMF_MODE_WINDOWED; }

            GDMF_DisplayMode confirmed =
                g_fullscreen ? (g_requestedMode == GDMF_MODE_WINDOWED
                                ? GDMF_MODE_BORDERLESS : g_requestedMode)
                             : GDMF_MODE_WINDOWED;
            if (confirmed != gdmf_st_displayMode) {
                gdmf_st_displayMode = confirmed;
                printf("[GDMF] Display mode: %s\n",
                       confirmed == GDMF_MODE_WINDOWED ? "WINDOWED" :
                       confirmed == GDMF_MODE_BORDERLESS ? "BORDERLESS" : "FULLSCREEN EXCLUSIVE");
            }
        } else if (ev->xproperty.atom == g_atomWmState) {
            gdmf_st_minimized = gdmf_x11_wm_state_iconic();
        }
        break;

    default:
        break;
    }

    return;
}

// --- Backend interface (see gdmf_window.h) -----------------------------------

static int gdmf_window_x11_create(void) {
    // Before any other Xlib call, ever: the public setters make Xlib calls
    // from whatever thread the game/render side runs them on.
    XInitThreads();

    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        printf("[GDMF] XOpenDisplay failed -- no X server? (DISPLAY=%s)\n",
               getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");

        return -1;
    }
    g_screen = DefaultScreen(g_display);

    g_atomWmProtocols     = XInternAtom(g_display, "WM_PROTOCOLS", False);
    g_atomWmDelete        = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    g_atomWmState         = XInternAtom(g_display, "WM_STATE", False);
    g_atomNetWmName       = XInternAtom(g_display, "_NET_WM_NAME", False);
    g_atomUtf8String      = XInternAtom(g_display, "UTF8_STRING", False);
    g_atomNetWmState      = XInternAtom(g_display, "_NET_WM_STATE", False);
    g_atomNetWmFullscreen = XInternAtom(g_display, "_NET_WM_STATE_FULLSCREEN", False);
    g_atomNetWmIcon       = XInternAtom(g_display, "_NET_WM_ICON", False);

    // Centered on the screen, like Win32's SM_CXSCREEN arithmetic.
    int x = (DisplayWidth(g_display, g_screen)  - gdmf_cfg_width)  / 2;
    int y = (DisplayHeight(g_display, g_screen) - gdmf_cfg_height) / 2;

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel = BlackPixel(g_display, g_screen);
    attrs.event_mask       = StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
    // No key/button masks: input is CAKE's business, on its own connection.

    g_window = XCreateWindow(g_display, RootWindow(g_display, g_screen),
                             x, y, (unsigned)gdmf_cfg_width, (unsigned)gdmf_cfg_height, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask, &attrs);
    if (!g_window) {
        printf("[GDMF] XCreateWindow failed\n");
        gdmf_window_x11_destroy();

        return -1;
    }

    // Close request as a WM protocol -- without it the WM would just
    // XKillClient on the close button.
    XSetWMProtocols(g_display, g_window, &g_atomWmDelete, 1);

    // Title: _NET_WM_NAME (UTF-8, what every current WM displays) plus the
    // legacy WM_NAME for anything ancient enough to read only that.
    XStoreName(g_display, g_window, gdmf_cfg_title);
    XChangeProperty(g_display, g_window, g_atomNetWmName, g_atomUtf8String, 8,
                    PropModeReplace, (const unsigned char*)gdmf_cfg_title,
                    (int)strlen(gdmf_cfg_title));

    XClassHint classHint;
    classHint.res_name  = (char*)"fuselage";
    classHint.res_class = (char*)"fuselage";
    XSetClassHint(g_display, g_window, &classHint);

    // PMinSize is WM_GETMINMAXINFO's counterpart; PAspect hands the WM the
    // interactive-resize enforcement Win32 does by hand in WM_SIZING (the
    // ConfigureNotify correction backstops WMs that ignore it). PPosition
    // asks the WM to honor the centered spot instead of its own placement.
    XSizeHints* hints = XAllocSizeHints();
    if (hints) {
        hints->flags        = PPosition | PSize | PMinSize | PAspect;
        hints->x            = x;
        hints->y            = y;
        hints->width        = gdmf_cfg_width;
        hints->height       = gdmf_cfg_height;
        hints->min_width    = 320;
        hints->min_height   = 180;
        hints->min_aspect.x = gdmf_cfg_aspectNum;
        hints->min_aspect.y = gdmf_cfg_aspectDen;
        hints->max_aspect.x = gdmf_cfg_aspectNum;
        hints->max_aspect.y = gdmf_cfg_aspectDen;
        XSetWMNormalHints(g_display, g_window, hints);
        XFree(hints);
    }

    gdmf_x11_apply_icon();

    g_blankCursor = gdmf_x11_create_blank_cursor();

    gdmf_st_width  = gdmf_cfg_width;
    gdmf_st_height = gdmf_cfg_height;

    // Map and wait for it to land -- the counterpart of the Wayland
    // sibling's first-configure wait, and safe to block on for the same
    // reason: nothing else is dispatching this connection yet.
    XMapWindow(g_display, g_window);
    while (!g_mapped) {
        XEvent ev;
        XNextEvent(g_display, &ev);
        gdmf_x11_handle_event(&ev);
    }

    // Keep the display awake while the engine runs -- the counterpart of
    // Win32's SetThreadExecutionState, and for the same reason: a
    // controller-only game looks idle to the screensaver. Lifted again in
    // destroy.
    XScreenSaverSuspend(g_display, True);

    // Apply whatever mouse capture / cursor visibility was requested before
    // the window existed -- same sequencing as the other backends. Focus
    // arrives via FocusIn moments later; starting from "focused" matches
    // Win32/macOS and is corrected by the first real focus event.
    gdmf_st_hasFocus = true;
    gdmf_x11_apply_input_state();

    printf("[GDMF] X11 window ready\n");

    return 0;
}

static void gdmf_window_x11_destroy(void) {
    if (!g_display) { return; }

    // Put the display mode back before the window goes -- the same
    // unconditional cleanup as Win32's WM_DESTROY cursor release.
    gdmf_x11_leave_exclusive();

    if (g_pointerGrabbed) {
        XUngrabPointer(g_display, CurrentTime);
        g_pointerGrabbed = false;
    }
    g_cursorHidden = false;

    XScreenSaverSuspend(g_display, False);

    if (g_blankCursor != None) {
        XFreeCursor(g_display, g_blankCursor);
        g_blankCursor = None;
    }
    if (g_window) {
        XDestroyWindow(g_display, g_window);
        g_window = 0;
    }

    XCloseDisplay(g_display);
    g_display = NULL;

    g_mapped        = false;
    g_fullscreen    = false;
    g_requestedMode = GDMF_MODE_WINDOWED;
    g_savedW        = 0;
    g_savedH        = 0;

    return;
}

static void gdmf_window_x11_pump(void) {
    if (!g_display) { return; }

    // Everything queued since last tick, never a blocking read: XPending
    // does the socket read itself when the queue is empty, so XNextEvent
    // here only ever takes what is already in hand.
    while (XPending(g_display) > 0) {
        XEvent ev;
        XNextEvent(g_display, &ev);
        gdmf_x11_handle_event(&ev);
    }

    // Push out anything the setters queued from other threads since the
    // last tick (their own XFlush covers the common case; this is the
    // backstop that bounds the latency).
    XFlush(g_display);

    return;
}

static void gdmf_window_x11_request_display_mode(GDMF_DisplayMode mode) {
    if (!g_display || !g_window) { return; }
    if (mode == gdmf_st_displayMode) { return; }

    // Called from the game/render side -- safe: XInitThreads makes the
    // connection locking, and the WM's confirmation comes back through the
    // pump (PropertyNotify) regardless of who sent the request.
    GDMF_DisplayMode leaving = gdmf_st_displayMode;

    if (leaving == GDMF_MODE_WINDOWED) {
        // Save windowed geometry before leaving it.
        Window child;
        int wx = 0, wy = 0;
        XTranslateCoordinates(g_display, g_window, RootWindow(g_display, g_screen),
                              0, 0, &wx, &wy, &child);
        g_savedX = wx;
        g_savedY = wy;
        g_savedW = gdmf_st_width;
        g_savedH = gdmf_st_height;
    }

    g_requestedMode = mode;

    switch (mode) {

    case GDMF_MODE_WINDOWED:
        gdmf_x11_request_fullscreen(false);
        // Most WMs restore the pre-fullscreen geometry on their own; the
        // explicit restore is the Win32 parity guarantee, not a hope.
        if (g_savedW > 0 && g_savedH > 0) {
            XMoveResizeWindow(g_display, g_window, g_savedX, g_savedY,
                              (unsigned)g_savedW, (unsigned)g_savedH);
        }
        break;

    case GDMF_MODE_BORDERLESS:
        gdmf_x11_request_fullscreen(true);
        break;

    case GDMF_MODE_FULLSCREEN_EXCLUSIVE:
        // The XRandR mode-set first (it keys off the window's current
        // monitor, best read before the WM starts moving it), then the same
        // EWMH fullscreen BORDERLESS uses.
        gdmf_x11_enter_exclusive();
        gdmf_x11_request_fullscreen(true);
        break;

    }

    // Restore the display mode when leaving exclusive -- Win32's
    // ChangeDisplaySettings(NULL) counterpart.
    if (leaving == GDMF_MODE_FULLSCREEN_EXCLUSIVE) { gdmf_x11_leave_exclusive(); }

    // BORDERLESS <-> EXCLUSIVE: the EWMH state doesn't change, so no
    // PropertyNotify will confirm anything -- the request round-trip is the
    // apply point (the XRandR call above is already synchronous).
    bool wasFullscreen  = (leaving != GDMF_MODE_WINDOWED);
    bool wantFullscreen = (mode    != GDMF_MODE_WINDOWED);
    if (wasFullscreen == wantFullscreen) {
        XSync(g_display, False);
        gdmf_st_displayMode = mode;
        printf("[GDMF] Display mode: %s\n",
               mode == GDMF_MODE_BORDERLESS ? "BORDERLESS" : "FULLSCREEN EXCLUSIVE");
    } else {
        XFlush(g_display);
    }

    return;
}

static void gdmf_window_x11_input_state_changed(void) {
    gdmf_x11_apply_input_state();

    return;
}

// XQueryPointer's child coords are the real client-area pixel the OS cursor
// is over right now -- full parity with Win32's GetCursorPos/ScreenToClient
// (no "last known" caveat like the Wayland sibling; X11 answers even with
// the pointer elsewhere).
static bool gdmf_window_x11_get_mouse_client(int* x, int* y) {
    if (!g_display || !g_window) { return false; }

    Window root, child;
    int rootX = 0, rootY = 0, winX = 0, winY = 0;
    unsigned int mask = 0;

    XQueryPointer(g_display, g_window, &root, &child,
                  &rootX, &rootY, &winX, &winY, &mask);

    *x = winX;
    *y = winY;

    return true;
}

// Native handles for gdmf_surface_x11.c
void* GDMF_GetX11Display(void) { return (void*)g_display; }
void* GDMF_GetX11Window(void)  { return (void*)(uintptr_t)g_window; }

// The tagged handle for CAKE_AttachWindow (see fuselage_native.h). An XID
// is an integer, not a pointer -- it rides through the void* field cast via
// uintptr_t. Filled fresh on each call: both change across create/destroy
// cycles.
static FuselageLinuxNativeHandle g_nativeHandle;
static void* gdmf_window_x11_native_handle(void) {
    g_nativeHandle.kind    = "x11";
    g_nativeHandle.display = (void*)g_display;
    g_nativeHandle.window  = (void*)(uintptr_t)g_window;

    return (void*)&g_nativeHandle;
}

// --- The dispatch table (see gdmf_window_linux.h) ----------------------------

const gdmf_linux_backend gdmf_backend_x11 = {
    .name                 = "x11",
    .create               = gdmf_window_x11_create,
    .destroy              = gdmf_window_x11_destroy,
    .pump                 = gdmf_window_x11_pump,
    .request_display_mode = gdmf_window_x11_request_display_mode,
    .input_state_changed  = gdmf_window_x11_input_state_changed,
    .get_mouse_client     = gdmf_window_x11_get_mouse_client,
    .native_handle        = gdmf_window_x11_native_handle,
};
