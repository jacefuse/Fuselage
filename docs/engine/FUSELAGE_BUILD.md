# Building native Fuselage programs

## Building

Run GNU Make from the repository root or from `fuselage`.
The common goals are:

```sh
make release           # Native version-banner demo and release library
make release launcher  # Release MISLlauncher
make debug             # Debug demo and library
make debug launcher    # Debug MISLlauncher, with a console
```

Goal order does not matter. A plain `make` selects the release demo and library.
Selecting both `release` and `debug` in one invocation is an error. `CONFIG=...`
and `MODE=...` remain available for explicit builds; native and embedded mode
goals can also accompany the variant. The root Makefile forwards to `fuselage`.

On Windows the deliverables are `fuselage/build/fuselage.exe`,
`fuselage/build/MISLlauncher.exe` and `fuselage/build/libfuselage-release.a`.
Run `MISLlauncher.exe path/to/game.roap` to load a ROAP. The default native
application shows the version banner and starter character; Escape exits.
`MODE=launcher` builds only the launcher, and `make -C fuselage lib` builds
only the combined library. Non-Windows executable names omit `.exe`.

The combined archive contains engine/host implementations and independent system
objects, not nested archives. It excludes `main()` and the starter `tick()`.
Consumers supplying their own entry point link it with the platform libraries
listed below. This archive is not yet a packaged native SDK.

A native C project supplies `project_config.h` and `project.c`, containing `tick()`.
Paths below are relative to the `fuselage` working directory. Give custom projects
their own `OUTPUT` so they do not replace the standard banner application:

```sh
make -C fuselage -j MODE=native PROJECT_DIR=../my_project OUTPUT=build/my_project.exe
```

To embed an already built ROAP in a native executable:

```sh
make -C fuselage -j MODE=embedded PROJECT_DIR=../my_host ROAP_FILE=../my_game/game.roap OUTPUT=build/my_game.exe
```

The embedded host directory needs `project_config.h`; it needs no native callback.
Build the guest ROAP separately with `sdk/roap.mk`. Guest-only development does not
require building the launcher or checking out the full engine.

Use `CONFIG=debug` for `-O0 -g -DDEBUG`; release uses `-O2 -DNDEBUG` and strips
Windows executable symbols. Debug deliverables use
`fuselage-debug`, `MISLlauncher-debug` and `libfuselage-debug.a`. Intermediate
objects, subsystem archives and generated data live under
`fuselage/build/obj/<platform>/<mode>/<configuration>`. Each mode/configuration
has separate intermediates. Internal review evidence lives under
`PRIVATE/fuselage/build`; host utilities built for use live under `fuselage/build/tools`.

For multiple native C files, set `PROJECT_SOURCES` to a quoted, space-separated
list of paths. Basenames must be unique. `CC`, `AR`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`,
`LDLIBS`, `BUILD_DIR` and `OUTPUT` can be overridden. Source/project/build paths
must be expressible without whitespace; use relative paths when the checkout's
parent directory contains spaces. `BUILD_DIR` is relative to `fuselage`. Use a new
build directory after changing compiler, target or custom compiler flags: command
line changes are not a complete dependency fingerprint.

GNU Make 4.x and a C11 compiler with GNU-style command-line options are required.
The default compiler is Clang. Windows was verified with 64-bit Clang/MinGW tools;
this Makefile does not select the MSVC compiler interface. No Python, CMake,
RISC-V compiler or shader compiler is required to build the ordinary launcher.
Embedded mode also builds the small native `bin2h` tool from its reviewed source.

## Dependencies

All platforms need Vulkan development headers and a suitable loader/import
library, plus a Vulkan runtime to run graphics. If `VULKAN_SDK` is set, its standard
platform include/library directories are used. Otherwise the Makefiles ask
`pkg-config` for Vulkan flags. Override `VULKAN_CFLAGS` and `VULKAN_LIBS` for a
nonstandard installation; `VULKAN_INCLUDE_DIR` and `VULKAN_LIBRARY_DIR` adjust SDK
subdirectories without embedding a machine-specific path in the build files.

Windows links the platform libraries used by input, windowing, randomness and
WASAPI. No winpthreads dependency is requested. Native/embedded release builds
and launcher release builds default to the Windows GUI subsystem; debug builds use a console. Override with
`WINDOWS_SUBSYSTEM=windows` or `WINDOWS_SUBSYSTEM=console` as needed.

The Windows launcher embeds Miss Pixie's executable icon from
`assets/RocketGirlRemake.ico`. Building it requires a MinGW-compatible resource
compiler (`windres` by default; override `WINDRES`, for example with
`WINDRES=llvm-windres`). The icon and resource script are included in the source
package. This adds no runtime DLL dependency. The Windows window uses the
executable's primary icon by default; a configured runtime RGBA icon overrides
it. macOS and Linux builds do not compile this Windows resource.

macOS needs its development tools and Cocoa, QuartzCore, IOKit, CoreFoundation,
GameController, AudioToolbox and CoreAudio frameworks. The current graphics path
still uses Vulkan; the planned Metal migration is separate work. This build
produces an executable, not a signed application bundle or Vulkan deployment.

Linux needs `pkg-config`, Vulkan, ALSA, Wayland client/cursor, X11, Xi, XRandR and
XScreenSaver development packages. It also needs `wayland-protocols` and
`wayland-scanner`. Protocol bindings are generated into the build directory.
`PKG_CONFIG`, `WAYLAND_PROTOCOLS_DIR` and `WAYLAND_SCANNER` are overridable.

Individual systems retain their standalone Makefiles. GDMF now has one alongside
its optional CMake build. The root invokes the system builds and links their
archives; no independent system imports the root or MISL to do so.

## Executable modes

Native mode applies project settings, resolves the executable path, configures
native SRAM and registers the project's `tick`. Save-load refusal registers the
existing failure view instead, so project code does not run.

Launcher mode reads command-line options and ROAP metadata before graphics startup.
Help, version and info requests exit without starting the root. A run registers
`FuselageTickLauncher`; launcher-owned bytes are released after root shutdown has
detached guest memory. Command-line display mode overrides manifest settings.

Embedded mode applies host project settings and checks the generated image before
starting graphics. It registers the engine's embedded callback through a small
verdict adapter. The guest loader alone opens the save declared in the ROAP
manifest; host `PROJECT_SRAM_*` values are not applied. As in ENTERON, embedded
window/title/pacing settings come from the host project configuration, while the
guest manifest supplies its memory/save declaration.

Root failure codes take precedence over an application verdict: 1 reports root
startup/runtime failure and 5 reports save-publication failure. Otherwise native
save-load refusal returns 4, and launcher/embedded guest faults return 3 or save
refusals 4. Normal completion and informational requests return 0. Embedded mode
latches a halt verdict before root teardown clears the guest session.

## Project configuration

An empty `project_config.h` selects defaults. Define only the settings needed:

| Setting | Default and meaning |
| --- | --- |
| `PROJECT_TITLE` | `"Fuselage"` |
| `PROJECT_CANVAS_WIDTH`, `PROJECT_CANVAS_HEIGHT` | Both 0: engine canvas default |
| `PROJECT_WINDOW_WIDTH`, `PROJECT_WINDOW_HEIGHT` | Both 0: engine window default |
| `PROJECT_ASPECT_NUM`, `PROJECT_ASPECT_DEN` | Both 0: engine aspect default |
| `PROJECT_SIM_RATE` | 0: engine simulation-rate default |
| `PROJECT_TARGET_FPS` | 0: no render cap |
| `PROJECT_CURSOR_VISIBLE` | 1: show the OS cursor |
| `PROJECT_BANNER` | 1: write component identities into the diagnostic text layer |
| `PROJECT_AUDIO_POOL_MB` | 0: SHARP pool default; nonzero values are MiB |
| `PROJECT_ICON_RGBA`, `PROJECT_ICON_WIDTH`, `PROJECT_ICON_HEIGHT` | NULL/0/0: no project icon |
| `PROJECT_SRAM_ID`, `PROJECT_SRAM_VERSION`, `PROJECT_SRAM_BYTES` | Empty/0/0: no native save region |

Dimension and aspect settings are applied as complete positive pairs. An icon
names a top-down RGBA8 array included by the project configuration header. Its
storage must cover width × height × 4 bytes. Wayland has no portable window-icon
operation. Title/icon lifetime and runtime restrictions are in FUSELAGE.md;
save identifiers/layouts are described in SRAM.md.

These macros are compile-time configuration. They cannot choose settings from
runtime display discovery, and unknown macro names are not automatically diagnosed.
The engine gathers settings in an internal typed record; applications should use
the macros and public controls, rather than access that record.

## Source inventories

`GDMF/sources.mk` and `MISL/host/sources.mk` explicitly enumerate shipping code.
The optional GDMF CMake inventory covers the same files; keep both lists in step
when moving or adding a source. Normal targets do not discover arbitrary `.c`
files or compile internal test harnesses. Checked-in shader/ROM headers are used
without regenerating them. Tests, tracking notes and build outputs are not inputs
to these shipping source lists.
