# Building a ROAP

A ROAP guest needs the SDK's headers, runtime sources, linker script and native
roappack executable, plus a compatible RISC-V C toolchain. It does not link the
engine or require the launcher, graphics/audio SDKs, Python or CMake. GNU Make
is the supplied build convenience; the underlying steps are described below.

The current default is **RV32IMFD / ILP32D**. The reviewed Windows builds use
GNU GCC 14.2 or Clang 22 with GNU RISC-V binutils 2.46. Integer-focused fixtures
also build and execute as RV32IM/ILP32 and RV32IMF/ILP32F. Those checks do not
promise that all soft-float applications link without additional support.

## First build

From a source checkout, build the standalone host packer once:

```sh
make -C fuselage/source/ROAP/tools
make -C sdk/examples/minimal
```

The first command needs a host C compiler. An SDK with a prebuilt roappack does
not need that compiler for guest builds. The second command expects
`riscv32-unknown-elf-gcc` on PATH and produces
`sdk/examples/minimal/build/minimal.roap`.

Set `RISCV` for another cross-tool prefix, or `GUEST_CC` for an explicit compiler.
For an installed SDK, set `ROAPPACK` to its native executable if it differs from
the source-checkout default. The tool is consumed as a prebuilt program; guest
rules never build engine code or write into shared SDK sources.

## Application entry

The starter in `sdk/examples/minimal` declares a manifest and supplies `_start`
in `.text.init`. It calls `MISL_PROGRAM_BEGIN()` before yielding to the host.
Start with that file and replace its loop with application work. See
[Guest API](MISL_GUEST.md#building-and-startup) for the manifest and startup calls.

There is no CRT startup, `main` wrapper, constructor runner or automatic allocator.
The host selects the manifest entry, supplies the stack and zeroes program memory
before loading the file. Initialize/discover the guest interface before using a
ROM service, including compiler helpers that call through BOOTSPUR.

## Make interface

An application Makefile sets its name before including the SDK rules:

```make
NAME := example
include ../sdk/roap.mk
```

Adjust the include path for your directory layout. The default source is
`misl.c`; `SOURCES` can list C and assembly files instead. Source basenames must
be unique because application objects share one output directory.

| Setting | Purpose |
|---|---|
| `SDK_DIR`, `MISL_DIR` | SDK-rule and canonical guest-source locations |
| `RISCV`, `GUEST_CC` | Cross-tool prefix or explicit compiler |
| `GUEST_TARGET_FLAGS` | Compiler target selection, such as Clang's `--target=riscv32-unknown-elf` |
| `GUEST_MARCH`, `GUEST_MABI` | ISA and calling convention, shared by every object |
| `GUEST_COMPILER_INCLUDE` | Compiler-owned freestanding headers; queried by default |
| `GUEST_CFLAGS`, `GUEST_CPPFLAGS`, `EXTRA_INC` | Compilation options and application includes |
| `GUEST_LDFLAGS`, `GUEST_LIBS`, `LDSCRIPT` | Link options, optional support libraries and script |
| `SOURCES`, `EXTRA_SRC`, `ASSETS` | Sources, extra default sources and additional dependencies |
| `BUILD_DIR`, `ROAPPACK` | Application output directory and native packer executable |
| `ROM_SECTIONS` | Optional `id=path` asset list |

`EXTRA_SRC` extends the default source list; setting `SOURCES` replaces that list.
`ROAP`, `ELF` and `FULL` name the generated outputs under `BUILD_DIR`.
`make info` reports the selected compiler, ABI and output. `make clean` removes
the listed outputs and dependency/option files, preserving unrelated files and
shared tools.

The rules track included project/runtime headers and compare build options so
changes to the compiler, ISA/ABI, flags, sources or asset list trigger rebuilding.
Ordinary file changes use Make's timestamps. After replacing a compiler binary
or an SDK while preserving timestamps, use `make clean` before rebuilding.

Build paths and source/asset list entries must not contain whitespace. A checkout
may live below a directory with spaces: run Make from the application directory
and use relative paths without spaces. For an SDK path containing spaces, choose
a relative include path or a filesystem alias. Compiler paths are quoted.

For Clang, select a compatible GNU RISC-V linker through `GUEST_LDFLAGS`, for
example `--ld-path=/path/to/riscv32-unknown-elf-ld`. The supplied link options are
tested with GNU ld. Other linker implementations require their own validation.

## Linker and startup

`roap.ld` places the optional 128-byte manifest first, then text, constants,
initialized data and BSS. `_start` must be the first code in `.text.init`.
Its address is zero without a manifest and 128 with one. Link assertions reject
missing/misplaced entries, malformed manifest sizes and unsupported constructor,
TLS or dynamic-linking sections. They do not validate manually written manifest
field contents; use `MISL_PROJECT` for the supported declaration.

BSS has no file bytes. The linker provides an eight-byte-aligned `_end` after
static storage for the heap break, unless the application explicitly defines it.
The script does not allocate a stack or prove that static storage, heap and stack
fit the host's program pool. See [Guest runtime](MISL_RUNTIME.md#heap-break).

The build disables small-data generation and linker relaxation because this
entry path does not initialize the RISC-V global pointer. It also disables
hosted include paths, PIC/PIE and stack-protector support. Only the SDK shims and
compiler freestanding headers are searched. Including an unavailable hosted
header such as `stdio.h` fails rather than depending on a bundled libc.

The linked runtime consists of `misl_guest_cache.c`, `misl_guest_heap.c`,
`misl_rt.S`, `misl_format.c` and `misl_format_numbers.c`. Unused functions are
discarded. Formatter arithmetic keeps contraction and fast-math disabled.
The ROM thunks provide reviewed services, not every helper a compiler might emit.
ABI changes or additional libraries require compatible objects and a completed
link check. C++ runtime support is not supplied by these C/assembly rules.

## Without Make

Use the same sequence with your build system:

1. Compile application sources and the five runtime files above for one ISA/ABI.
   Use freestanding compilation, SDK/shim/compiler headers, function sections,
   disabled small-data generation/relaxation, and the floating-point flags above.
2. Link without CRT or default libraries using `roap.ld`, `--no-relax`,
   `--gc-sections` and no build ID. GNU ld's RWX-segment warning can be disabled
   for this intentionally flat program image.
3. Run `roappack --flatten example.elf -o example.roap`.

No objcopy step is needed for this guest path. ELF inspection, debugging and
alternative assembly flows may still use the target toolchain's other programs.
See [roappack](ROAPPACK.md) for ELF constraints and packaging commands.

## Optional assets and running

`ROM_SECTIONS = 7=assets/sprites.bin 9=assets/level.bin` adds
`build/example_full.roap`; the original program remains `build/example.roap`.
The convenience list excludes spaces and extra equals signs in paths. Use the
roappack command directly with quoted arguments for those paths. Asset identifiers,
data and changes to the list are checked by the packer/build rules.

Run the resulting ROAP with a separately installed compatible launcher. Building
that launcher is not part of building the guest. Native platform packages and
real-application execution are separate acceptance work from the controlled SDK
builds described here.
