OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_BASE = 0x100000;    /* 1MB, multiboot loads here */

SECTIONS
{
    . = KERNEL_BASE;

    _kernel_start = .;

    .text : {
        /* Entry code FIRST: stage0 far-jumps to offset 0.
         * Multiboot header must merely live in the first 8KB. */
        KEEP(*(.text))
        KEEP(*(.multiboot))
    }

    .rodata : { *(.rodata*) }

    . = ALIGN(16);
    _kernel_image_end = .;

    _bss_start = .;
    .bss : { *(.bss*) *(COMMON) }
    _bss_end = .;

    _kernel_end = .;

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
