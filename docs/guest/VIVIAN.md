# VIVIAN

## Core interface

VIVIAN is the independent execution core. MISL supplies the guest-facing system
layer; ROAP is the program file format. The core resolves memory and executes
instructions without calling other Fuselage systems. Its public header is
`fuselage/source/VIVIAN/vivian.h`.

The core includes execution, instruction decoding and a standalone Makefile.
The behavior below describes the interpreter as implemented, not a promise of
complete RISC-V compliance. Host loading, MISL services and ROAP integration are
documented separately in [the native host guide](MISL_HOST.md).

`VIVIAN_Step` attempts one instruction. `VIVIAN_Run` attempts up to `step_budget`
instructions and optionally writes the attempt count to `steps_executed`. The
count includes the attempt that stops execution, including a failed fetch.
A zero budget returns `VIVIAN_OK` and a zero count without fetching.

| Status | Value | Meaning |
| --- | --- | --- |
| `VIVIAN_OK` | 0 | The requested instruction budget was consumed. This does not mean the program has finished. |
| `VIVIAN_HALT` | 1 | An EBREAK was encountered. The host decides whether execution is finished. |
| `VIVIAN_ECALL` | 2 | An ECALL requires host service. |
| `VIVIAN_ILLEGAL_INSTRUCTION` | 3 | An instruction or encoding is unrecognized, reserved or unsupported. |
| `VIVIAN_MISALIGNED_FETCH` | 4 | The instruction address is not aligned to a two-byte boundary. |
| `VIVIAN_BUS_ERROR` | 5 | An instruction access could not be resolved, or an atomic access had invalid alignment. |
| `VIVIAN_UNSUPPORTED_ROUNDING` | 6 | A floating-point operation or control write requested a rounding mode the interpreter does not support there. |

On ECALL or EBREAK, `pc` still identifies that instruction. After servicing an
ECALL, call `VIVIAN_CompleteEcall` once to advance by four bytes. It does not check
the instruction or remember a pending call. Running again without advancing
reports the same instruction. There is no persistent halted-state flag. Fault
reporting does not promise transactional rollback of all state changes.

`VIVIAN_ExpandCompressedInstruction` returns one for a successful expansion to a
32-bit instruction, zero otherwise. Supply a valid output pointer and use its
value only on success. Expansion alone does not execute the instruction.

## State and ownership

The caller owns `VivianState` and all mapped storage, pointer tables, permission
tables and callback data. Keep that storage valid while it can be accessed.
Serialize execution, mapping changes and host access to the state; the interface
does not provide concurrent access protection.

`VIVIAN_Init` initializes execution registers and mapping activity, records the
borrowed flat memory and entry address, and establishes the calling thread's
floating-point controls. It does not clear program bytes, validate the entry
address or validate the backing allocation. It does not zero every byte of the
state: inactive bank metadata and structure padding need not be cleared.

`regs` contains 32 integer register slots. Initialization zeros x0 and instruction
writes to x0 are ignored. The caller must preserve `regs[0]` as zero: the current
read helper reads the stored slot. `fregs` contains 32 64-bit floating-point
slots; f0 is an ordinary register. Single-precision instruction results use
NaN boxing. Initialization sets the raw floating-point slots to zero, which is
not a NaN-boxed single-precision zero. `fcsr` contains rounding control and sticky
exception flags. `lrValid` and `lrAddr` track the load-reserved reservation.

Existing field spellings and layouts are retained in this pass. Prefer the
mapping functions over manually changing their related fields.

## Memory mappings

The flat memory covers guest addresses starting at zero. `VIVIAN_MapWindow` adds
one writable span at an arbitrary base. `VIVIAN_MapReadOnlySpan` selects one of
three read-only spans; `VIVIAN_MapReadOnlyMemory` selects span zero. A null span
pointer disables that mapping. An invalid read-only slot leaves mappings alone.

`VIVIAN_MapBanks` selects one of two bank sets. Each bank has `bank_size` usable
bytes at a stride of `1 << bank_shift`, starting at `base`. Bank pointers and
per-bank permission bytes are borrowed and read on access. A null bank is absent;
a null permission table makes all banks in that set read-only. A nonzero
permission byte permits writing to that bank. Accesses cannot cross bank edges,
even when adjacent banks happen to share an allocation.

An invalid bank-set index leaves existing mappings alone. For a valid index,
a missing table, zero bank count, shift of 32 or more, or size larger than the
stride disables the set. A zero bank size is accepted but cannot resolve a
positive-length access. These setters do not validate overlaps, backing allocation
sizes or whether an entire mapping fits the guest address space.

`VIVIAN_ResolveMemory` checks flat memory, the window, read-only spans in slot order,
then bank sets in order. `VIVIAN_ResolveWritableMemory` checks flat memory, the
window, then writable banks; it skips read-only spans. Both return a pointer when
the requested range fits a backing region, or null when it cannot be resolved.
Neither resolver checks alignment. Supply valid backing storage and positive
lengths; zero-length and null-backing edge behavior is not a validated contract.

Prevent overlaps. In particular, overlapping read-only and writable mappings can
resolve the same address to different storage for reading and writing. A failure
inside a selected bank also need not fall through to another overlapping set.
The read resolver retains a mutable `void*` return type for compatibility; this
does not grant permission to write read-only storage. Host service code must
validate guest pointers with the resolver appropriate to its access direction.

`VIVIAN_SetStoreWatch` registers one half-open range `[base, end)`. A null callback
disables notifications. The callback receives the complete store range, not just
the overlapping bytes. After a direct host write, `VIVIAN_NotifyStore` performs the
watch notification; it currently does not invalidate a load-reserved reservation.
Watch range arithmetic near the end of the address space and zero-length
notifications need hardening. Do not treat the watch as a permission check or a
complete mechanism for maintaining atomic reservations.

## Floating-point environment

`VIVIAN_EstablishFloatingPointEnvironment` requests nearest-even rounding and clears
the supported architecture's flush/rounding control bits. It changes the calling
thread's controls and does not restore the caller's previous environment.
Initialization calls it on the initializing thread; execution on another thread
requires establishing that thread's environment too.

`VIVIAN_EnsureFloatingPointEnvironment` checks those controls and attempts to repair
drift. Zero means it detected no drift; nonzero means it detected drift and
attempted correction. The result does not certify that every host operation
succeeded. Step and Run do not automatically perform this check for each slice.

The current arithmetic implementation principally uses nearest-even rounding;
explicit float-to-integer conversions handle additional modes. Unsupported and
reserved encodings can report different fault statuses depending on the
instruction. A complete instruction-by-instruction support table awaits the
implementation review.

Floating exception flags can remain pending during execution and are synchronized
to `fcsr` on execution return and guest control-register operations. A store-watch
callback must not assume that `fcsr` already includes every preceding operation.
Cross-platform bit identity, caller environment restoration and complete ISA
conformance remain validation work.

## Floating-point helpers

Private register helpers distinguish bit-preserving moves from arithmetic
results. Single-precision operands are checked for NaN boxing; an unboxed operand
reads as the canonical quiet NaN. Raw moves retain the low word. Arithmetic result
writes canonicalize NaNs, while bit writes preserve payloads. Double-precision
values use the entire register. Classification and signaling-NaN detection inspect
bit patterns directly.

Float-to-integer conversion rounds before checking the signed or unsigned 32-bit
range. Supported encodings select nearest-even, truncation, floor, ceiling or
nearest with ties away from zero; dynamic rounding currently uses nearest-even.
Out-of-range values saturate and raise invalid; in-range fractional conversions
raise inexact. Host rounding/library operations can also affect exception flags,
so exact flag behavior still needs instruction-level conformance validation.

The conversion helper currently handles NaNs before validating the rounding
encoding. Consequently, reserved rounding encodings can escape rejection for
NaN inputs. This is a known issue awaiting correction and focused instruction
tests. These private helpers are implementation details, not additional public
API entry points.

## Compressed instructions

The private fetch helper expands a 16-bit instruction into its 32-bit equivalent.
The ordinary decoder then executes that result. Expansion does not read program
memory, change registers or perform a branch; it rearranges fields and produces
an instruction encoding. Branch and jump offsets retain their encoded values;
the execution loop remains responsible for the original instruction's length.

`VIVIAN_ExpandCompressedInstruction` exposes the same expansion for host tools
and tests. Supply a valid host output pointer. A return value of one means the
current decoder accepted the input; zero means it rejected it. Use the output
only on success. Inputs whose low two bits are both set are not compressed
instructions and are rejected here.

The current decoder includes RV32 integer and single/double floating compressed
forms. It rejects several encodings described as hints in the original source,
while accepting others. The precise hint/reserved/extension policy still needs
an ISA conformance review; acceptance is not a general legality certificate.

Migration verification covers all 65,536 inputs against ENTERON, using two initial
output patterns and comparing the public and private paths. The existing GNU
assembler reference also passes 49 compressed/wide pairs and three rejection
checks. These establish preservation and selected independent encoding checks,
not complete instruction execution or ISA conformance.

## Instruction memory access

Private load/store helpers handle 8-, 16-, 32- and 64-bit little-endian values.
Each resolves the entire range before accessing bytes, so a failed resolution
does not perform part of a store. The helpers do not enforce alignment or join
separate mappings into one access. Adjacent mapped regions cannot jointly supply
one instruction or data access. Atomic instruction alignment is handled by the
instruction decoder.

Stores use writable resolution. Store-watch notification and reservation
invalidation belong to the instruction or host-write caller; the byte-transfer
helpers do not perform them. These helpers are private and expect valid state
and host output pointers.

## Instruction fetch

Fetch requires two-byte pc alignment. The fast path reads four bytes when they
fit in flat memory or a read-only span. Otherwise, the general resolver supplies
a halfword first and, for an uncompressed instruction, the complete word.
A valid compressed instruction at the final two bytes of a mapping can therefore
execute without an unnecessary four-byte access.

The helper returns a decoded instruction and its original size, two or four
bytes. It leaves pc updates and attempt counting to the execution loop. Failed
fetches report misalignment, bus error or illegal compressed encoding. Execution
retains the original size for sequential pc updates and jump return addresses.

Mapping overlaps remain unsupported by caller convention. The fast fetch path
checks read-only spans before falling back to window/bank resolution, while an
ordinary read checks the window before read-only spans. Overlapping mappings can
therefore make instruction fetch and data reads see different bytes. This
confirmed inconsistency is recorded for FUNDUS hardening.

## Execution loop and instruction families

`VIVIAN_Run` executes a budget within one shared loop. `VIVIAN_Step` calls that
loop with a budget of one. Fetch reports the expanded instruction and original
length; decoding extracts opcode, registers, function fields and signed
immediates. The instruction updates a candidate next pc. Successful execution
commits it; a stopping status uses the common epilogue and counts that attempt.
The count is attempts, not a hardware retired-instruction counter.

The loop lives in `vivian_execute.c`. Seven private `.inc` files group its switch
cases without adding an instruction-dispatch call:

| Fragment | Responsibility |
| --- | --- |
| `vivian_instructions_control.inc` | Upper immediates, jumps and conditional branches |
| `vivian_instructions_memory.inc` | Integer loads and stores |
| `vivian_instructions_atomic.inc` | Word LR/SC and atomic read/modify/write |
| `vivian_instructions_integer.inc` | Integer arithmetic, shifts, multiply and divide |
| `vivian_instructions_float_memory.inc` | Floating register loads and stores |
| `vivian_instructions_float.inc` | Single/double arithmetic, fused operations, conversions and comparisons |
| `vivian_instructions_system.inc` | ECALL, EBREAK and floating control/status register operations |

FENCE is handled in the loop as a no-op; FENCE.I is rejected. The fragments are
required source files included by `vivian_execute.c`, not separate translation
units or public APIs. They share its decoded operands, next pc, floating-flag
state and fault path.

Integer add/subtract uses unsigned arithmetic for 32-bit wraparound. Divide and
remainder handle zero divisors and the signed minimum divided by minus one.
Atomic operations require word alignment. They are interpreted memory operations,
not native host atomic primitives. One guest execution stream does not make
concurrent host access safe; callers must enforce memory ownership and ordering.

Floating exception collection starts lazily on the first relevant instruction
and synchronizes at control-register access and slice return. The only exposed
CSRs are fflags, frm and fcsr. Rounding control remains restricted as described
above. CSR operations clear the collected host flags so a guest clear is not
undone at the next synchronization.

Known limitations still include incomplete encoding checks, reservation handling
differences between store paths, and host-dependent floating comparisons and
signed-zero minimum/maximum behavior. These remain explicit FUNDUS work; passing
the migration comparisons does not certify ISA conformance.

## Building VIVIAN

Use the Makefile in `fuselage/source/VIVIAN` with GNU Make, a GCC-compatible
Clang/GCC C11 compiler, and an `ar`-compatible archiver. The compiler must supply
the standard math and floating-environment facilities. No other Fuselage system,
guest toolchain or CMake project is needed to build the library.

```sh
cd fuselage/source/VIVIAN
make CC=clang AR=llvm-ar
make print-libs
```

On native Windows, use `mingw32-make` in place of `make`. The output defaults to
`fuselage/build/VIVIAN/windows/libvivian.a`, with `macos` or `linux` selected for
those compiler targets. `PLATFORM` controls the output label and does not retarget
the compiler; select the appropriate compiler through `CC` for a cross build.

`print-sources` lists the five production C files. Include `vivian.h` in an
application and link the archive followed by the flags from `print-libs`, normally
`-lm`. All seven private headers and seven `.inc` fragments are required source
material. Fragments are included by the execution source and do not have separate
object files.

Tools and paths are configurable:

```sh
make CC=clang AR=llvm-ar BUILD_DIR=../../build/VIVIAN/custom \
     CFLAGS="-O2 -g" CPPFLAGS="-DMY_APPLICATION_BUILD=1"
```

`VIVIAN_LDLIBS` can override the reported math linkage for another toolchain.
The Makefile adds C11 mode and compiler-generated dependency files. A changed
private fragment rebuilds its owner. Use a separate output directory or clean
when changing compilers, compiler flags or target architecture; their command
lines are not themselves dependency files.

Keep fast-math disabled: the helpers depend on NaN and signed-zero behavior.

Checkout paths may contain spaces. A custom `BUILD_DIR` must be nonempty and
contain no whitespace; use a relative output path when building from a checkout
with spaces. Run this Makefile from its directory, or use Make's `-C` option.
`clean` removes only the listed objects, dependency files and archive, leaving
unrelated files and the directory intact. Archive creation replaces the previous
archive so removed sources cannot leave stale members behind.

Native Windows x64/Clang validation covers the archive, all 15 public symbols,
standalone linking, relocation, fragment dependencies and bounded clean. The
consumer has no winpthreads import. The macOS archive builds natively and runs
guests in the engine; the regression suites have not been run there, and Linux
native builds and runtime checks remain pending.
MSVC/clang-cl command-line compatibility is not supplied by this Makefile.
