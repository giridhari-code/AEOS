/*
 * AEOS x86_64 - MBR boot code linker script (LBA 0 image)
 */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x7C00;

    .text : { *(.text*) }

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
