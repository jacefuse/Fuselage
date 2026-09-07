/* roap.ld -- linker script for ROAP blobs.
 *
 * The VPU loads a blob at address 0 and sets pc there, so everything is laid
 * out flat starting at 0 with .text first (the entry point at offset 0). No
 * MMU, no relocation at load time -- what objcopy emits as a raw binary is
 * exactly the image the VPU executes. sp is set by the host to the top of VPU
 * memory, so this script owns only the static image, not the stack.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x0;

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

    /* Drop metadata that has no meaning inside the VPU's flat image. */
    /DISCARD/ : {
        *(.comment)
        *(.riscv.attributes)
        *(.note*)
        *(.eh_frame*)
    }
}
