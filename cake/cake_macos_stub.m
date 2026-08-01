// CAKE -- macOS input stub.
// The minimal Objective-C layer for window-attached input: one NSEvent
// local monitor scoped to the NSWindow handed over by CAKE_AttachWindow.
// Local monitors fire inside the application's own [NSApp sendEvent:] pump
// (GDMF's, on the main thread) -- no hidden window, no process-global tap,
// no Input Monitoring permission. Trackpads need nothing special: the OS
// delivers their motion as ordinary mouse moved/dragged deltas and their
// two-finger scroll as precise scrollWheel events.
//
// No engine state lives here; every observation is passed straight to the
// cake_macos_intake_* functions in cake.c (which own the pending state and
// its lock) and the event is returned unmodified so normal dispatch -- menu
// key equivalents, window moves, GDMF's view -- continues untouched. Keep
// it that way: if a change here starts holding state or making policy, it
// belongs in the C half instead.

#import <Cocoa/Cocoa.h>
#include <stdbool.h>
#include "cake.h"   // CAKE_ControllerState + CAKE_BUTTON_* for the controller reads

// Intake, implemented in cake.c (main-thread callers, sim-thread drain)
void cake_macos_intake_key(unsigned short keyCode, bool down);
void cake_macos_intake_flags(unsigned long deviceFlags, bool capsLock);
void cake_macos_intake_button(int buttonNumber, bool down);
void cake_macos_intake_motion(double dx, double dy);
void cake_macos_intake_scroll(double scrollingDeltaY, bool precise, double lineDeltaY);

static NSWindow* g_cakeWindow  = nil;
static id        g_cakeMonitor = nil;

void cake_macos_stub_attach(void* nsWindow) {
    if (g_cakeMonitor) { return; }   // one attachment, matching one app window

    g_cakeWindow = (__bridge NSWindow*)nsWindow;

    // Plain cursor movement (no button held) is only delivered if the
    // window opts in -- CAKE needs it for the motion deltas, so the
    // attached window is configured here rather than asking the window
    // layer to know about input's needs.
    [g_cakeWindow setAcceptsMouseMovedEvents:YES];

    NSEventMask mask =
        NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged |
        NSEventMaskMouseMoved |
        NSEventMaskLeftMouseDown  | NSEventMaskLeftMouseUp  | NSEventMaskLeftMouseDragged |
        NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskRightMouseDragged |
        NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp | NSEventMaskOtherMouseDragged |
        NSEventMaskScrollWheel;

    g_cakeMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                                                          handler:^NSEvent* (NSEvent* event) {
        // Only the attached window's input is CAKE's business. Events that
        // carry no window (some flagsChanged) are keyboard-global and kept.
        if (event.window && event.window != g_cakeWindow) { return event; }

        switch (event.type) {

        case NSEventTypeKeyDown:
            // Auto-repeat is a text-input affordance; a state table wants
            // only the real down edge (Raw Input never repeats either).
            if (!event.isARepeat) { cake_macos_intake_key(event.keyCode, true); }
            break;

        case NSEventTypeKeyUp:
            cake_macos_intake_key(event.keyCode, false);
            break;

        case NSEventTypeFlagsChanged:
            cake_macos_intake_flags(event.modifierFlags & 0xFFFF,
                                    (event.modifierFlags & NSEventModifierFlagCapsLock) != 0);
            break;

        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
            cake_macos_intake_motion(event.deltaX, event.deltaY);
            break;

        case NSEventTypeLeftMouseDown:   cake_macos_intake_button(0, true);  break;
        case NSEventTypeLeftMouseUp:     cake_macos_intake_button(0, false); break;
        case NSEventTypeRightMouseDown:  cake_macos_intake_button(1, true);  break;
        case NSEventTypeRightMouseUp:    cake_macos_intake_button(1, false); break;
        case NSEventTypeOtherMouseDown:  cake_macos_intake_button((int)event.buttonNumber, true);  break;
        case NSEventTypeOtherMouseUp:    cake_macos_intake_button((int)event.buttonNumber, false); break;

        case NSEventTypeScrollWheel:
            cake_macos_intake_scroll(event.scrollingDeltaY,
                                     event.hasPreciseScrollingDeltas,
                                     event.deltaY);
            break;

        default:
            break;
        }

        return event;   // always: CAKE observes, never consumes
    }];

    return;
}

void cake_macos_stub_detach(void) {
    if (g_cakeMonitor) {
        [NSEvent removeMonitor:g_cakeMonitor];
        g_cakeMonitor = nil;
    }
    g_cakeWindow = nil;

    return;
}

// --- Controllers -------------------------------------------------------------
// GameController.framework: the OS's own mapping layer, so Xbox/PlayStation/
// Switch pads arrive with A/B/X/Y, sticks, triggers, and D-pad already in
// their correct positions -- no per-device tables, no permissions. Slot
// policy and the connection lifecycle live in cake.c (the
// cake_macos_gc_connected/_disconnected callbacks); this side only holds
// strong references while a controller is present so the opaque pointers
// cake.c keeps stay valid, and reads one snapshot per poll.

#import <GameController/GameController.h>

// Callbacks implemented in cake.c (fire on the main thread)
void cake_macos_gc_connected(void* gc, const char* name);
void cake_macos_gc_disconnected(void* gc);

static NSMutableArray<GCController*>* g_gcHeld           = nil;
static id                             g_gcConnectObs     = nil;
static id                             g_gcDisconnectObs  = nil;
static NSTimer*                       g_gcReconcileTimer = nil;

static void cake_gc_report(GCController* controller) {
    // Only full gamepads map onto CAKE's XInput-shaped state. (Remotes and
    // other micro profiles would arrive mostly-empty and lie about it.)
    if (!controller.extendedGamepad) { return; }
    if ([g_gcHeld containsObject:controller]) { return; }

    [g_gcHeld addObject:controller];

    const char* name = controller.vendorName.UTF8String;
    cake_macos_gc_connected((__bridge void*)controller, name ? name : "Gamepad");

    return;
}

// Diffs the framework's live controller list against ours and reports the
// differences up. This is the safety net, not the primary path: a pad that
// powered itself off and back on can come back as a brand-new controller
// object, and under a manually pumped event loop a notification delivery is
// not something to bet the reconnect on. The Windows backend runs the exact
// same fallback re-enumeration on a 2-second timer for the same reason.
static void cake_gc_reconcile(void) {
    NSArray<GCController*>* live = GCController.controllers;

    for (GCController* held in [g_gcHeld copy]) {
        if (![live containsObject:held]) {
            cake_macos_gc_disconnected((__bridge void*)held);
            [g_gcHeld removeObject:held];
        }
    }

    for (GCController* controller in live) {
        cake_gc_report(controller);   // skips ones already held
    }

    return;
}

void cake_macos_gc_start(void) {
    if (g_gcHeld) { return; }

    g_gcHeld = [NSMutableArray array];

    // queue:nil = deliver synchronously on the posting thread (these post on
    // the main thread). Queueing onto NSOperationQueue.mainQueue instead
    // proved unreliable under the manually pumped event loop -- the queue's
    // drain isn't guaranteed there, and a reconnect notification that never
    // drains is a controller that never comes back.
    g_gcConnectObs = [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidConnectNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* note) {
        cake_gc_report(note.object);
    }];

    g_gcDisconnectObs = [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidDisconnectNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* note) {
        GCController* controller = note.object;
        if ([g_gcHeld containsObject:controller]) {
            cake_macos_gc_disconnected((__bridge void*)controller);
            [g_gcHeld removeObject:controller];
        }
    }];

    // Fallback reconciliation, scheduled in common modes so it keeps firing
    // through tracking loops (window drags) as well as the normal pump.
    g_gcReconcileTimer = [NSTimer timerWithTimeInterval:2.0
                                                repeats:YES
                                                  block:^(NSTimer* timer) {
        (void)timer;
        cake_gc_reconcile();
    }];
    [[NSRunLoop mainRunLoop] addTimer:g_gcReconcileTimer forMode:NSRunLoopCommonModes];

    // Anything already connected before monitoring began
    for (GCController* controller in GCController.controllers) {
        cake_gc_report(controller);
    }

    return;
}

void cake_macos_gc_stop(void) {
    if (g_gcReconcileTimer) { [g_gcReconcileTimer invalidate]; g_gcReconcileTimer = nil; }
    if (g_gcConnectObs)    { [[NSNotificationCenter defaultCenter] removeObserver:g_gcConnectObs];    g_gcConnectObs = nil; }
    if (g_gcDisconnectObs) { [[NSNotificationCenter defaultCenter] removeObserver:g_gcDisconnectObs]; g_gcDisconnectObs = nil; }
    g_gcHeld = nil;

    return;
}

static int16_t cake_gc_axis(float value) {
    // -1..1 float to the XInput-shaped -32768..32767. The framework already
    // reports +Y as up, which is CAKE's convention too.
    float scaled = value * 32767.0f;
    if (scaled >  32767.0f) { scaled =  32767.0f; }
    if (scaled < -32768.0f) { scaled = -32768.0f; }

    return (int16_t)scaled;
}

bool cake_macos_gc_read(void* gcOpaque, CAKE_ControllerState* out) {
    GCController*      controller = (__bridge GCController*)gcOpaque;
    GCExtendedGamepad* pad        = controller.extendedGamepad;
    if (!pad) { return false; }

    uint16_t buttons = 0;
    if (pad.buttonA.pressed)              { buttons |= CAKE_BUTTON_A; }
    if (pad.buttonB.pressed)              { buttons |= CAKE_BUTTON_B; }
    if (pad.buttonX.pressed)              { buttons |= CAKE_BUTTON_X; }
    if (pad.buttonY.pressed)              { buttons |= CAKE_BUTTON_Y; }
    if (pad.leftShoulder.pressed)         { buttons |= CAKE_BUTTON_LEFT_SHOULDER; }
    if (pad.rightShoulder.pressed)        { buttons |= CAKE_BUTTON_RIGHT_SHOULDER; }
    if (pad.dpad.up.pressed)              { buttons |= CAKE_BUTTON_DPAD_UP; }
    if (pad.dpad.down.pressed)            { buttons |= CAKE_BUTTON_DPAD_DOWN; }
    if (pad.dpad.left.pressed)            { buttons |= CAKE_BUTTON_DPAD_LEFT; }
    if (pad.dpad.right.pressed)           { buttons |= CAKE_BUTTON_DPAD_RIGHT; }
    // Objective-C's nil-messaging makes the newer, nullable buttons safe on
    // pads that lack them: a missing button simply reads as not-pressed.
    if (pad.buttonMenu.pressed)           { buttons |= CAKE_BUTTON_START; }
    if (pad.buttonOptions.pressed)        { buttons |= CAKE_BUTTON_BACK; }
    if (pad.leftThumbstickButton.pressed)  { buttons |= CAKE_BUTTON_LEFT_THUMB; }
    if (pad.rightThumbstickButton.pressed) { buttons |= CAKE_BUTTON_RIGHT_THUMB; }

    out->buttons      = buttons;
    out->leftTrigger  = (uint8_t)(pad.leftTrigger.value  * 255.0f);
    out->rightTrigger = (uint8_t)(pad.rightTrigger.value * 255.0f);
    out->thumbLeftX   = cake_gc_axis(pad.leftThumbstick.xAxis.value);
    out->thumbLeftY   = cake_gc_axis(pad.leftThumbstick.yAxis.value);
    out->thumbRightX  = cake_gc_axis(pad.rightThumbstick.xAxis.value);
    out->thumbRightY  = cake_gc_axis(pad.rightThumbstick.yAxis.value);

    return true;
}
