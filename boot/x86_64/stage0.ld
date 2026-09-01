OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    /* Low memory, safely below the 1MB payload destination:
     * copying must not overwrite the running loader. */
    . = 0x8000;

    .text : {
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .ksize  : { *(.ksize) }

    . = ALIGN(16);
    _bss_start = .;
    .bss : { *(.bss*) *(COMMON) }
    _bss_end = .;

    .payload : {
        kpayload_start = .;
        *(.payload)
        kpayload_end = .;
    }

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
