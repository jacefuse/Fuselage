// GDMF -- macOS window backend, Objective-C stub.
// The minimal AppKit layer: only what plain C cannot do -- NSApplication
// bring-up, the NSWindow and its CAMetalLayer-backed content view, the
// event pump, display modes, and cursor capture/visibility. No engine state
// lives here; every event is reported up through the gdmf_macos_notify_*
// callbacks implemented in gdmf_window_macos.c, and every decision (what to
// apply, when) is made there. Keep it that way: if a change here starts
// holding state or making policy, it belongs in the C half instead.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <stdbool.h>

// Callbacks implemented in gdmf_window_macos.c
void gdmf_macos_notify_resize(int pixelWidth, int pixelHeight);
void gdmf_macos_notify_close(void);
void gdmf_macos_notify_focus(bool focused);
void gdmf_macos_notify_minimized(bool minimized);
void gdmf_macos_notify_display_mode(int mode);

// GDMF_DisplayMode values (kept numerically in sync with gdmf.h -- the enum
// itself isn't imported to keep this stub free of engine headers)
#define GDMF_MACOS_MODE_WINDOWED   0
#define GDMF_MACOS_MODE_BORDERLESS 1
#define GDMF_MACOS_MODE_EXCLUSIVE  2

static NSWindow*     g_window       = nil;
static CAMetalLayer* g_metalLayer   = nil;
static NSView*       g_view         = nil;
static id            g_delegate     = nil;   // GDMFWindowDelegate, retained
static bool          g_cursorHidden = false;
static int           g_displayMode  = GDMF_MACOS_MODE_WINDOWED;
static NSRect        g_savedFrame;           // windowed frame, for leaving borderless
static NSUInteger    g_savedStyle   = 0;

// Keeps the CAMetalLayer's drawableSize matched to the view's backing-pixel
// size and reports it up -- called on live resize and backing-scale changes.
static void gdmf_macos_sync_drawable_size(void) {
    if (!g_view || !g_metalLayer) { return; }

    NSSize backing = [g_view convertSizeToBacking:g_view.bounds.size];
    g_metalLayer.contentsScale = g_window.backingScaleFactor;
    g_metalLayer.drawableSize  = CGSizeMake(backing.width, backing.height);

    gdmf_macos_notify_resize((int)backing.width, (int)backing.height);

    return;
}

// Content view: exists only to back itself with a CAMetalLayer and to
// swallow key events (input is CAKE's job via HID; without this every
// keypress beeps as "unhandled").
@interface GDMFView : NSView
@end

@implementation GDMFView
- (BOOL)wantsUpdateLayer { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (CALayer*)makeBackingLayer { return [CAMetalLayer layer]; }
- (void)keyDown:(NSEvent*)event { (void)event; }
- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    gdmf_macos_sync_drawable_size();
}
@end

@interface GDMFWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation GDMFWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    // Mirror the Win32 WM_CLOSE behavior: flag the request and let the
    // engine drive the actual teardown (GDMF_Shutdown -> stub_destroy).
    gdmf_macos_notify_close();
    return NO;
}
- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    gdmf_macos_sync_drawable_size();
}
- (void)windowDidBecomeKey:(NSNotification*)notification {
    (void)notification;
    gdmf_macos_notify_focus(true);
}
- (void)windowDidResignKey:(NSNotification*)notification {
    (void)notification;
    gdmf_macos_notify_focus(false);
}
- (void)windowDidMiniaturize:(NSNotification*)notification {
    (void)notification;
    gdmf_macos_notify_minimized(true);
}
- (void)windowDidDeminiaturize:(NSNotification*)notification {
    (void)notification;
    gdmf_macos_notify_minimized(false);
}
- (void)quitRequested:(id)sender {
    (void)sender;
    // The app menu's Quit item -- routed through the same close-request path
    // as the window's close button, never a hard [NSApp terminate:].
    gdmf_macos_notify_close();
}
@end

// Minimal menu bar: just the app menu with a working Cmd+Q that requests a
// clean engine shutdown. Without any menu, Cmd+Q does nothing at all.
static void gdmf_macos_build_menu(NSString* appName) {
    NSMenu* menubar = [[NSMenu alloc] init];
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];

    NSMenu* appMenu = [[NSMenu alloc] init];
    NSMenuItem* quitItem =
        [[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:appName]
                                   action:@selector(quitRequested:)
                            keyEquivalent:@"q"];
    [quitItem setTarget:g_delegate];
    [appMenu addItem:quitItem];
    [appMenuItem setSubmenu:appMenu];

    [NSApp setMainMenu:menubar];

    return;
}

// Dock icon from the engine's top-down RGBA8 buffer, if one was configured.
static void gdmf_macos_apply_icon(const unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) { return; }

    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:w
                      pixelsHigh:h
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:w * 4
                    bitsPerPixel:32];
    if (!rep) { return; }

    memcpy([rep bitmapData], rgba, (size_t)w * (size_t)h * 4);

    NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(w, h)];
    [image addRepresentation:rep];
    [NSApp setApplicationIconImage:image];

    return;
}

int gdmf_macos_stub_create(const char* title, int width, int height,
                           int aspectNum, int aspectDen,
                           const unsigned char* iconRGBA, int iconW, int iconH) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        g_delegate = [[GDMFWindowDelegate alloc] init];

        NSString* appName = [NSString stringWithUTF8String:(title ? title : "Fuselage")];
        gdmf_macos_build_menu(appName);

        NSRect contentRect = NSMakeRect(0, 0, width, height);
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

        g_window = [[NSWindow alloc] initWithContentRect:contentRect
                                               styleMask:style
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        if (!g_window) { return -1; }

        [g_window setTitle:appName];
        [g_window setDelegate:g_delegate];
        [g_window setContentAspectRatio:NSMakeSize(aspectNum, aspectDen)];
        [g_window setContentMinSize:NSMakeSize(320, 180)];  // matches the Win32 backend
        [g_window setReleasedWhenClosed:NO];                // teardown is stub_destroy's job

        g_view = [[GDMFView alloc] initWithFrame:contentRect];
        [g_view setWantsLayer:YES];
        g_metalLayer = (CAMetalLayer*)[g_view layer];
        if (!g_metalLayer) { return -1; }

        [g_window setContentView:g_view];
        [g_window center];
        [g_window makeKeyAndOrderFront:nil];

        gdmf_macos_apply_icon(iconRGBA, iconW, iconH);

        [NSApp finishLaunching];
        if (@available(macOS 14.0, *)) {
            [NSApp activate];
        } else {
            [NSApp activateIgnoringOtherApps:YES];
        }

        gdmf_macos_sync_drawable_size();
    }

    return 0;
}

void gdmf_macos_stub_destroy(void) {
    @autoreleasepool {
        if (g_cursorHidden) {
            [NSCursor unhide];
            g_cursorHidden = false;
        }
        CGAssociateMouseAndMouseCursorPosition(true);

        if (g_window) {
            [g_window setDelegate:nil];
            [g_window close];
            g_window = nil;
        }
        g_view       = nil;
        g_metalLayer = nil;
        g_delegate   = nil;
    }

    return;
}

// Non-blocking: drains whatever events are pending and returns. Called once
// per GDMF_Tick on the main thread.
void gdmf_macos_stub_pump(void) {
    @autoreleasepool {
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:nil
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES])) {
            [NSApp sendEvent:event];
        }
    }

    return;
}

void* gdmf_macos_stub_metal_layer(void) {
    return (__bridge void*)g_metalLayer;
}

void* gdmf_macos_stub_nswindow(void) {
    return (__bridge void*)g_window;
}

void gdmf_macos_stub_set_display_mode(int mode) {
    // May be called from the game/sim thread; AppKit work must happen on the
    // main thread, so hop there asynchronously (mirrors the Win32 backend's
    // PostMessage to its window thread).
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            if (!g_window || mode == g_displayMode) { return; }

            if (g_displayMode == GDMF_MACOS_MODE_WINDOWED) {
                g_savedFrame = [g_window frame];
                g_savedStyle = [g_window styleMask];
            }

            switch (mode) {

            case GDMF_MACOS_MODE_WINDOWED: {
                [g_window setStyleMask:g_savedStyle];
                [g_window setFrame:g_savedFrame display:YES];
                [g_window setLevel:NSNormalWindowLevel];
                [NSApp setPresentationOptions:NSApplicationPresentationDefault];
                printf("[GDMF] Display mode: WINDOWED\n");
                break;
            }

            // No exclusive display capture on modern macOS -- the compositor
            // always owns the screen, so both non-windowed modes are the
            // borderless-cover-the-screen shape (the renderer letterboxes).
            case GDMF_MACOS_MODE_BORDERLESS:
            case GDMF_MACOS_MODE_EXCLUSIVE: {
                NSScreen* screen = [g_window screen] ?: [NSScreen mainScreen];
                [g_window setStyleMask:NSWindowStyleMaskBorderless];
                [g_window setFrame:[screen frame] display:YES];
                [g_window setLevel:NSMainMenuWindowLevel + 1];
                [NSApp setPresentationOptions:NSApplicationPresentationHideDock |
                                              NSApplicationPresentationHideMenuBar];
                [g_window makeKeyAndOrderFront:nil];
                printf(mode == GDMF_MACOS_MODE_BORDERLESS
                           ? "[GDMF] Display mode: BORDERLESS\n"
                           : "[GDMF] Display mode: FULLSCREEN EXCLUSIVE (borderless on macOS)\n");
                break;
            }

            default:
                return;
            }

            g_displayMode = mode;
            gdmf_macos_notify_display_mode(mode);
            gdmf_macos_sync_drawable_size();
        }
    });

    return;
}

// The C half already resolved policy (focus etc.) into the two bools.
// macOS has no cursor-clipping rectangle; capture is done by pinning the
// cursor (disassociating it from mouse movement), which pairs with CAKE's
// raw HID deltas the same way ClipCursor pairs with Raw Input on Windows.
void gdmf_macos_stub_apply_input_state(bool capture, bool cursorVisible) {
    dispatch_async(dispatch_get_main_queue(), ^{
        CGAssociateMouseAndMouseCursorPosition(capture ? false : true);

        if (!cursorVisible && !g_cursorHidden) {
            [NSCursor hide];
            g_cursorHidden = true;
        } else if (cursorVisible && g_cursorHidden) {
            [NSCursor unhide];
            g_cursorHidden = false;
        }
    });

    return;
}

// Current cursor position in the view's backing pixels, top-left origin --
// the same space as the reported window size.
bool gdmf_macos_stub_mouse_client(int* x, int* y) {
    if (!g_window || !g_view) { return false; }

    @autoreleasepool {
        NSPoint screenPoint = [NSEvent mouseLocation];
        NSRect  screenRect  = NSMakeRect(screenPoint.x, screenPoint.y, 0, 0);
        NSPoint windowPoint = [g_window convertRectFromScreen:screenRect].origin;
        NSPoint viewPoint   = [g_view convertPoint:windowPoint fromView:nil];

        // AppKit's origin is bottom-left; the engine's is top-left.
        viewPoint.y = g_view.bounds.size.height - viewPoint.y;

        NSPoint backing = [g_view convertPointToBacking:viewPoint];

        *x = (int)backing.x;
        *y = (int)backing.y;
    }

    return true;
}
