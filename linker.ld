/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   GRUB Linker Script for Milestone 8: Project Structure + ISO (AetherOS-64)
   ============================================================== */

ENTRY(_start)           /* Define our entry point symbol from boot.asm */

SECTIONS
{
    /* GRUB loads our kernel at the physical memory address 1MB (0x100000).
       This is standard for Multiboot kernels to avoid BIOS areas. */
    . = 0x100000;

    _kernel_start = .;

    /* Code Section */
    .text :
    {
        /* Ensure the Multiboot2 header is placed at the very beginning
           of our text section. This guarantees that GRUB finds it within
           the first 8192 bytes of the file. */
        KEEP(*(.multiboot_header))
        *(.text)        /* Include all standard code sections */
    }

    /* Read-Only Data Section (Aligned to 4KB page boundaries) */
    .rodata : ALIGN(0x1000)
    {
        *(.rodata)      /* Include constant strings, static tables, etc. */
    }

    /* Initialized Writeable Data Section (Aligned to 4KB page boundaries) */
    .data : ALIGN(0x1000)
    {
        *(.data)        /* Include initialized global variables */
    }

    /* Uninitialized Data Section (Aligned to 4KB page boundaries) */
    .bss : ALIGN(0x1000)
    {
        _bss_start = .;
        *(.bss)         /* Include uninitialized global variables */
        *(COMMON)       /* Include common variables */
        _bss_end = .;
    }

    _kernel_end = .;
}
