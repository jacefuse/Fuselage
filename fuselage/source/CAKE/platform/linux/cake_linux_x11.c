// XInput2 raw intake. SEE: (CAKE.md, Linux input)
#include "cake_linux_private.h"
#if defined(__linux__)
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
static Display* cake_linux_x11_display;
static int cake_linux_x11_opcode;
static void cake_linux_x11_handle_raw(const XIRawEvent* raw) {
    if (!raw) {
        return;
    }
    switch (raw->evtype) {
    case XI_RawKeyPress:
    case XI_RawKeyRelease:
        if (raw->detail >= 8 && raw->detail <= 263) {
            cake_linux_intake_key((uint32_t)(raw->detail - 8), raw->evtype == XI_RawKeyPress);
        }
        return;
    case XI_RawButtonPress:
    case XI_RawButtonRelease: {
        bool down = raw->evtype == XI_RawButtonPress;
        int button;
        switch (raw->detail) {
        case 1:
            button = CAKE_MOUSE_LEFT;
            break;
        case 2:
            button = CAKE_MOUSE_MIDDLE;
            break;
        case 3:
            button = CAKE_MOUSE_RIGHT;
            break;
        case 4:
        case 5:
            if (down) {
                cake_linux_intake_scroll(raw->detail == 4 ? 120.0 : -120.0);
            }
            return;
        case 8:
            button = CAKE_MOUSE_X1;
            break;
        case 9:
            button = CAKE_MOUSE_X2;
            break;
        default:
            return;
        }
        cake_linux_intake_button(button, down);
        return;
    }
    case XI_RawMotion: {
        if (raw->valuators.mask_len <= 0 || !raw->valuators.mask || !raw->raw_values) {
            return;
        }
        const double* value = raw->raw_values;
        double delta_x = 0, delta_y = 0;
        if (XIMaskIsSet(raw->valuators.mask, 0)) {
            delta_x = *value++;
        }
        if (XIMaskIsSet(raw->valuators.mask, 1)) {
            delta_y = *value;
        }
        cake_linux_intake_motion(delta_x, delta_y);
        return;
    }
    default:
        return;
    }
}

void cake_linux_x11_drain(void) {
    while (cake_linux_x11_display && XPending(cake_linux_x11_display) > 0) {
        XEvent event;
        XNextEvent(cake_linux_x11_display, &event);

        if (event.xcookie.type == GenericEvent && event.xcookie.extension == cake_linux_x11_opcode &&
            XGetEventData(cake_linux_x11_display, &event.xcookie)) {
            cake_linux_x11_handle_raw((const XIRawEvent*)event.xcookie.data);
            XFreeEventData(cake_linux_x11_display, &event.xcookie);
        }
    }

    return;
}

bool cake_linux_x11_attach(void* display_connection) {
    if (!display_connection || cake_linux_x11_display) {
        return false;
    }
    cake_linux_x11_display = XOpenDisplay(DisplayString((Display*)display_connection));
    if (!cake_linux_x11_display) {
        printf("[CAKE] Warning: XOpenDisplay failed; no keyboard/mouse\n");
        return false;
    }

    int event, error;
    if (!XQueryExtension(cake_linux_x11_display, "XInputExtension", &cake_linux_x11_opcode, &event, &error)) {
        printf("[CAKE] Warning: no XInput extension; no keyboard/mouse\n");
        XCloseDisplay(cake_linux_x11_display);
        cake_linux_x11_display = NULL;
        return false;
    }

    int major = 2;
    int minor = 3;
    if (XIQueryVersion(cake_linux_x11_display, &major, &minor) != Success || major < 2) {
        printf("[CAKE] Warning: XInput2 unavailable (server has %d.%d); no keyboard/mouse\n", major, minor);
        XCloseDisplay(cake_linux_x11_display);
        cake_linux_x11_display = NULL;
        return false;
    }

    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};
    XIEventMask event_mask = {
        .deviceid = XIAllMasterDevices,
        .mask_len = sizeof(mask),
        .mask = mask,
    };
    XISetMask(mask, XI_RawKeyPress);
    XISetMask(mask, XI_RawKeyRelease);
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawButtonRelease);
    XISetMask(mask, XI_RawMotion);
    if (XISelectEvents(cake_linux_x11_display, DefaultRootWindow(cake_linux_x11_display), &event_mask, 1) !=
        Success) {
        cake_linux_x11_detach();
        return false;
    }
    XFlush(cake_linux_x11_display);

    return true;
}

void cake_linux_x11_detach(void) {
    if (cake_linux_x11_display) {
        XCloseDisplay(cake_linux_x11_display);
        cake_linux_x11_display = NULL;
    }
    cake_linux_x11_opcode = 0;
}
#endif
