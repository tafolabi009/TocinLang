/* Linker script for the Tocin multiboot kernel.
 *
 * Loads at the traditional 1 MiB mark and forces the .multiboot header to the
 * very front of the image so GRUB finds it within the required first 8 KiB.
 * Entry is _start (defined in kernel.to's module-level asm).
 */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Multiboot header must come first. */
    .multiboot : ALIGN(4)
    {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) }
}
