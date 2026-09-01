/*
 * AEOS user program linker script.
 * Programs are linked at the kernel's USER_VA_BASE (boot/aarch64/vmm.h)
 * so absolute references resolve once mapped there at EL0.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x8000000000;   /* USER_VA_BASE */

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }

    /DISCARD/ : {
        *(.comment)
        *(.ARM.attributes)
        *(.eh_frame*)
        *(.note*)
    }
}
