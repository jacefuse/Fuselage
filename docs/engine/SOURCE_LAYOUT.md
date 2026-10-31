# Source layout

## Repository directories

| Directory | Contents |
|---|---|
| `fuselage/source` | Production implementations, public and private headers, build manifests and required data. |
| `sdk` | Guest build rules and the minimal guest starter. |
| `docs` | Public API and build guides. |
| `fuselage/build` | Standard deliverables and ordinary generated build output. |
| `PRIVATE` | Internal notes, tests, test builds, migration evidence and historical copies. |

The public source package includes the production inputs and public documentation.
Internal material mirrors its former paths below `PRIVATE`: for example,
`PRIVATE/fuselage/tests` and `PRIVATE/fuselage/tracking`. It is excluded from the
public package. Application and asset selection is separate from engine source.

## Engine boundaries

GDMF, CAKE, DICE, SHARP, SRAM and VIVIAN have their own source directories and
standalone builds. Fuselage root files coordinate their lifecycles and application
callbacks. MISL contains the guest interface and native host adapters; ROAP owns
format handling and packing tools. Integration belongs at those boundaries,
rather than making independent systems depend on one another.

Subsystem roots retain their entry headers and Makefiles. Implementations are
grouped below them: CAKE has core/helpers/platform directories; DICE has clocks,
timers, RNG and helpers; SHARP has chips, mixing, transport, backends and platform
code; SRAM separates core/storage; VIVIAN separates execution, memory, floating
point and compressed decoding. MISL separates ABI, guest API/runtime, math,
formatting, compiler support, ROM and host integration. Fuselage coordination
files live under `BASE`, with core, application and pacing subdirectories.
`fuselage.h` remains at the source root; its supporting root headers sit in `BASE`
beside those three directories.

Public guides are grouped by system under `docs`. Start at the
[documentation guide](../README.md).

GDMF separates `textlayer`, `sprites`, `tiles`, `pixies`, `interactions`, `compute`,
`vulkan` and `shaders`. Shared canvas, palette, window and worker interfaces remain
at the GDMF root. Relative-operation helpers sit with the layer they extend.
The compute contract is separate from its Vulkan implementation.

Private headers and implementation fragments are build inputs even when they
are not application interfaces. A split into smaller files does not make those
files optional. Source manifests list the implementations for each platform;
update the appropriate manifest when adding or moving an implementation. GDMF
has Make and optional CMake manifests. See [building GDMF](../graphics/GDMF_BUILD.md).

## Generated data

Checked-in generated data lets an ordinary host build avoid a shader compiler,
RISC-V compiler or asset editor. Keep the corresponding source and regeneration
tools with it where available.

GDMF's `textlayer/packedbitmaps.h` contains the Diagnostic Text glyph data exported
by Edibima. It is required data, not a disposable build cache. Regenerate it from
the font authoring source when changing the typeface.

The shader directory contains GLSL sources, compiled SPIR-V and C headers that
embed the SPIR-V. Render code includes the headers. Ordinary builds use these
checked-in headers; they do not automatically compile edited GLSL. A shader change
must regenerate and verify the matching compiled data and embedded header.

BOOTSPUR likewise includes a checked-in system-ROM image so a host build does not
require the guest cross toolchain. Its version and contents are part of the ROM
contract. See [system-ROM generation](../guest/MISL_ROM.md).

Platform bindings generated from installed Wayland protocol files belong under
the build directory. Application-specific embedded ROAP headers also belong in
build output; they are regenerated from the selected application's ROAP.
