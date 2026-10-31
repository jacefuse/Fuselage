# Native MISL host boundary

The native host layer connects a loaded ROAP to Fuselage services. VIVIAN remains
the independent instruction core; MISL guest headers remain usable without this
host layer. ROAP describes the file format, not the name of the execution system.
See [Conventions](../CONVENTIONS.md#function-names) for ownership and naming.

## Implementation

The host owns image preflight, program memory/mappings, descriptor pages,
ABI routing and service adapters. [Graphics adapters](MISL_HOST_GRAPHICS.md)
and [native host services](MISL_HOST_SERVICES.md) describe how guest calls reach
their native owners. [SRAM](../storage/SRAM.md) supplies save persistence and its host binding;
[execution and pumps](MISL_HOST_RUNTIME.md) describes scheduling and halt views.

The [launcher](MISL_LAUNCHER.md), root lifecycle and [native build modes](../engine/FUSELAGE_BUILD.md)
are integrated. Complete Windows executables build. Real application/device
validation remains separate from controlled host tests and informational launcher
checks.

## Session interface

`misl_host.h` declares native `Fuselage...` operations. `MislGuestImage` contains
borrowed file bytes, their byte count and an optional entry override. An override
of zero derives the entry from the manifest, or selects zero for a legacy image.
It does not force entry zero in a manifest-bearing image.

Keep the descriptor and bytes alive and unchanged while loaded. The existing
session selects a new image by descriptor identity; changing its contents in
place is not an explicit reload request. A null descriptor is not a supported
run request. Session calls and pump operations belong on the host's tick thread.

The native operations follow this interface:

| Operation | Meaning |
|---|---|
| `FuselageRunGuest` | Lazy-load, publish device snapshots, run/sleep, flush graphics and drain audio |
| `FuselageIsGuestHalted` | Nonzero after a halt, load refusal or execution fault |
| `FuselageGetGuestHaltInfo` | Return 1 with stop information, or 0 with zeroed output; null output queries only |
| `FuselageSetGuestCaptureDirectory` | Select the host-controlled capture directory before loading |
| `FuselageSetGuestProgramPath` | Supply the program/executable path used by save handling |
| `FuselageSetGuestMemoryCapMB` | Cap program plus extra-pool RAM in MiB; zero adds no cap |

The memory cap excludes the borrowed ROAP file, mapped asset payload and separate
save storage. It is not a process-wide memory quota. Null/empty or overlong path
settings clear the stored path in the existing implementation. Capture and save
file behavior must still pass their own implementation reviews.

Halt kinds retain values 0 through 3: none, deliberate finish, fault and save-load
refusal. A stopped result includes the program counter, instruction word when
readable, stack pointer and return address. Its reason is host-owned text; copy
it if it must survive session changes. Running/not-yet-loaded queries return zero
information. Runtime register/instruction values are captured at the stop, rather
than read later from mutable CPU state. A repeated internal halt preserves the
first cause. Load refusals have zero execution fields because the new guest has
not run. A compressed instruction can be reported as a 16-bit value; an unreadable
or incomplete instruction reports zero. Reasons are copied into bounded host
storage and may be truncated. The scheduler keeps halted descriptors stopped on
subsequent calls.

## Image preflight

`fuselage_guest_plan_image` is an internal, allocation-free operation. Supply a
valid descriptor whose byte count describes readable storage, a host RAM cap and
a separate writable plan. The output must not overlap the descriptor or its
bytes. Success returns `MISL_HOST_IMAGE_OK`; every refusal zeroes the output.
A null output returns `MISL_HOST_IMAGE_ARGUMENT` without writing anything.

The planner:

- separates a recognized asset trailer and checks its version, payload extent,
  directory header and complete directory-record extent;
- validates a recognized manifest, entry location/alignment, pool declaration
  and minimum BOOTSPUR version using only program bytes;
- places assets downward from the shared-space ceiling, then save storage,
  using the established placement alignment;
- sums program and extra-pool RAM in 64-bit arithmetic, checking shared-space,
  host-cap and program-image-plus-stack-reserve bounds before narrowing values.

Default program RAM and placement rules remain those in the MISL map. The plan
reports pool bases/sizes and a borrowed asset pointer; it does not allocate,
copy the program, open saves, publish descriptors or change a running session.

| Result suffix (`MISL_HOST_IMAGE_`) | Refusal |
|---|---|
| `ARGUMENT` | Missing descriptor, bytes, nonempty input or output |
| `FILE_SIZE` | File exceeds this host's shared-address-space limit |
| `TRAILER` | Recognized asset trailer/directory has invalid structure or version |
| `MANIFEST` | Recognized manifest has invalid size/version/entry or pool declaration |
| `ENTRY` | Selected entry is out of the program image, inside its manifest, or not two-byte aligned |
| `ROM_VERSION` | Program requires a newer BOOTSPUR version |
| `ADDRESS_SPACE` | Declared regions do not fit the shared zone |
| `MEMORY_CAP` | Program plus extra-pool RAM exceeds the host cap |
| `PROGRAM_SIZE` | Missing executable bytes or image plus stack reserve exceeds program RAM |

`fuselage_guest_get_image_result_message` returns static explanatory text for any
result, including an unknown value. When several conditions fail, the first
check determines the returned result.

An absent format signature retains legacy behavior. A recognized but malformed
manifest or asset trailer is refused rather than executed as raw program data.
Two-byte entry alignment allows compressed instructions; it is not proof that
the instruction stream is valid or that every requested instruction is supported.

Preflight does not yet validate individual asset entry contents, duplicate IDs,
codecs, decoded sizes, save-ID policy or runtime capabilities beyond the declared
ROM version. Raw images also omit BSS extent. Those checks remain separate
from accepting a placement plan.

## Mapped pages

`misl_host_pages_internal.h` declares page accessors and shared page services. Input and clock
accessors are typed; audio pages and bank refreshes stay in the internal native
interface. These pages belong to an eight-byte-aligned static device window,
independent of per-load RAM. Accessor addresses survive load attempts and failures.
The combined tick resets audio pages on replacement and republishes input/clock
before execution. Memory commit resets sprite/tile storage, submission/status pages
and dirty tracking. Bank tables start empty; the pre-slice pump refreshes them.
No guest header includes this internal interface.

## Load ownership

The internal load path has two steps. `fuselage_guest_prepare_load` retires the
previous execution state, validates the image and embedded BOOTSPUR table,
allocates zeroed RAM and copies the program. It records the attempted descriptor
even on refusal. CPU mappings and the region descriptor remain empty while
preparation is pending. It resets per-guest ABI, sleep, frame-estimate and step
tracking; host configuration and device-window addresses remain stable.

The native save adapter then configures/opens or closes save storage; the memory
layer performs no file I/O. `fuselage_guest_commit_save_load` supplies that binding.
After storage is ready, `fuselage_guest_commit_load` validates its span, initializes
VIVIAN, places SP at the top of program RAM, maps RAM/window/banks/assets/BOOTSPUR/
save storage, and publishes one complete descriptor before marking the session
running. The save span must be readable for its reported size and owned separately
from guest RAM. A declared save needs a nonempty span no larger than its reserved
size; the descriptor reports the actual mapped size. No-save input requires null
storage and zero bytes. Assets and save spans remain borrowed, not freed here.

The scheduler installs the graphics store watch after mapping and before the first
instruction. The combined tick refreshes clocks/banks and resets audio pages;
committing memory alone does not perform that work. Broader native resource
ownership/reset remains separate work.

Failed preparation, save readiness or descriptor publication leaves a halted
attempt with no CPU mappings, published regions or owned guest RAM. The old guest
is not resumed. Mappings and descriptor state are detached before RAM is freed,
so failure reporting never follows a stale CPU pointer. Failure reasons are copied
before releasing storage that may contain the message. This ordering avoids
keeping two full RAM allocations alive during reload.

A runtime halt retains mapped storage for diagnostics until the next preparation
or internal release. The private release helper is resource cleanup; it does not
define a new public unload/restart API. Calls are serialized on the tick thread.
Zeroed allocation is guaranteed by `calloc`; physical residency/commit behavior
depends on the host and is not a memory-use guarantee.

## Mapping permissions

VIVIAN refuses writes through the asset, BOOTSPUR and save spans and through
read-only pixie banks. Tile banks are writable when present; absent banks and
stride gaps fault. A bank descriptor describes its address window, not proof
that every bank is populated. Banks borrow engine storage and must be refreshed
before use when that storage changes.

The current device window is mapped writable as a whole. Input, clock, descriptor
and status read flags express their interface ownership, but do not enforce
read-only access against a guest. The core also has no separate execute-permission
gate for readable spans. Per-region device permissions and execution policy are
FUNDUS hardening work. Region flags alone must not be treated as a security boundary.

## Argument marshalling

Guest arguments occupy `a0` through `a6`; `a7` selects the call. A 32-bit result
uses `a0`, and a 64-bit result uses low/high words in `a0:a1`. Single-precision
values use their 32-bit representations; doubles use low/high pairs. The native
helpers copy floating representations with `memcpy` rather than numeric casts.
Color conversion uses the canonical native RGBA packer and low-byte-red order.

The generic helpers do not depend on graphics types; native Color conversion is
in a separate internal header. Single argument indices must fit 0..6 and pair
indices 0..5; out-of-range helper indices return zero. Return helpers preserve
registers outside their declared result slots. Numeric opcodes and wire records
are unchanged.

## Guest memory access

The input helper resolves the complete requested range and returns a const pointer.
Zero-length input ranges may produce a one-past pointer; no byte may be read from
that result. Unbound CPU storage returns null. Output admission requires a positive
capacity and uses VIVIAN's writable resolver, rejecting asset, system-ROM and save
spans or read-only banks. The device-window permission limits above still apply.

A successful output lookup reports its full capacity to the store watch before
the callee writes. This conservatively marks dirty ranges for post-slice processing;
it is not a report of the exact bytes eventually changed. The current dirty watcher
must not consume new data synchronously at that point. Callees still must respect
the admitted capacity, and typed accesses require suitable host alignment or a
copy through aligned storage. A range check alone does not make a typed cast safe.

Collision batch output is restricted to nonempty ordinary guest RAM. Its helper
does not admit device/bank/ROM output. The separate range check requires two-byte
input address alignment and disjoint virtual intervals using 64-bit end arithmetic.
Callers must also resolve the full input, validate the item count before size
multiplication and account for the actual native pointer alignment.

The string copier always terminates a nonempty output buffer and returns the
copied length plus one of `COMPLETE`, `TRUNCATED`, `UNMAPPED` or `ARGUMENT`.
`COMPLETE` means a guest NUL was found. An unmapped or truncated string still has
a safe local prefix; the caller decides whether that prefix is useful. Null/zero
output arguments are refused without a write. Address arithmetic cannot wrap
from the end of guest address space back into program RAM.

## System services

The system adapter forwards the existing signal, mouse/cursor, vsync, timing,
canvas, clear-color, capture and descriptor calls to their native owners. It uses
the narrow native `BASE/fuselage_runtime.h` declarations rather than importing a full
orchestrator header into every helper. The root supplies these implementations;
see [native orchestration](../engine/FUSELAGE.md).

Signal values retain 0 quit, 1 windowed, 2 borderless, 3 exclusive fullscreen and
4 apply target FPS. The guest signal wrapper currently accepts an integer; it
does not yet publish the native enum names. Setting target FPS records a value;
after startup, signal 4 requests application at the host's safe point. The getter
returns the recorded value, not proof that a pacing change has taken effect.
Non-finite/rate validation remains native scheduling policy, not a bridge guarantee.

Yield and wait-VBL end the current slice. Sleep stores its unsigned tick count and
ends the slice; zero sleeps behave as a yield. The scheduler counts subsequent
sleep ticks without executing the guest. Time uses DICE's elapsed-microsecond query and
preserves the complete 64-bit result. Canvas width/height and focus query GDMF.

A capture request needs a configured host directory and a complete NUL-terminated
guest name of 1..64 ASCII letters, digits, underscores or hyphens. Path syntax,
Windows device basenames and incomplete guest strings are refused. The host joins
the name to its directory with a `.ppm` suffix. A positive result means GDMF
accepted the request, not that the asynchronous file write has finished. The
pending query reports the renderer's global capture state even when this guest
has no capture directory. Filename admission does not by itself guarantee safe
handling of existing files or links in that directory.

## ABI gate and ECALL completion

The host services a call only while the session is running and the current aligned
instruction is an ECALL. Before declaration, only the exact ABI-declare opcode is
admitted. A matching declaration returns one; repeating it is permitted. A mismatch
returns zero and faults, even after an earlier matching declaration. Other calls
before declaration fault without running their native handler.

The router selects the existing service family and passes the full call number to
its handler. Handlers return one to continue the slice or zero to yield. A handled
call advances PC once, including yield/sleep calls. A fault leaves PC at the ECALL,
returns zero and cannot be re-serviced while halted. Completion follows the original
serviced instruction even if a memory service overwrites that instruction's bytes.
Unknown/unimplemented calls fault with the call number in the diagnostic overlay.
Dispatch validation does not add an execute-permission or ROM-origin policy.
