# MISL calls and shared memory

MISL is the Machine Independent System Layer used by programs in ROAP files.
VIVIAN executes the guest instructions; Fuselage connects guest requests to the
engine systems. The contracts here need only fixed-width C integer types, not
the engine implementations. See [Conventions](../CONVENTIONS.md#terminology) for
the names used for the format, execution core and guest interface.

## Header organization

| Header | Contents |
|---|---|
| `misl_abi.h` | Ecall numbers, register arguments and results, ABI version |
| `misl_mmio.h` | Shared-memory umbrella header |
| `misl_mmio_map.h` | Allocation constants, region IDs, descriptor and address checks |
| `misl_mmio_audio.h` | Audio event, ring control and capture records |
| `misl_mmio_graphics.h` | Pixie commands, submission results and tile cells |
| `misl_mmio_input.h` | Input snapshot, key/button values and calendar record |
| `misl_rom.h` | BOOTSPUR system-ROM header and entry indices |
| `roap_asset_rom.h` | ROAP asset payload records, owned by the file format |

The MMIO umbrella includes the canonical asset header from the adjacent ROAP
directory. These headers and that relative directory arrangement can be copied
without the host session or engine sources. This describes their dependencies;
it does not provide a complete guest SDK, startup runtime or toolchain.

## Calls and compatibility

A guest puts the call number in `a7`, arguments in `a0` through `a6`, and executes
`ecall`. Results use `a0`, or `a0:a1` for a 64-bit value, low word first. The
per-call comments specify which registers carry each argument. A pair need not
start at an even register: tile offsets and some timer calls use odd pairs.

Floats cross as their 32-bit IEEE-754 representation in an integer register.
Doubles use two integer registers. Colors occupy one RGBA8 word, with red in
the low byte and alpha in the high byte. Pointer arguments are 32-bit guest
addresses; host validation and read/write permissions are service-specific.
A C return value of `bool` uses zero for false and nonzero for true.

`MISL_SYS_ABI_DECLARE` must precede every other ecall. The current host accepts
exactly `MISL_ABI_VERSION`, currently `2026080501`. A mismatch or undeclared call
halts the guest. This number is a compatibility contract, not a modification
date. `MISL_VERSION` identifies the layer's source revision and can change
without changing the ABI.

Changing an existing argument's encoding, units, result convention or shared
record layout requires compatibility review. Adding a call does not change
older calls, but a guest still needs a host that implements the new call.
Numeric equality alone does not negotiate optional capabilities. Retired call
numbers and region IDs must never be reused. Renaming a C identifier without
changing its numeric value or representation does not change compiled guest
instructions.

The ecall header retains the register-level descriptions, including special
results such as `MISL_SPR_FIND_PAIRS`: `-1` means refusal, zero means no pairs,
and the total found may exceed output capacity. A zero return is not a universal
error code across MISL. BOOTSPUR calls use `0x5000 + entry index`; guests normally
reach them through the ROM table. SRAM writes return one on success, zero for a
zero-byte request, and halt on an invalid nonempty range.

Engine lifecycle and native callback registration remain host operations.
Guest timers expose polling services. Guest code formats its own variadic text
before submitting a string. Frame capture requires host configuration and a
name accepted by the host; it is not a general guest file-writing service.

## Memory discovery

Call `MISL_SYS_DESCRIPTOR` after ABI declaration to obtain a guest pointer to
`MislDescriptor`, or zero if unavailable. Check its magic, ABI version, byte
extent and region count before walking entries. The record is 528 bytes with
room for 32 regions. Each 16-byte entry gives an ID, guest base address, byte
size and read/write/execute flags. An unpublished ID denotes an unavailable
region, not a promise of readable zero-filled memory.

Declared RAM pools grow upward from address zero. Asset data and SRAM are
placed downward below `MISL_SHARED_TOP` (`0xE0000000`). The default program pool
is 16 MiB; this is not every program's memory limit. Placed bases use 64 KiB
alignment. Discover each pool's published base and size rather than inferring
them from the defaults. Pool IDs 17 through 24 identify up to eight extra pools.

The upper engine zone contains separate banks, the writable window and the
read-only system ROM. The window starts at `0xF8000000`; BOOTSPUR starts at
`0xF9F00000`, outside that window. Address space reservation and allocated host
storage are different quantities. These constants do not guarantee lazy
commitment, bounded resident memory or safety against arbitrary guest accesses.

Discovery does not currently remove every fixed-address dependency. In
particular, guest sprite and tile atlas helpers compile window addresses into
their instructions. Moving these addresses requires review of those consumers.
`MISL_ASSET_ROM_MAX` retains the older bound based on the default program size;
it is not a universal ROAP file-size limit. Actual placement depends on the
program's declared RAM, assets and SRAM.

## Audio

Both audio ecalls and the MMIO event ring are implemented. Ecalls suit individual
operations; the ring batches timestamped register writes. `MislSharpEvent` is
16 bytes: a 64-bit sample position, chip selector, explicit padding, 16-bit
register and 16-bit value, followed by alignment padding.

The host publishes audio control state before a guest slice and drains events
afterwards on the same simulation thread. Guest code writes event slots and
then advances `writeIndex`. Indices advance modulo 2^32; the slot is the index
modulo capacity. The host computes the unsigned difference from `readIndex`.
An overrun retains the newest capacity-sized batch and increments `dropped`.
This protocol requires the publication/drain order; it is not a concurrent
host/guest queue or an atomic snapshot protocol.

Event timestamps use SHARP render frames, not DICE microseconds. Schedule ahead
of the published position to allow for delivery between slices. `late` reports
events behind the render clock; `disordered` reports timestamps behind an
earlier event. These are separate from an MMIO ring overrun.

Audio capture provides eight request slots. Guests request a bus or master mix,
signed 16-bit or float samples, one or two channels, a frame count and an enable
flag. The host supplies the offset and actual delivered count. Offsets are
relative to the capture page, not the control record or the sample area. Read
only the delivered frames. The first 4 KiB contains control data; samples follow.
The sample upload pool is accessed by ecall rather than a published pool mapping.

## Graphics and submission results

The pixie scratch list contains up to 4,096 records of 20 bytes each. Filling it
does not execute commands: `MISL_PIX_SUBMIT` supplies the count and executes the
list at that point in the guest's slice. Persistent command data belongs in
pixie RAM. Its sixteen 4 MiB banks are readable through a separate mapping;
changes still go through the write service.

The status page describes the last submission and cumulative unsigned 32-bit
counters. `firstRejectIndex` is `MISL_PIXIE_NO_REJECT` when no record was refused;
zero is a valid rejected index. The echoed opcode and pixie identify the first
rejected record. Cumulative counts can wrap modulo 2^32.

Sprites use 1,024 atlas slots of 2,048 bytes, each holding a packed 64 by 64
4-bit bitmap. Tile bitmap storage has sixteen layer spans of 512 KiB, with slot
size determined by the initialized layer's tile dimensions. Initialize a layer
before filling its bitmap slots.

Writable tile cell banks mirror GDMF's `TileLocation`, including the tag field.
Each bank uses a fixed `[x][y]` array of 512 by 512 twelve-byte cells, even when
the initialized map is smaller. The address stride is 4 MiB, while cell storage
uses 3 MiB; the remaining stride is not additional cell storage. Access only
initialized, published banks and valid cells. Host integration must check sizes,
offsets, flag values and capacities against the engine-owned definitions.

## Input and clocks

`MislInput` is a per-pump snapshot. `mouseX` and `mouseY` are relative motion;
`canvasMouseX` and `canvasMouseY` are absolute integer reference-canvas coordinates.
They describe different quantities. Key and mouse edge arrays capture observed
transitions between pumps. Controller button edges derive from consecutive
polled states, so a press entirely between polls can be missed. Key codes are
USB HID keyboard usages. Trigger bytes range from 0 to 255; signed stick axes
range from -32768 to 32767.

`elapsedMicroseconds` is the monotonic DICE clock sampled at the pump. It does
not advance during guest computation; `MISL_SYS_ELAPSED_US` reads the live clock.
`MislClock` is a separate 56-byte wall-clock record. UTC seconds identify an
instant; calendar fields are local time, with a signed UTC offset in minutes.
The wall clock may change when the system clock is corrected. The clock page
is a pump snapshot; `MISL_DICE_REAL_TIME` requests a live calendar reading.

Records use fixed-width fields and the current little-endian host/guest layout.
That is a layout requirement, not support for arbitrary host endianness or
concurrent readers and writers. Preserve published field names, order and widths
when adding convenience wrappers or reorganizing implementation files.
