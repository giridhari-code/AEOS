/*
 * AEOS-BIOS - Linker Script
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine flash (pflash0 @ 0x00000000)
 *
 * Code and read-only data execute in place from flash. Writable
 * sections (.data/.bss/stack) live in RAM at BIOS_RAM_BASE; their
 * load images trail the read-only text in flash and start.S copies
 * them across before any C code runs.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

FLASH_BASE = 0x00000000;
BIOS_RAM_BASE = 0x40200000;     /* above DTB/kernel load window */

SECTIONS
{
    . = FLASH_BASE;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }

    .rodata : { *(.rodata*) *(.data.rel.ro*) }

    /* Writable sections: VMA in RAM, LMA in flash right after
     * rodata so the raw binary carries their initial contents. */
    . = ALIGN(16);
    __data_load = .;                    /* LMA of .data (in flash) */
    .data BIOS_RAM_BASE : AT(__data_load) {
        __data_start = .;
        *(.data*)
        __data_end = .;
    }

    . = ALIGN(16);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*) *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    __stack_bottom = .;
    . += 65536;
    __stack_top = .;

    /DISCARD/ : {
        *(.comment)
        *(.ARM.attributes)
        *(.eh_frame*)
        *(.note*)
    }
}
