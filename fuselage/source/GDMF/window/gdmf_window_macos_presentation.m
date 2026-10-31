// macOS display transitions and cursor presentation. AppKit work runs on the
// main thread; public bridge setters queue their changes there.
#include "gdmf_window_macos_internal.h"
#include <stdio.h>

static bool gdmf_macos_cursor_hidden = false;
static GDMF_DisplayMode gdmf_macos_display_mode = GDMF_MODE_WINDOWED;
static NSRect gdmf_macos_saved_window_frame;
static NSUInteger gdmf_macos_saved_window_style = 0;
static NSApplicationPresentationOptions gdmf_macos_saved_presentation_options;
static bool gdmf_macos_has_saved_presentation_options = false;

// Restore only presentation options saved when leaving windowed mode.
void gdmf_macos_restore_presentation(void) {
    if (gdmf_macos_cursor_hidden) {
        [NSCursor unhide];
        gdmf_macos_cursor_hidden = false;
    }
    CGAssociateMouseAndMouseCursorPosition(true);
    if (gdmf_macos_has_saved_presentation_options) {
        [NSApp setPresentationOptions:gdmf_macos_saved_presentation_options];
        gdmf_macos_has_saved_presentation_options = false;
    }
    gdmf_macos_display_mode = GDMF_MODE_WINDOWED;
    gdmf_macos_saved_window_frame = NSZeroRect;
    gdmf_macos_saved_window_style = 0;
    gdmf_macos_notify_display_mode(GDMF_MODE_WINDOWED);
}

// Both fullscreen requests use borderless coverage in this implementation.
// SEE: (GDMF_MACOS.md, Presentation and queued work).
void gdmf_macos_appkit_set_display_mode(GDMF_DisplayMode mode) {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            if (!gdmf_macos_window || mode == gdmf_macos_display_mode) { return; }

            if (gdmf_macos_display_mode == GDMF_MODE_WINDOWED) {
                gdmf_macos_saved_window_frame = [gdmf_macos_window frame];
                gdmf_macos_saved_window_style = [gdmf_macos_window styleMask];
                gdmf_macos_saved_presentation_options = [NSApp presentationOptions];
                gdmf_macos_has_saved_presentation_options = true;
            }

            switch (mode) {

            case GDMF_MODE_WINDOWED: {
                [gdmf_macos_window setStyleMask:gdmf_macos_saved_window_style];
                [gdmf_macos_window setFrame:gdmf_macos_saved_window_frame display:YES];
                [gdmf_macos_window setLevel:NSNormalWindowLevel];
                if (gdmf_macos_has_saved_presentation_options) {
                    [NSApp setPresentationOptions:gdmf_macos_saved_presentation_options];
                    gdmf_macos_has_saved_presentation_options = false;
                }
                [gdmf_macos_window makeKeyAndOrderFront:nil];
                [gdmf_macos_window makeFirstResponder:gdmf_macos_content_view];
                printf("[GDMF] Display mode: WINDOWED\n");
                break;
            }
            case GDMF_MODE_BORDERLESS:
            case GDMF_MODE_FULLSCREEN_EXCLUSIVE: {
                NSScreen* screen = [gdmf_macos_window screen] ?: [NSScreen mainScreen];
                [gdmf_macos_window setStyleMask:NSWindowStyleMaskBorderless];
                [gdmf_macos_window setFrame:[screen frame] display:YES];
                [gdmf_macos_window setLevel:NSMainMenuWindowLevel + 1];
                [NSApp setPresentationOptions:NSApplicationPresentationHideDock |
                                              NSApplicationPresentationHideMenuBar];
                [gdmf_macos_window makeKeyAndOrderFront:nil];
                [gdmf_macos_window makeFirstResponder:gdmf_macos_content_view];
                if (@available(macOS 14.0, *)) {
                    [NSApp activate];
                } else {
                    [NSApp activateIgnoringOtherApps:YES];
                }
                printf(mode == GDMF_MODE_BORDERLESS
                           ? "[GDMF] Display mode: BORDERLESS\n"
                           : "[GDMF] Display mode: FULLSCREEN EXCLUSIVE (borderless on macOS)\n");
                break;
            }

            default:
                return;
            }

            gdmf_macos_display_mode = mode;
            gdmf_macos_notify_display_mode(mode);
            gdmf_macos_sync_drawable_size();
        }
    });

    return;
}

// Capture pins the cursor; focus policy is resolved by the C backend.
// SEE: (GDMF_MACOS.md, Presentation and queued work).
void gdmf_macos_appkit_apply_input_state(bool capture_requested, bool cursor_visible) {
    dispatch_async(dispatch_get_main_queue(), ^{
        CGAssociateMouseAndMouseCursorPosition(capture_requested ? false : true);

        if (!cursor_visible && !gdmf_macos_cursor_hidden) {
            [NSCursor hide];
            gdmf_macos_cursor_hidden = true;
        } else if (cursor_visible && gdmf_macos_cursor_hidden) {
            [NSCursor unhide];
            gdmf_macos_cursor_hidden = false;
        }
    });

    return;
}
