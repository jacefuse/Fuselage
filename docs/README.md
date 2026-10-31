# Documentation guide

For native applications, begin with the build guide and native orchestration.
For guest applications, begin with Building a ROAP and the guest API. Public
headers are the declarations; these guides explain behavior, return values,
lifetimes and threading restrictions.

## Start here

- [Application targets and project boundaries](engine/APPLICATIONS.md)
- [Source layout and generated data](engine/SOURCE_LAYOUT.md)
- [Building native Fuselage programs](engine/FUSELAGE_BUILD.md)
- [Fuselage native orchestration](engine/FUSELAGE.md)
- [Fuselage conventions](CONVENTIONS.md)
- [Building a ROAP](guest/BUILDING_A_ROAP.md)

## Graphics and collisions

- [Choosing a collision query](graphics/COLLISION.md)
- [Colors and palettes](graphics/GDMF_COLORS.md)
- [Building GDMF](graphics/GDMF_BUILD.md)
- [Sprites](graphics/GDMF_SPRITES.md)
- [Tiles](graphics/GDMF_TILES.md)
- [Pixies](graphics/GDMF_PIXIES.md)
- [Interactions](graphics/GDMF_INTERACTIONS.md)
- [Diagnostic Text layer](graphics/GDMF_TEXT.md)
- [Pixie commands](graphics/GDMF_PIXIES_COMMANDS.md)
- [Pixie image formats](graphics/GDMF_PIXIES_FORMATS.md)
- [GDMF compute](graphics/GDMF_COMPUTE.md)
- [GDMF Vulkan implementation](graphics/GDMF_VULKAN.md)
- [GDMF workers](graphics/GDMF_WORKERS.md)
- [GDMF on macOS](graphics/GDMF_MACOS.md)
- [GDMF on Linux](graphics/GDMF_LINUX.md)

## Input, timing, audio and saves

- [CAKE](input/CAKE.md)
- [DICE](timing/DICE.md)
- [SHARP](audio/SHARP.md)
- [SHARP on Windows](audio/SHARP_WINDOWS.md)
- [SHARP on macOS](audio/SHARP_MACOS.md)
- [SHARP on Linux](audio/SHARP_LINUX.md)
- [SRAM save storage](storage/SRAM.md)
- [Display pacing](engine/FUSELAGE_PACING.md)
- [Internal synchronization](engine/FUSELAGE_SYNC.md)

## Guest programs and tools

- [VIVIAN](guest/VIVIAN.md)
- [Writing a MISL guest](guest/MISL_GUEST.md)
- [MISL calls and shared memory](guest/MISL_ABI_MMIO.md)
- [Guest runtime](guest/MISL_RUNTIME.md)
- [Bounded MISL formatting](guest/MISL_FORMAT.md)
- [Direct MISL math](guest/MISL_MATH.md)
- [MISL compiler-support implementation](guest/MISL_HELPERS.md)
- [BOOTSPUR guest services](guest/BOOTSPUR.md)
- [BOOTSPUR system-ROM image](guest/MISL_ROM.md)
- [ROAP format contracts](guest/ROAP_FORMAT.md)
- [roappack](guest/ROAPPACK.md)
- [bin2h](guest/BIN2H.md)

## Native guest host

- [Native MISL host boundary](guest/MISL_HOST.md)
- [MISL diagnostic text and graphics](guest/MISL_HOST_GRAPHICS.md)
- [MISL input, audio, timers and ROM services](guest/MISL_HOST_SERVICES.md)
- [MISL host execution and device pumps](guest/MISL_HOST_RUNTIME.md)
- [Native ROAP launch modes](guest/MISL_LAUNCHER.md)
