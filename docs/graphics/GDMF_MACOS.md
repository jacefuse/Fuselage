# GDMF on macOS

## Implementation layout

The macOS window backend has three implementation files:

| File | Responsibility |
|---|---|
| `gdmf_window_macos.c` | Shared engine-state updates, focus policy, and the display-sleep assertion |
| `gdmf_window_macos_appkit.m` | AppKit objects, lifecycle, events, menus, icons, drawable sizing, and mouse coordinates |
| `gdmf_window_macos_presentation.m` | Display transitions and cursor presentation |

`gdmf_window_macos_internal.h` declares their shared interface. Its Objective-C
section exposes the window and view only to the implementation files. The
lifecycle file owns these objects; the presentation file uses them on the main
thread. The display-mode enum lives in `gdmf_display_mode.h`, shared with the
public GDMF header.

The macOS build must compile the C backend and both Objective-C files. The
Objective-C files use automatic reference counting (ARC).

## Rendering surface

The content view uses a `CAMetalLayer`. The current Vulkan surface obtains that
layer through `GDMF_GetMetalLayer()` and wraps it with `VK_EXT_metal_surface`.
The window backend does not create rendering pipelines or submit draw commands.

AppKit window dimensions are expressed in points. The backend converts drawable
dimensions and mouse positions to backing pixels. Canvas mapping then uses the
renderer viewport and the configured logical canvas dimensions.

## Responder handling

The content view accepts first-responder status and consumes key-down events;
the engine's input system handles application input separately. This avoids
AppKit treating those key presses as unhandled commands and sounding an alert.

The custom window remains eligible to become key and main when borderless.
Display transitions explicitly restore the content view as first responder.
The close button and Quit menu request engine shutdown so the engine can release
its resources before the window is destroyed.

## Presentation and queued work

Both fullscreen requests currently use a borderless window covering a screen.
They do not perform exclusive display capture. Before leaving windowed mode,
the backend saves the window geometry, style, and application presentation
options. Returning to windowed mode restores those saved options. Teardown
also restores saved presentation options and clears local display bookkeeping.

Cursor capture disassociates mouse movement from cursor movement, pinning the
cursor rather than clipping it to a client rectangle. The C backend resolves
focus policy before queuing a cursor update. Teardown balances the backend's
cursor hide and restores cursor association.

Display and input setters enqueue their AppKit work on the main dispatch queue.
The current interface does not acknowledge completion or cancel outstanding
requests during teardown. Callers must not treat setter return as proof that
the requested state is applied. Queue ordering and window lifetime still need
hardening before a general cross-thread safety guarantee can be made.
