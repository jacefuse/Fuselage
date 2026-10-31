# Building GDMF

GDMF can be built and linked without the Fuselage application, CAKE, DICE,
SHARP, VIVIAN or MISL integration. Its standalone Makefile and optional CMake
target own the graphics source inventory. The Fuselage Makefile builds and links
the standalone archive when it coordinates the engine systems.

## Requirements

Use GNU Make, a C11 compiler and Vulkan development headers/loader. The optional
CMake build requires CMake 3.24 or newer.
Normal builds use the checked-in shader headers and do not run a shader compiler.
The Makefile uses VULKAN_SDK or pkg-config, with overridable Vulkan flags. See
[Fuselage build dependencies](../engine/FUSELAGE_BUILD.md#dependencies). CMake discovers Vulkan
through its own search paths; use CMAKE_PREFIX_PATH or its Vulkan package variables
for another installation.
No compiler path, SDK version or package-manager prefix is fixed by the target.

Windows uses the native window and synchronization APIs. With MinGW Clang,
GDMF selects native thread-local storage to avoid emulated-TLS dependencies.
The current build has been checked with Windows Clang; this does not establish
MSVC compatibility or runtime behavior on every Windows installation.

macOS also needs Objective-C and Cocoa, QuartzCore, IOKit and CoreFoundation.
The current renderer uses Vulkan/MoltenVK through CAMetalLayer. Native Metal
rendering remains future work.

Linux needs pkg-config, wayland-client, wayland-cursor, wayland-protocols,
wayland-scanner, X11, Xrandr and XScreenSaver development packages. Both Wayland
and X11 backends are included. Protocol bindings are generated from the installed
XML files into the build directory. macOS and Linux build execution still need
validation on their respective platforms.

## Standalone library

From the repository root:

```sh
make -C fuselage/source/GDMF
```

The archive is written beneath fuselage/build/GDMF by default. No CMake or test
source is required. Alternatively:

```sh
cmake -S fuselage/source/GDMF -B fuselage/build/gdmf -DCMAKE_BUILD_TYPE=Release
cmake --build fuselage/build/gdmf --config Release
```

Choose a generator and compiler using ordinary CMake options or a toolchain file.
Keep generated files outside the source directory. Vulkan remains a runtime
dependency; building the static library does not embed a graphics driver.

For a CMake application in this repository:

```cmake
add_subdirectory(fuselage/source/GDMF)
target_link_libraries(my_application PRIVATE GDMF::GDMF)
```

The application includes gdmf.h, or a specific interface such as
sprites/gdmf_sprites.h or compute/gdmf_compute.h. The public include path is the
GDMF directory. Applications do not need backend include directories or Vulkan
headers to use these public interfaces. Do not compile another copy of the GDMF
sources into the application.

## Shared utilities

The source tree supplies BASE/fuselage_sync.h for small native mutex/event wrappers
and BASE/fuselage_native.h for the Linux native-window descriptor. These are shared
platform contracts, independent of the orchestrator and other system state.
Keep them with the source distribution. GDMF owns the window/descriptor lifetime;
consumers borrow it. SEE: (FUSELAGE_SYNC.md, Synchronization contract) and
(GDMF_LINUX.md, Native window descriptor).
