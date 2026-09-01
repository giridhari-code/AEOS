/*
 * AEOS - Linker Script
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine
 *
 * Memory: 0x40000000 - 0x48000000 (128MB RAM)
 *
 * QEMU's -kernel loader places raw AArch64 images at the Image
 * protocol text_offset (0x80000 into RAM), i.e. VA 0x40080000.
 * The kernel must be linked at that address or every absolute
 * symbol (VBAR_EL1, page tables, heap) points at the wrong page.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

KERNEL_BASE = 0x40080000;

SECTIONS
{
    . = KERNEL_BASE;

    _kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }

    .rodata : { *(.rodata*) }

    . = ALIGN(16);
    .data : { *(.data*) }

    /* GOT as an explicit section: the raw binary (objcopy -O
     * binary) must carry everything the kernel needs at its load
     * address, and _kernel_image_end marks exactly that boundary -
     * used by the self-update path to re-write a byte-exact copy
     * of the running image back to disk. */
    .got : { *(.got*) *(.got.plt*) }
    _kernel_image_end = .;

    . = ALIGN(16);
    _bss_start = .;
    .bss : { *(.bss*) *(COMMON) }
    _bss_end = .;

    . = ALIGN(16);
    _stack_bottom = .;
    . += 16384;
    _stack_top = .;

    _kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.ARM.attributes)
        *(.eh_frame*)
        *(.note*)
    }
}
