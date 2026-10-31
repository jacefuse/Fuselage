/* Flat guest image layout. SEE: (BUILDING_A_ROAP.md, Linker and startup) */
ENTRY(_start)

SECTIONS
{
    . = 0;
    .misl_manifest : {
        KEEP(*(.misl_manifest))
    }
    .text : {
        *(.text.init)
        *(.text*)
    }
    .rodata : {
        *(.rodata*)
        *(.srodata*)
    }
    .data : {
        *(.data*)
        *(.sdata*)
    }
    .bss : {
        *(.bss*)
        *(.sbss*)
        *(COMMON)
    }
    . = ALIGN(8);
    PROVIDE(_end = .);

    /* These require startup support that this guest runtime does not provide. */
    .misl_unsupported : {
        KEEP(*(.preinit_array* .init_array* .fini_array* .ctors* .dtors*))
        KEEP(*(.tdata* .tbss* .dynamic .dynsym .dynstr .got* .plt* .interp .init .fini))
    }
    /DISCARD/ : {
        *(.comment)
        *(.riscv.attributes)
        *(.note*)
        *(.eh_frame*)
    }
    ASSERT(DEFINED(_start), "ROAP requires _start")
    ASSERT(SIZEOF(.misl_manifest) == 0 || SIZEOF(.misl_manifest) == 128,
           "ROAP requires zero or one 128-byte manifest")
    ASSERT(ADDR(.misl_manifest) == 0, "ROAP manifest must start at zero")
    ASSERT(ABSOLUTE(ADDR(.text)) == ABSOLUTE(SIZEOF(.misl_manifest)), "ROAP text must follow its manifest")
    ASSERT(_start == ADDR(.text), "ROAP _start must be first in .text; use .text.init")
    ASSERT(SIZEOF(.misl_unsupported) == 0, "ROAP does not support constructors, TLS or dynamic linking")
}
