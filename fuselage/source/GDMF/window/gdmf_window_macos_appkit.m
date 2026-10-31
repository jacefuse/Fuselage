// macOS AppKit window lifecycle, events, drawable sizing, and coordinates.
#include "gdmf_window_macos_internal.h"
#import <QuartzCore/CAMetalLayer.h>
#include <stdio.h>
#include <string.h>

NSWindow* gdmf_macos_window = nil;
NSView* gdmf_macos_content_view = nil;
static CAMetalLayer* gdmf_macos_metal_layer = nil;
static id gdmf_macos_window_delegate = nil;

// Keep drawable dimensions in backing pixels through resize and scale changes.
void gdmf_macos_sync_drawable_size(void) {
    if (!gdmf_macos_content_view || !gdmf_macos_metal_layer) { return; }

    NSSize backing_pixels = [gdmf_macos_content_view convertSizeToBacking:gdmf_macos_content_view.bounds.size];
    gdmf_macos_metal_layer.contentsScale = gdmf_macos_window.backingScaleFactor;
    gdmf_macos_metal_layer.drawableSize  = CGSizeMake(backing_pixels.width, backing_pixels.height);

    gdmf_macos_notify_resize((int)backing_pixels.width, (int)backing_pixels.height);

    return;
}
// Consume key events here; the input system handles application input.
// SEE: (GDMF_MACOS.md, Responder handling).
@interface GDMFMacOSMetalView : NSView
@end

@implementation GDMFMacOSMetalView
- (BOOL)wantsUpdateLayer { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (CALayer*)makeBackingLayer { return [CAMetalLayer layer]; }
- (void)keyDown:(NSEvent*)event { (void)event; }
- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    gdmf_macos_sync_drawable_size();
}
@end
// Borderless windows must remain eligible for keyboard focus.
// SEE: (GDMF_MACOS.md, Responder handling).
@interface GDMFMacOSWindow : NSWindow
@end

@implementation GDMFMacOSWindow
- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface GDMFMacOSWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation GDMFMacOSWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    // Let the engine perform teardown instead of terminating AppKit immediately.
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
- (void)requestQuit:(id)sender {
    (void)sender;
    // Let the engine perform teardown instead of terminating AppKit immediately.
    gdmf_macos_notify_close();
}
@end
static void gdmf_macos_build_menu(NSString* application_name) {
    NSMenu* menu_bar = [[NSMenu alloc] init];
    NSMenuItem* application_menu_item = [[NSMenuItem alloc] init];
    [menu_bar addItem:application_menu_item];

    NSMenu* application_menu = [[NSMenu alloc] init];
    NSMenuItem* quit_menu_item =
        [[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:application_name]
                                   action:@selector(requestQuit:)
                            keyEquivalent:@"q"];
    [quit_menu_item setTarget:gdmf_macos_window_delegate];
    [application_menu addItem:quit_menu_item];
    [application_menu_item setSubmenu:application_menu];

    [NSApp setMainMenu:menu_bar];

    return;
}
static void gdmf_macos_apply_application_icon(const unsigned char* icon_rgba, int icon_width, int icon_height) {
    if (!icon_rgba || icon_width <= 0 || icon_height <= 0) { return; }

    NSBitmapImageRep* bitmap_representation = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:icon_width
                      pixelsHigh:icon_height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:icon_width * 4
                    bitsPerPixel:32];
    if (!bitmap_representation) { return; }

    memcpy([bitmap_representation bitmapData], icon_rgba, (size_t)icon_width * (size_t)icon_height * 4);

    NSImage* icon_image = [[NSImage alloc] initWithSize:NSMakeSize(icon_width, icon_height)];
    [icon_image addRepresentation:bitmap_representation];
    [NSApp setApplicationIconImage:icon_image];

    return;
}

int gdmf_macos_appkit_create(const char* title, int width, int height,
                           int aspect_numerator, int aspect_denominator,
                           const unsigned char* icon_rgba, int icon_width, int icon_height) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        gdmf_macos_window_delegate = [[GDMFMacOSWindowDelegate alloc] init];

        NSString* application_name = [NSString stringWithUTF8String:(title ? title : "Fuselage")];
        gdmf_macos_build_menu(application_name);

        NSRect content_rect = NSMakeRect(0, 0, width, height);
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

        gdmf_macos_window = [[GDMFMacOSWindow alloc] initWithContentRect:content_rect
                                               styleMask:style
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        if (!gdmf_macos_window) { return -1; }

        [gdmf_macos_window setTitle:application_name];
        [gdmf_macos_window setDelegate:gdmf_macos_window_delegate];
        [gdmf_macos_window setContentAspectRatio:NSMakeSize(aspect_numerator, aspect_denominator)];
        [gdmf_macos_window setContentMinSize:NSMakeSize(320, 180)];  // matches the Win32 backend
        [gdmf_macos_window setReleasedWhenClosed:NO]; // engine owns teardown

        gdmf_macos_content_view = [[GDMFMacOSMetalView alloc] initWithFrame:content_rect];
        [gdmf_macos_content_view setWantsLayer:YES];
        gdmf_macos_metal_layer = (CAMetalLayer*)[gdmf_macos_content_view layer];
        if (!gdmf_macos_metal_layer) { return -1; }

        [gdmf_macos_window setContentView:gdmf_macos_content_view];
        [gdmf_macos_window center];
        [gdmf_macos_window makeKeyAndOrderFront:nil];
        [gdmf_macos_window makeFirstResponder:gdmf_macos_content_view];

        gdmf_macos_apply_application_icon(icon_rgba, icon_width, icon_height);

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

// Main-thread teardown; discard presentation state before releasing the window.
void gdmf_macos_appkit_destroy(void) {
    @autoreleasepool {
        gdmf_macos_restore_presentation();

        if (gdmf_macos_window) {
            [gdmf_macos_window setDelegate:nil];
            [gdmf_macos_window close];
            gdmf_macos_window = nil;
        }
        gdmf_macos_content_view       = nil;
        gdmf_macos_metal_layer = nil;
        gdmf_macos_window_delegate   = nil;
    }

    return;
}
void gdmf_macos_appkit_pump(void) {
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

void* gdmf_macos_appkit_metal_layer(void) {
    return (__bridge void*)gdmf_macos_metal_layer;
}

void* gdmf_macos_appkit_window_handle(void) {
    return (__bridge void*)gdmf_macos_window;
}

// Convert screen points to top-left backing-pixel coordinates.
bool gdmf_macos_appkit_mouse_client(int* mouse_x, int* mouse_y) {
    if (!gdmf_macos_window || !gdmf_macos_content_view) { return false; }

    @autoreleasepool {
        NSPoint screen_point = [NSEvent mouseLocation];
        NSRect  screen_rect  = NSMakeRect(screen_point.x, screen_point.y, 0, 0);
        NSPoint window_point = [gdmf_macos_window convertRectFromScreen:screen_rect].origin;
        NSPoint view_point   = [gdmf_macos_content_view convertPoint:window_point fromView:nil];
        view_point.y = gdmf_macos_content_view.bounds.size.height - view_point.y;

        NSPoint backing_pixels = [gdmf_macos_content_view convertPointToBacking:view_point];

        *mouse_x = (int)backing_pixels.x;
        *mouse_y = (int)backing_pixels.y;
    }

    return true;
}
