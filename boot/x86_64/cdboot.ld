/*
 * AEOS x86_64 - El Torito boot image linker script
 * The BIOS loads this image at 0x7C00; the kernel payload rides
 * behind the code in .payload.
 */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x7C00;

    .text : { *(.text*) }
    .payload : { *(.payload*) }

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }

    /* Cross-section size arithmetic lives here where both
     * endpoints are already placed. */
    IMAGE_SIZE = payload_end - _start;
    TOTAL_SECTORS = (IMAGE_SIZE + 511) / 512;
}
