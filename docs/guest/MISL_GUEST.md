# Writing a MISL guest

`fuselage_misl.h` is the C entry header for programs built into ROAPs. It combines
the guest declarations and wrappers without including GDMF, CAKE, DICE, SHARP
or VIVIAN implementation headers. See [MISL ABI and MMIO](MISL_ABI_MMIO.md) for
the numeric contracts and [Conventions](../CONVENTIONS.md#terminology) for terminology.

## Building and startup

Compile this header for RV32 with a compatible RISC-V compiler. The wrappers
bind integer arguments to `a0` through `a6` and execute `ecall`; this is guest
code and is not a native host header. Both soft-float `rv32im/ilp32` and
hard-float `rv32imfd/ilp32d` can compile it. Actual compiler support routines,
startup, linking and ROM thunks depend on the chosen runtime and build rules.
The inline wrappers alone do not guarantee a library-free link.

Use `MISL_PROGRAM_BEGIN()` before other guest services. It declares the exact
ABI version and discovers regions, issuing an `ebreak` if either step fails.
The macro is not a C nonreturning construct; do not use it as a substitute for
correct host halt handling. `MISL_Halt`, `MISL_Yield` and `MISL_Sleep` are guest
controls. Yield and sleep release the current slice; sleep counts simulation
ticks rather than wall-clock milliseconds.

`MISL_PROJECT(...)` declares one `RoapManifest` in `.misl_manifest`, aligned to
four bytes. Supply it once at file scope with designated initializers:

```c
MISL_PROJECT(
    .title = "Example",
    .canvasWidth = 1280,
    .canvasHeight = 720,
    .simRateMilli = 60 * ROAP_MANIFEST_RATE_SCALE
);
```

The declaration retains its section, symbol and 128-byte layout. The linker and
packer must place it correctly; the macro does not arrange a program image on
its own. Omitted fields are zero and receive their field-specific host handling.
See [ROAP format](ROAP_FORMAT.md) for defaults, units and manifest readers.
Use the [guest build rules](BUILDING_A_ROAP.md) and `.text.init` starter to keep
the manifest's entry offset consistent with the linked image.

The facade is divided into `misl_guest_*.h` headers for types, startup, memory,
system, text, sprites, tiles, pixies, colors, interactions, input, audio and DICE.
The private call header supplies the shared register and representation helpers.
These are required source and SDK files. Applications can continue including
the umbrella rather than depending on the private helper names.

## Calls and bit representations

The wrappers preserve the register assignments in `misl_abi.h`. Floating values
cross as bits, colors as RGBA8 words, and 64-bit arguments/results as low/high
word pairs. Pair positions are specific to each call. Private `misl_guest_`
helpers and `MISL_GUEST_` macros perform this encoding; their names are not a
supported application interface.

There is no general error convention shared by all wrappers. Read the ecall
description and the corresponding subsystem documentation for results, units and
valid ranges. A wrapper does not make a host operation safe merely by casting
its arguments. In particular, pointer addresses and sizes still require host
validation. Most wrappers pass arguments through without local bounds checks.

## Types and subsystem wrappers

`misl_guest_types.h` declares guest mirrors such as `Color`, `PixieTransform`,
`SpritePairRef`, `TileCellRef` and the pixie enums. Their values, widths and field
order match the existing wire contract. `FUSELAGE_COLOR_TYPE` permits an asset
header to provide `Color` first; that alternative definition must have the same
RGBA8 representation.

Wrappers for engine systems use their approved names, including
`PackColorsRGBA8`, `SetColorsDeepPalette`, `GetInteractionsSpriteDistance`,
`DICE_RollDieRange` and `SHARP_GetRenderPosition`. Guest-only services use MISL
names. Source using earlier spellings must be updated and rebuilt; the function
renames do not change existing ecall numbers.

Some guest signatures intentionally differ from native signatures. Controller
names are copied into guest storage rather than returned as host pointers.
`DICE_InitTimer` takes an interval in microseconds and offers polling rather than
a host callback. `DICE_GetRealTime` writes a `MislClock` and returns success.
`CopySpriteCollisions` retains its guest buffer signature. A guest pixie RAM
pointer is volatile and refers to a mapped guest bank. Do not assume replacing
an include is sufficient to port every native call.

## Discovery and mapped data

The runtime must define exactly one `misl_guest_descriptor` and
`misl_guest_region_bases` cache for the complete guest, shared across translation
units. `MISL_DiscoverRegions` populates known IDs after checking descriptor magic.
It currently does not validate the full descriptor version, byte extent or
duplicate IDs. `MISL_GetRegionSize` searches the published count; use the cache
only with a descriptor supplied by a compatible host. Discovery failure does
not clear a previously populated cache.

`MISL_GetRegionBase` returns zero for an unavailable or out-of-cache ID. This is
an availability convention for mapped regions, not every possible memory region:
program memory legitimately begins at zero. `MISL_GetProgramSize` reports the
program allocation. Extra pool helpers report consecutive published pools,
their bases and sizes; an out-of-range pool index returns zero.

Asset helpers operate on the mapped ROAP payload. `MISL_FindAsset` returns false
for an unavailable payload, missing ID or detected malformed entry, and may have
written entry fields before returning false. Its checks are narrower than the
packer's: it checks the directory magic and bounds some reads, but does not
validate all trailer/version/overlap conditions. It clamps an oversized directory
count to the available entry space. It is not a complete file validator.

`MISL_GetAssetData` returns a mapped pointer and stored byte count, or null and
zero on failure. It does not decode a compressed section. Inspect the
`RoapAssetEntry` flags when interpreting its contents. All offsets are relative
to the payload start.

`MISL_ReadSram` copies from the save region. Null destination, zero length or a
range outside the published save size returns false. `MISL_WriteSram` is a runtime
ROM thunk, not an inline ecall in this header. The host owns save-file policy.

## System and presentation

System wrappers cover signals, pacing, cursor/mouse capture, presentation options
and capture requests. Canvas getters report reference-canvas dimensions.
`FuselageElapsedMicroseconds` reads a live monotonic clock; input-page elapsed
time is sampled once per pump. Frame capture is available only when configured
by the host. Its name argument is not a general filesystem path.

## Diagnostic text

The `tl` wrappers retain the diagnostic text layer's naming exception. Each call
uses the same property-level ecall contract, including separate cell character
and color getters. Format a string in guest code before printing it; there is no
variadic host printf wrapper here. See [Diagnostic text](../graphics/GDMF_TEXT.md).

## Sprites and tiles

Sprite and tile wrappers retain their existing coordinate and attribute
encodings. `GetSpriteBitmapSlot` computes a fixed atlas address without checking
the bitmap ID. `GetTileBitmapSlot` obtains the initialized tile dimensions by
ecall and computes the slot address, returning null for a zero-sized slot.
It does not validate every layer/bitmap argument. `GetTileCells` computes the
fixed cell-bank address; initialize the layer and use valid indices before
accessing it. These helpers are current fixed-address exceptions to discovery.

## Pixies and command lists

`PackPixieFloatArg` reinterprets a float for command arguments.
`PackPixieDrawXY` packs signed 13.3 coordinates; `PackPixieDrawLine` clips a segment
to the representable box before packing its endpoints, returning false when
rejected. Point packing clamps each coordinate before integer conversion;
infinities saturate and each NaN coordinate becomes zero. Guest rounding retains
the add-half-and-truncate calculation: under the normal nearest rounding mode,
halfway values round away from zero. Native packing instead uses the active
`lrintf` rounding mode, normally nearest-even. For example, positive/negative
0.0625 pack as positive/negative one eighth in the guest, but zero in native
nearest-even mode. Exact eighth-pixel inputs agree.

The line helper requires writable output for two words. It returns false for
null output, nonfinite endpoints, a nonfinite endpoint difference or a wholly
outside segment, leaving output unchanged. Clipping still uses single precision;
extreme finite coordinates can lose geometric detail. See
[coordinate packing](../graphics/GDMF_PIXIES_COMMANDS.md#coordinate-packing) for the encoding.

`GetPixieList` exposes the discovered scratch list. `PushPixieListCommand` writes
only when the list is present and the index is below capacity, but returns the
incremented index even if it did not write. That unsigned index can wrap.
`SubmitPixieList` returns the number of commands that succeeded; rejection does
not stop the whole batch. The status helpers report rejection and cumulative
counts, with the existing no-rejection sentinel and unsigned wraparound.

RAM-list execution requires an aligned RAM offset. Transform slot zero is the
identity; transform setup uses degrees, while pixie rotation currently uses
radians. Those units are preserved here. `GetPixieRAMReadPtr` returns null for
an unpublished region or out-of-range ID; a pointer for an uninitialized bank
may still fault when read.

## Collision queries

Interactions uses the same ownership naming as other GDMF subsystems.
Collision pair batches and sprite-to-tile batches remain separate operations.
Pair discovery can return more results than output capacity; `-1` is refusal,
while zero is a valid no-collision result. Batch buffers must satisfy the host's
memory, alignment and non-overlap rules. See [Interactions](../graphics/GDMF_INTERACTIONS.md)
and [Sprites](../graphics/GDMF_SPRITES.md) for the geometry and collision semantics.

## Input snapshots and calls

The `MISL_Is*`, `MISL_Was*` and input getters use the per-pump snapshot without
an ecall. Discover and check the input page before using them. Key and mouse
button helpers mask indices; controller helpers currently apply inconsistent
masking, so always supply valid controller indices. Snapshot levels and edges
have different meanings; see [MISL ABI and MMIO](MISL_ABI_MMIO.md#input-and-clocks).

The CAKE wrappers are the existing ecall alternatives and controller metadata
services. Reading the snapshot avoids a trap for repeated input queries; it
does not make the snapshot update during a guest slice.

## Audio events and capture

The SHARP wrappers configure chips, upload samples and issue individual register
writes. `SHARP_WriteRingEvent` batches a timestamped event in MMIO without a trap.
Its accessors require published ring/control pages, including before calling
`SHARP_IsRingReady`. Use valid chip/register values and schedule ahead of the
published render position. Writes can overwrite undrained events; diagnostics
are updated when the host drains the ring. Volatile accesses do not establish
a concurrent producer/consumer protocol.

`MISL_RequestAudioCapture` updates a persistent request slot;
`MISL_StopAudioCapture` disables it. Out-of-range slots or an unavailable capture
page are ignored. `MISL_GetAudioCaptureData` returns null with zero frames when
no samples are delivered. Otherwise it returns the host-published page-relative
offset and frame count, which it currently trusts without validating the complete
sample extent. Read the delivered count, not the requested count.

## Random values and timers

DICE wrappers preserve seed word order, probability float bits, timer intervals
and 64-bit sequence positions. Timers are polled; guests cannot register a native
function pointer as a host callback. `DICE_AllocTimer` returns a free handle or
`-1`. `DICE_GetTimerDelta` returns a double. The guest interval initializer keeps
the name `DICE_InitTimer`; its microsecond signature differs from the native
callback initializer.

## Published colors

`misl_colors.h` supplies 384 color macros without requiring GDMF headers. Its
published names and values match the existing guest table and current native
definitions. The header must follow a compatible `Color` declaration; the
umbrella supplies one. The publication is a generated snapshot, so changing the
canonical GDMF definitions requires regenerating and checking it before release.
Matching the native definitions does not certify their historical palette
accuracy. Color-name accuracy remains a separate review.
