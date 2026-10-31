// X11 window lifecycle, properties, display transitions, and event handling.
// SEE: (GDMF_LINUX.md, X11 event ownership).
#define _POSIX_C_SOURCE 200809L

#include "../gdmf.h"
#include "gdmf_window.h"
#include "gdmf_window_linux.h"
#include "../../BASE/fuselage_native.h"

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

static Display* gdmf_x11_display_connection = NULL;
static Window   gdmf_x11_window_id = 0;
static int      gdmf_x11_screen_index = 0;

static Atom gdmf_x11_atom_window_protocols = None;
static Atom gdmf_x11_atom_delete_window = None;
static Atom gdmf_x11_atom_window_state = None;
static Atom gdmf_x11_atom_window_title = None;
static Atom gdmf_x11_atom_utf8_string = None;
static Atom gdmf_x11_atom_extended_window_state = None;
static Atom gdmf_x11_atom_fullscreen = None;
static Atom gdmf_x11_atom_window_icon = None;

static bool gdmf_x11_window_mapped = false;

static int gdmf_x11_saved_window_x = 0;
static int gdmf_x11_saved_window_y = 0;
static int gdmf_x11_saved_window_width = 0;
static int gdmf_x11_saved_window_height = 0;

static bool gdmf_x11_fullscreen_confirmed = false;

static GDMF_DisplayMode gdmf_x11_requested_mode = GDMF_MODE_WINDOWED;

static RRCrtc gdmf_x11_exclusive_controller = None;
static RRMode gdmf_x11_saved_display_mode = None;

static Cursor gdmf_x11_blank_cursor = None;
static bool   gdmf_x11_pointer_grabbed = false;
static bool   gdmf_x11_cursor_hidden = false;

// Protects grab/visibility bookkeeping, not all window state or object lifetime.
static pthread_mutex_t gdmf_x11_input_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static void gdmf_x11_apply_input_state(void);
static void gdmf_x11_destroy_window(void);

static void gdmf_x11_fit_aspect_ratio(int* window_width, int* window_height) {
    if (*window_width <= 0 || *window_height <= 0) { return; }

    int height_for_width = (*window_width * gdmf_window_config_aspect_denominator) / gdmf_window_config_aspect_numerator;
    if (height_for_width <= *window_height) { *window_height = height_for_width; }
    else { *window_width = (*window_height * gdmf_window_config_aspect_numerator) / gdmf_window_config_aspect_denominator; }

    return;
}

// Read the window manager fullscreen property when its notification arrives.
static bool gdmf_x11_is_fullscreen(void) {
    Atom property_type;
    int property_format;
    unsigned long item_count, remaining_bytes;
    unsigned char* property_data = NULL;
    bool fullscreen_present = false;

    if (XGetWindowProperty(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_x11_atom_extended_window_state, 0, 64, False,
                           XA_ATOM, &property_type, &property_format, &item_count, &remaining_bytes, &property_data) == Success && property_data) {
        for (unsigned long item_index = 0; item_index < item_count; item_index++) {
            Atom state_atom;
            memcpy(&state_atom, property_data + item_index * sizeof(Atom), sizeof(Atom));
            if (state_atom == gdmf_x11_atom_fullscreen) { fullscreen_present = true; }
        }
        XFree(property_data);
    }

    return fullscreen_present;
}

// Read the window manager iconic state used for minimization reporting.
static bool gdmf_x11_is_iconic(void) {
    Atom property_type;
    int property_format;
    unsigned long item_count, remaining_bytes;
    unsigned char* property_data = NULL;
    bool window_iconic = false;

    if (XGetWindowProperty(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_x11_atom_window_state, 0, 2, False,
                           gdmf_x11_atom_window_state, &property_type, &property_format, &item_count, &remaining_bytes, &property_data) == Success && property_data) {
        if (item_count >= 1) {
            unsigned long window_state;
            memcpy(&window_state, property_data, sizeof(window_state));
            window_iconic = (window_state == IconicState);
        }
        XFree(property_data);
    }

    return window_iconic;
}

// Ask the window manager through a root-window ClientMessage.
static void gdmf_x11_request_fullscreen(bool fullscreen_requested) {
    XEvent event;

    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = gdmf_x11_window_id;
    event.xclient.message_type = gdmf_x11_atom_extended_window_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = fullscreen_requested ? 1 : 0;
    event.xclient.data.l[1] = (long)gdmf_x11_atom_fullscreen;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;

    XSendEvent(gdmf_x11_display_connection, RootWindow(gdmf_x11_display_connection, gdmf_x11_screen_index), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);

    return;
}

// Property elements are unsigned long; each pixel encodes ARGB in its low 32 bits.
static void gdmf_x11_apply_icon(void) {
    if (!gdmf_window_config_icon_rgba || gdmf_window_config_icon_width <= 0 || gdmf_window_config_icon_height <= 0) { return; }

    int icon_width = gdmf_window_config_icon_width;
    int icon_height = gdmf_window_config_icon_height;
    size_t property_item_count = 2 + (size_t)icon_width * (size_t)icon_height;

    unsigned long* icon_property = malloc(property_item_count * sizeof(unsigned long));
    if (!icon_property) { return; }

    icon_property[0] = (unsigned long)icon_width;
    icon_property[1] = (unsigned long)icon_height;
    const unsigned char* icon_rgba = gdmf_window_config_icon_rgba;
    for (int pixel_index = 0; pixel_index < icon_width * icon_height; pixel_index++) {
        icon_property[2 + pixel_index] = ((unsigned long)icon_rgba[pixel_index * 4 + 3] << 24) |
                      ((unsigned long)icon_rgba[pixel_index * 4 + 0] << 16) |
                      ((unsigned long)icon_rgba[pixel_index * 4 + 1] <<  8) |
                      ((unsigned long)icon_rgba[pixel_index * 4 + 2]);
    }

    XChangeProperty(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_x11_atom_window_icon, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)icon_property, (int)property_item_count);
    free(icon_property);

    return;
}

// Find the active display controller containing the window center; caller frees its info.
static XRRCrtcInfo* gdmf_x11_find_display_controller(XRRScreenResources* screen_resources, RRCrtc* selected_controller) {
    Window child_window;
    int center_x = 0, center_y = 0;

    XTranslateCoordinates(gdmf_x11_display_connection, gdmf_x11_window_id, RootWindow(gdmf_x11_display_connection, gdmf_x11_screen_index),
                          gdmf_window_width / 2, gdmf_window_height / 2, &center_x, &center_y, &child_window);

    for (int controller_index = 0; controller_index < screen_resources->ncrtc; controller_index++) {
        XRRCrtcInfo* controller_info = XRRGetCrtcInfo(gdmf_x11_display_connection, screen_resources, screen_resources->crtcs[controller_index]);
        if (!controller_info) { continue; }
        if (controller_info->mode != None &&
            center_x >= controller_info->x && center_x < controller_info->x + (int)controller_info->width &&
            center_y >= controller_info->y && center_y < controller_info->y + (int)controller_info->height) {
            *selected_controller = screen_resources->crtcs[controller_index];
            return controller_info;
        }
        XRRFreeCrtcInfo(controller_info);
    }

    return NULL;
}

// Currently reapplies the selected controller mode; it does not choose a new mode.
static void gdmf_x11_enter_exclusive(void) {
    XRRScreenResources* screen_resources = XRRGetScreenResourcesCurrent(gdmf_x11_display_connection,
                                  RootWindow(gdmf_x11_display_connection, gdmf_x11_screen_index));
    if (!screen_resources) { return; }

    RRCrtc display_controller = None;
    XRRCrtcInfo* controller_info = gdmf_x11_find_display_controller(screen_resources, &display_controller);
    if (controller_info) {

        RRMode target_mode = controller_info->mode;

        if (XRRSetCrtcConfig(gdmf_x11_display_connection, screen_resources, display_controller, CurrentTime,
                             controller_info->x, controller_info->y, target_mode, controller_info->rotation,
                             controller_info->outputs, controller_info->noutput) == RRSetConfigSuccess) {
            gdmf_x11_exclusive_controller = display_controller;
            gdmf_x11_saved_display_mode = controller_info->mode;
        }
        XRRFreeCrtcInfo(controller_info);
    }
    XRRFreeScreenResources(screen_resources);

    return;
}

static void gdmf_x11_leave_exclusive(void) {
    if (gdmf_x11_exclusive_controller == None) { return; }

    XRRScreenResources* screen_resources = XRRGetScreenResourcesCurrent(gdmf_x11_display_connection,
                                  RootWindow(gdmf_x11_display_connection, gdmf_x11_screen_index));
    if (screen_resources) {
        XRRCrtcInfo* controller_info = XRRGetCrtcInfo(gdmf_x11_display_connection, screen_resources, gdmf_x11_exclusive_controller);
        if (controller_info) {
            XRRSetCrtcConfig(gdmf_x11_display_connection, screen_resources, gdmf_x11_exclusive_controller, CurrentTime,
                             controller_info->x, controller_info->y, gdmf_x11_saved_display_mode, controller_info->rotation,
                             controller_info->outputs, controller_info->noutput);
            XRRFreeCrtcInfo(controller_info);
        }
        XRRFreeScreenResources(screen_resources);
    }

    gdmf_x11_exclusive_controller = None;
    gdmf_x11_saved_display_mode = None;

    return;
}

// Apply requested confinement and visibility according to focus.
static void gdmf_x11_apply_input_state(void) {
    if (!gdmf_x11_display_connection || !gdmf_x11_window_id) { return; }

    pthread_mutex_lock(&gdmf_x11_input_state_mutex);

    bool grab_requested = gdmf_window_has_focus && gdmf_window_mouse_capture_requested;
    if (grab_requested && !gdmf_x11_pointer_grabbed) {

        if (XGrabPointer(gdmf_x11_display_connection, gdmf_x11_window_id, True,
                         ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                         GrabModeAsync, GrabModeAsync,
                         gdmf_x11_window_id, None, CurrentTime) == GrabSuccess) {
            gdmf_x11_pointer_grabbed = true;
        }

    } else if (!grab_requested && gdmf_x11_pointer_grabbed) {
        XUngrabPointer(gdmf_x11_display_connection, CurrentTime);
        gdmf_x11_pointer_grabbed = false;
    }

    bool hide_cursor = gdmf_window_has_focus && !gdmf_window_cursor_visible_requested;
    if (hide_cursor != gdmf_x11_cursor_hidden) {
        if (hide_cursor) { XDefineCursor(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_x11_blank_cursor); }
        else            { XUndefineCursor(gdmf_x11_display_connection, gdmf_x11_window_id); }
        gdmf_x11_cursor_hidden = hide_cursor;
    }

    XFlush(gdmf_x11_display_connection);

    pthread_mutex_unlock(&gdmf_x11_input_state_mutex);

    return;
}

// A transparent cursor image implements hidden-cursor presentation.
static Cursor gdmf_x11_create_blank_cursor(void) {
    char transparent_pixel = 0;
    XColor cursor_color;

    memset(&cursor_color, 0, sizeof(cursor_color));
    Pixmap cursor_bitmap = XCreateBitmapFromData(gdmf_x11_display_connection, gdmf_x11_window_id, &transparent_pixel, 1, 1);
    Cursor blank_cursor = XCreatePixmapCursor(gdmf_x11_display_connection, cursor_bitmap, cursor_bitmap, &cursor_color, &cursor_color, 0, 0);
    XFreePixmap(gdmf_x11_display_connection, cursor_bitmap);

    return blank_cursor;
}

static void gdmf_x11_handle_event(const XEvent* event) {
    switch (event->type) {

    case ClientMessage:
        if (event->xclient.message_type == gdmf_x11_atom_window_protocols &&
            (Atom)event->xclient.data.l[0] == gdmf_x11_atom_delete_window) {
            gdmf_window_close_requested = true;
        }
        break;

    case ConfigureNotify: {
        int window_width = event->xconfigure.width;
        int window_height = event->xconfigure.height;

        if (!gdmf_x11_fullscreen_confirmed && gdmf_x11_requested_mode == GDMF_MODE_WINDOWED) {
            int corrected_width = window_width;
            int corrected_height = window_height;
            gdmf_x11_fit_aspect_ratio(&corrected_width, &corrected_height);
            if (corrected_width != window_width || corrected_height != window_height) { XResizeWindow(gdmf_x11_display_connection, gdmf_x11_window_id, (unsigned)corrected_width, (unsigned)corrected_height); }
        }

        if (window_width > 0 && window_height > 0 && (window_width != gdmf_window_width || window_height != gdmf_window_height)) {
            gdmf_window_width = window_width;
            gdmf_window_height = window_height;
            gdmf_window_resize_pending = true;
        }
        break;
    }

    case FocusIn:

        if (event->xfocus.mode == NotifyGrab || event->xfocus.mode == NotifyUngrab) { break; }
        gdmf_window_has_focus = true;
        gdmf_x11_apply_input_state();
        break;

    case FocusOut:

        if (event->xfocus.mode == NotifyGrab || event->xfocus.mode == NotifyUngrab) { break; }
        gdmf_window_has_focus = false;
        gdmf_x11_apply_input_state();
        break;

    case MapNotify: {
        bool was_minimized = gdmf_window_minimized;
        gdmf_x11_window_mapped = true;
        gdmf_window_minimized = false;

        if (was_minimized) { gdmf_window_resize_pending = true; }
        break;
    }

    case UnmapNotify:

        gdmf_window_minimized = true;
        break;

    case PropertyNotify:
        if (event->xproperty.atom == gdmf_x11_atom_extended_window_state) {

            gdmf_x11_fullscreen_confirmed = gdmf_x11_is_fullscreen();
            if (!gdmf_x11_fullscreen_confirmed) { gdmf_x11_requested_mode = GDMF_MODE_WINDOWED; }

            GDMF_DisplayMode confirmed_mode =
                gdmf_x11_fullscreen_confirmed ? (gdmf_x11_requested_mode == GDMF_MODE_WINDOWED
                                ? GDMF_MODE_BORDERLESS : gdmf_x11_requested_mode)
                             : GDMF_MODE_WINDOWED;
            if (confirmed_mode != gdmf_window_display_mode) {
                gdmf_window_display_mode = confirmed_mode;
                printf("[GDMF] Display mode: %s\n",
                       confirmed_mode == GDMF_MODE_WINDOWED ? "WINDOWED" :
                       confirmed_mode == GDMF_MODE_BORDERLESS ? "BORDERLESS" : "FULLSCREEN EXCLUSIVE");
            }
        } else if (event->xproperty.atom == gdmf_x11_atom_window_state) {
            gdmf_window_minimized = gdmf_x11_is_iconic();
        }
        break;

    default:
        break;
    }

    return;
}

// Initialize Xlib threading before opening this backend connection.
static int gdmf_x11_create_window(void) {

    XInitThreads();

    gdmf_x11_display_connection = XOpenDisplay(NULL);
    if (!gdmf_x11_display_connection) {
        printf("[GDMF] XOpenDisplay failed (DISPLAY=%s)\n",
               getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");

        return -1;
    }
    gdmf_x11_screen_index = DefaultScreen(gdmf_x11_display_connection);

    gdmf_x11_atom_window_protocols = XInternAtom(gdmf_x11_display_connection, "WM_PROTOCOLS", False);
    gdmf_x11_atom_delete_window = XInternAtom(gdmf_x11_display_connection, "WM_DELETE_WINDOW", False);
    gdmf_x11_atom_window_state = XInternAtom(gdmf_x11_display_connection, "WM_STATE", False);
    gdmf_x11_atom_window_title = XInternAtom(gdmf_x11_display_connection, "_NET_WM_NAME", False);
    gdmf_x11_atom_utf8_string = XInternAtom(gdmf_x11_display_connection, "UTF8_STRING", False);
    gdmf_x11_atom_extended_window_state = XInternAtom(gdmf_x11_display_connection, "_NET_WM_STATE", False);
    gdmf_x11_atom_fullscreen = XInternAtom(gdmf_x11_display_connection, "_NET_WM_STATE_FULLSCREEN", False);
    gdmf_x11_atom_window_icon = XInternAtom(gdmf_x11_display_connection, "_NET_WM_ICON", False);

    int window_x = (DisplayWidth(gdmf_x11_display_connection, gdmf_x11_screen_index)  - gdmf_window_config_width)  / 2;
    int window_y = (DisplayHeight(gdmf_x11_display_connection, gdmf_x11_screen_index) - gdmf_window_config_height) / 2;

    XSetWindowAttributes window_attributes;
    memset(&window_attributes, 0, sizeof(window_attributes));
    window_attributes.background_pixel = BlackPixel(gdmf_x11_display_connection, gdmf_x11_screen_index);
    window_attributes.event_mask = StructureNotifyMask | FocusChangeMask | PropertyChangeMask;

    gdmf_x11_window_id = XCreateWindow(gdmf_x11_display_connection, RootWindow(gdmf_x11_display_connection, gdmf_x11_screen_index),
                             window_x, window_y, (unsigned)gdmf_window_config_width, (unsigned)gdmf_window_config_height, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask, &window_attributes);
    if (!gdmf_x11_window_id) {
        printf("[GDMF] XCreateWindow failed\n");
        gdmf_x11_destroy_window();

        return -1;
    }

    XSetWMProtocols(gdmf_x11_display_connection, gdmf_x11_window_id, &gdmf_x11_atom_delete_window, 1);

    XStoreName(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_window_config_title);
    XChangeProperty(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_x11_atom_window_title, gdmf_x11_atom_utf8_string, 8,
                    PropModeReplace, (const unsigned char*)gdmf_window_config_title,
                    (int)strlen(gdmf_window_config_title));

    XClassHint class_hint;
    class_hint.res_name = (char*)"fuselage";
    class_hint.res_class = (char*)"fuselage";
    XSetClassHint(gdmf_x11_display_connection, gdmf_x11_window_id, &class_hint);

    XSizeHints* size_hints = XAllocSizeHints();
    if (size_hints) {
        size_hints->flags = PPosition | PSize | PMinSize | PAspect;
        size_hints->x = window_x;
        size_hints->y = window_y;
        size_hints->width = gdmf_window_config_width;
        size_hints->height = gdmf_window_config_height;
        size_hints->min_width = 320;
        size_hints->min_height = 180;
        size_hints->min_aspect.x = gdmf_window_config_aspect_numerator;
        size_hints->min_aspect.y = gdmf_window_config_aspect_denominator;
        size_hints->max_aspect.x = gdmf_window_config_aspect_numerator;
        size_hints->max_aspect.y = gdmf_window_config_aspect_denominator;
        XSetWMNormalHints(gdmf_x11_display_connection, gdmf_x11_window_id, size_hints);
        XFree(size_hints);
    }

    gdmf_x11_apply_icon();

    gdmf_x11_blank_cursor = gdmf_x11_create_blank_cursor();

    gdmf_window_width = gdmf_window_config_width;
    gdmf_window_height = gdmf_window_config_height;

    XMapWindow(gdmf_x11_display_connection, gdmf_x11_window_id);
    while (!gdmf_x11_window_mapped) {
        XEvent event;
        XNextEvent(gdmf_x11_display_connection, &event);
        gdmf_x11_handle_event(&event);
    }

    XScreenSaverSuspend(gdmf_x11_display_connection, True);

    gdmf_window_has_focus = true;
    gdmf_x11_apply_input_state();

    printf("[GDMF] X11 window ready\n");

    return 0;
}

// Restore display mode and release input effects before closing the connection.
static void gdmf_x11_destroy_window(void) {
    if (!gdmf_x11_display_connection) { return; }

    gdmf_x11_leave_exclusive();

    if (gdmf_x11_pointer_grabbed) {
        XUngrabPointer(gdmf_x11_display_connection, CurrentTime);
        gdmf_x11_pointer_grabbed = false;
    }
    gdmf_x11_cursor_hidden = false;

    XScreenSaverSuspend(gdmf_x11_display_connection, False);

    if (gdmf_x11_blank_cursor != None) {
        XFreeCursor(gdmf_x11_display_connection, gdmf_x11_blank_cursor);
        gdmf_x11_blank_cursor = None;
    }
    if (gdmf_x11_window_id) {
        XDestroyWindow(gdmf_x11_display_connection, gdmf_x11_window_id);
        gdmf_x11_window_id = 0;
    }

    XCloseDisplay(gdmf_x11_display_connection);
    gdmf_x11_display_connection = NULL;

    gdmf_x11_window_mapped = false;
    gdmf_x11_fullscreen_confirmed = false;
    gdmf_x11_requested_mode = GDMF_MODE_WINDOWED;
    gdmf_x11_saved_window_width = 0;
    gdmf_x11_saved_window_height = 0;

    return;
}

// The main thread drains pending events and flushes queued requests.
static void gdmf_x11_pump(void) {
    if (!gdmf_x11_display_connection) { return; }

    while (XPending(gdmf_x11_display_connection) > 0) {
        XEvent event;
        XNextEvent(gdmf_x11_display_connection, &event);
        gdmf_x11_handle_event(&event);
    }

    XFlush(gdmf_x11_display_connection);

    return;
}

static void gdmf_x11_request_display_mode(GDMF_DisplayMode mode) {
    if (!gdmf_x11_display_connection || !gdmf_x11_window_id) { return; }
    if (mode == gdmf_window_display_mode) { return; }

    GDMF_DisplayMode previous_mode = gdmf_window_display_mode;

    if (previous_mode == GDMF_MODE_WINDOWED) {

        Window child_window;
        int window_root_x = 0, window_root_y = 0;
        XTranslateCoordinates(gdmf_x11_display_connection, gdmf_x11_window_id, RootWindow(gdmf_x11_display_connection, gdmf_x11_screen_index),
                              0, 0, &window_root_x, &window_root_y, &child_window);
        gdmf_x11_saved_window_x = window_root_x;
        gdmf_x11_saved_window_y = window_root_y;
        gdmf_x11_saved_window_width = gdmf_window_width;
        gdmf_x11_saved_window_height = gdmf_window_height;
    }

    gdmf_x11_requested_mode = mode;

    switch (mode) {

    case GDMF_MODE_WINDOWED:
        gdmf_x11_request_fullscreen(false);

        if (gdmf_x11_saved_window_width > 0 && gdmf_x11_saved_window_height > 0) {
            XMoveResizeWindow(gdmf_x11_display_connection, gdmf_x11_window_id, gdmf_x11_saved_window_x, gdmf_x11_saved_window_y,
                              (unsigned)gdmf_x11_saved_window_width, (unsigned)gdmf_x11_saved_window_height);
        }
        break;

    case GDMF_MODE_BORDERLESS:
        gdmf_x11_request_fullscreen(true);
        break;

    case GDMF_MODE_FULLSCREEN_EXCLUSIVE:

        gdmf_x11_enter_exclusive();
        gdmf_x11_request_fullscreen(true);
        break;

    }

    if (previous_mode == GDMF_MODE_FULLSCREEN_EXCLUSIVE) { gdmf_x11_leave_exclusive(); }

    bool was_fullscreen = (previous_mode != GDMF_MODE_WINDOWED);
    bool fullscreen_requested = (mode    != GDMF_MODE_WINDOWED);
    if (was_fullscreen == fullscreen_requested) {
        XSync(gdmf_x11_display_connection, False);
        gdmf_window_display_mode = mode;
        printf("[GDMF] Display mode: %s\n",
               mode == GDMF_MODE_BORDERLESS ? "BORDERLESS" : "FULLSCREEN EXCLUSIVE");
    } else {
        XFlush(gdmf_x11_display_connection);
    }

    return;
}

static void gdmf_x11_input_state_changed(void) {
    gdmf_x11_apply_input_state();

    return;
}

// Query window-relative position. The XQueryPointer result is not yet checked.
static bool gdmf_x11_get_mouse_client(int* mouse_x, int* mouse_y) {
    if (!gdmf_x11_display_connection || !gdmf_x11_window_id) { return false; }

    Window root_window, child_window;
    int root_x = 0, root_y = 0, window_x = 0, window_y = 0;
    unsigned int input_mask = 0;

    XQueryPointer(gdmf_x11_display_connection, gdmf_x11_window_id, &root_window, &child_window,
                  &root_x, &root_y, &window_x, &window_y, &input_mask);

    *mouse_x = window_x;
    *mouse_y = window_y;

    return true;
}

void* GDMF_GetX11Display(void) { return (void*)gdmf_x11_display_connection; }
void* GDMF_GetX11Window(void)  { return (void*)(uintptr_t)gdmf_x11_window_id; }

static FuselageLinuxNativeHandle gdmf_x11_native_handle;
static void* gdmf_x11_get_native_handle(void) {
    gdmf_x11_native_handle.backend_name = "x11";
    gdmf_x11_native_handle.display_connection = (void*)gdmf_x11_display_connection;
    gdmf_x11_native_handle.window_id = (void*)(uintptr_t)gdmf_x11_window_id;

    return (void*)&gdmf_x11_native_handle;
}

const gdmf_linux_window_backend gdmf_linux_window_backend_x11 = {
    .name = "x11",
    .create = gdmf_x11_create_window,
    .destroy = gdmf_x11_destroy_window,
    .pump = gdmf_x11_pump,
    .request_display_mode = gdmf_x11_request_display_mode,
    .input_state_changed = gdmf_x11_input_state_changed,
    .get_mouse_client = gdmf_x11_get_mouse_client,
    .get_native_handle = gdmf_x11_get_native_handle,
};
