# Guest runtime

The guest runtime supplies shared region-cache storage, heap-break entry points
and assembly jumps into BOOTSPUR. It runs inside a ROAP under VIVIAN, independently
of the native engine systems. See [Guest API](MISL_GUEST.md) for startup and
[ABI/MMIO](MISL_ABI_MMIO.md) for the shared contracts.

## Source organization

| File | Purpose |
|---|---|
| `misl_guest_cache.c` | One shared discovery cache per linked guest |
| `misl_guest_cache_internal.h` | Cache declarations used by runtime and facade |
| `misl_guest_heap.c` | Weak `_sbrk` and `sbrk` implementations |
| `misl_guest_heap.h` | Heap-break declarations, also usable from C++ |
| `misl_rt.S` | Weak tail jumps through the system-ROM table |

Link the cache and heap C files and the assembly thunk file with the guest.
The application still supplies its entry point; the runtime does not supply
`_start`, a complete C library, an allocator or a native host entry point.
Linker placement, compiler flags and ROM support must agree with the guest ABI.
The supplied rules and linker are described in [Building a ROAP](BUILDING_A_ROAP.md).

## Shared discovery cache

`misl_guest_descriptor` and `misl_guest_region_bases` have one strong definition
in `misl_guest_cache.c`. The facade fills them during `MISL_PROGRAM_BEGIN()`.
Every application C file and the ROM thunks use that same storage. Do not define
private copies in individual files or substitute an application-owned cache.
Duplicate definitions fail to link rather than quietly creating independent state.

The cache starts at zero and contains guest addresses, not host pointers.
Discover regions before calling a ROM service. A thunk does not validate an
undiscovered cache: it would load a table offset using address zero as the ROM
base and could jump into unrelated program bytes.

## Heap break

`misl_guest_heap.h`, included by `fuselage_misl.h`, declares `_sbrk` and `sbrk`.
These standard runtime names are intentional naming exceptions. Both accept a
signed byte increment, return the previous break on success, and return
`(void*)-1` on refusal. `sbrk` calls the selected `_sbrk` implementation.

The linker must define `_end` as the first available address, aligned to eight
bytes. The initial break is that address. Positive requests round upward to a
multiple of eight; negative requests also round toward positive infinity:
`-1` through `-7` leave the break unchanged, while `-9` reduces it by eight.
`sbrk(0)` reads the current break without moving it.

The default upper bound is the discovered program size minus
`MISL_STACK_RESERVE`. Before discovery, or if the descriptor has no program
entry, the implementation uses `MISL_MEM_SIZE` as a fallback. Discover first,
particularly when the declared program pool differs from that default.
This is a fixed stack reservation, not a measurement of the live stack pointer
or a guarantee against stack overflow.

The implementation checks rounding and address arithmetic before changing the
break. It refuses a request below `_end`, above the heap ceiling, or when the
current break no longer lies within those limits. A program size smaller than
the stack reserve is refused. Descriptor scans reject an oversized region count
or a byte extent too short for the declared entries. These checks bound this
scan; they do not constitute complete descriptor validation.

The heap entries are weak. An application may provide a strong `_sbrk` to select
a different policy, and the default `sbrk` will call that implementation.
`MISL_RT` can override the weak-declaration attribute when rebuilding the C file
for a specialized environment. Heap calls are not synchronized for concurrent
callers. No `malloc` or `free` implementation is implied.

## ROM thunks

Each thunk loads the system-ROM base from the shared region cache, loads its
fixed entry offset, adds the base and tail-jumps to that address. The five
instructions do not add a call frame; the ROM service returns to the original
caller. `MISL_WriteSram` uses the same mechanism as the compiler-required memory
and integer helpers. Its name changed, but its ROM index and ecall remain the
same. Old cache and SRAM-thunk symbol names are not provided as aliases.

Each thunk is weak and occupies a separate text section. Applications may
replace a service with a strong implementation, and section garbage collection
can discard thunks that are not referenced. Compiler-required spellings such
as `memcpy`, `__divdi3` and `sinf` must remain unchanged.

The emitted services depend on the floating ABI:

| ABI | Thunks | Floating services |
|---|---:|---|
| ILP32 | 17 | None; applications supply their required soft-float support |
| ILP32F | 32 | Single precision and integer/single conversions |
| ILP32D | 47 | Single and double precision, with their integer conversions |

Compile the thunks and application with compatible ISA/ABI flags. The ROM's
floating stubs expect the corresponding floating-register calling convention;
the thunk itself does not translate a soft-float call into that convention.
Counts describe the current contract, not every routine a compiler or library
might emit. Check the completed link for unresolved support routines.
