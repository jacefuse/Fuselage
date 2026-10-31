// AppKit event observation. SEE: (CAKE.md, macOS input and controllers)
#include "cake_macos_private.h"
#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
static BOOL cake_macos_previous_mouse_moves;
static NSWindow* cake_macos_window = nil;
static id cake_macos_event_monitor = nil;

bool cake_macos_attach_input(void* native_window) {
    if (cake_macos_event_monitor) {
        return cake_macos_window == (__bridge NSWindow*)native_window;
    }

    cake_macos_window = (__bridge NSWindow*)native_window;

    cake_macos_previous_mouse_moves = cake_macos_window.acceptsMouseMovedEvents;
    [cake_macos_window setAcceptsMouseMovedEvents:YES];

    NSEventMask mask = NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged |
                       NSEventMaskMouseMoved | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp |
                       NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDown | NSEventMaskRightMouseUp |
                       NSEventMaskRightMouseDragged | NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp |
                       NSEventMaskOtherMouseDragged | NSEventMaskScrollWheel;

    cake_macos_event_monitor = [NSEvent
        addLocalMonitorForEventsMatchingMask:mask
                                     handler:^NSEvent*(NSEvent* event) {
                                       if (event.window && event.window != cake_macos_window) {
                                           return event;
                                       }

                                       switch (event.type) {

                                       case NSEventTypeKeyDown:

                                           if (!event.isARepeat) {
                                               cake_macos_intake_key(event.keyCode, true);
                                           }
                                           break;

                                       case NSEventTypeKeyUp:
                                           cake_macos_intake_key(event.keyCode, false);
                                           break;

                                       case NSEventTypeFlagsChanged:
                                           cake_macos_intake_flags(
                                               event.modifierFlags & 0xFFFF,
                                               (event.modifierFlags & NSEventModifierFlagCapsLock) != 0);
                                           break;

                                       case NSEventTypeMouseMoved:
                                       case NSEventTypeLeftMouseDragged:
                                       case NSEventTypeRightMouseDragged:
                                       case NSEventTypeOtherMouseDragged:
                                           cake_macos_intake_motion(event.deltaX, event.deltaY);
                                           break;

                                       case NSEventTypeLeftMouseDown:
                                           cake_macos_intake_button(0, true);
                                           break;
                                       case NSEventTypeLeftMouseUp:
                                           cake_macos_intake_button(0, false);
                                           break;
                                       case NSEventTypeRightMouseDown:
                                           cake_macos_intake_button(1, true);
                                           break;
                                       case NSEventTypeRightMouseUp:
                                           cake_macos_intake_button(1, false);
                                           break;
                                       case NSEventTypeOtherMouseDown:
                                           cake_macos_intake_button((int)event.buttonNumber, true);
                                           break;
                                       case NSEventTypeOtherMouseUp:
                                           cake_macos_intake_button((int)event.buttonNumber, false);
                                           break;

                                       case NSEventTypeScrollWheel:
                                           cake_macos_intake_scroll(event.scrollingDeltaY,
                                                                    event.hasPreciseScrollingDeltas,
                                                                    event.deltaY);
                                           break;

                                       default:
                                           break;
                                       }

                                       return event;
                                     }];

    if (!cake_macos_event_monitor) {
        [cake_macos_window setAcceptsMouseMovedEvents:cake_macos_previous_mouse_moves];
        cake_macos_window = nil;
        return false;
    }
    return true;
}

void cake_macos_detach_input(void) {
    if (cake_macos_event_monitor) {
        [NSEvent removeMonitor:cake_macos_event_monitor];
        cake_macos_event_monitor = nil;
    }
    [cake_macos_window setAcceptsMouseMovedEvents:cake_macos_previous_mouse_moves];
    cake_macos_window = nil;

    return;
}

bool cake_macos_is_main_thread(void) {
    return NSThread.isMainThread;
}

#endif
