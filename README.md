# Fuselage

*Free Unrestricted Software Enabling Layered Asset Game Environments*

Fuselage is a retro-inspired 2D engine with sprites, tile maps, indexed palettes,
programmable framebuffers and virtual sound chips. Applications can run as native
C programs or as RISC-V guest programs stored in ROAP files. A ROAP can be loaded
by the launcher or embedded in a native executable.

## Engine systems

| System | Purpose |
| --- | --- |
| GDMF | Windows, graphics layers, palettes and collision queries |
| CAKE | Keyboard, mouse and controller input |
| DICE | Timers, elapsed time and random-number generation |
| SHARP | Virtual sound chips, mixing and audio output |
| SRAM | Application save storage |
| VIVIAN | Independent guest instruction interpreter |
| MISL | Guest interfaces, memory-mapped services and native host integration |
| ROAP | Program-file format and packing tools |

Fuselage coordinates the systems. They also have standalone interfaces and build
rules. The renderer currently uses Vulkan on Windows, macOS and Linux; Linux
includes Wayland and X11 backends. Native Metal rendering is planned separately.

## Getting started

To build the native launcher with GNU Make and a C11 compiler:

```sh
make -C fuselage -j MODE=launcher
```

The Windows launcher is `fuselage/build/MISLlauncher.exe`. From the repository
root, `make release launcher` builds it. `make release` builds the standard native
`fuselage.exe` banner application and `libfuselage-release.a` in that directory.
Use `debug` instead of `release` for debug variants.

Vulkan development files and platform libraries are required. See the
[build guide](docs/engine/FUSELAGE_BUILD.md) for dependencies, native projects and embedded
ROAP builds. The ordinary host build does not require Python, CMake or a RISC-V
compiler.

To write a guest application, start with [Building a ROAP](docs/guest/BUILDING_A_ROAP.md).
Guest builds use the SDK headers/runtime, a compatible RISC-V toolchain and the
native packer; they do not build or link the engine.

The [documentation guide](docs/README.md) links the system interfaces, formats,
threading rules and build instructions. Public headers live under
`fuselage/source`; guest SDK build rules live under `sdk`.

## Current release

This source tree prepares ENTERON, version 0.5, for publication. ENTERON is in
feature lock. Public interfaces and documentation are being checked together;
applications using older function names need corresponding source updates.

Complete Windows host builds have been checked in native, launcher and embedded
modes. macOS and Linux implementations and build rules are included, but complete
native builds and device/application tests still need validation on those systems.
A successful build or controlled test does not establish hardware compatibility.

## License

Fuselage uses the [Zero-Clause BSD license](LICENSE).
