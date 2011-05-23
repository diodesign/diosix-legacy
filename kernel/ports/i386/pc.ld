/* from OsFaqWiki - Higher Half Bare Bones linker script */

ENTRY(_loader)
OUTPUT_FORMAT(elf32-i386)
virt = 0xC0400000;
phys = 0x00400000;
SECTIONS
{
    /* The kernel will live at 3GB + 4MB in the virtual
       address space, which will be mapped to 4MB in the
       physical address space. */
    . = 0xC0400000;
    _knlstart = .;

    .text : AT(ADDR(.text) - 0xC0000000)
    {
        *(.text)
        *(.rodata*)
    }

    .data ALIGN (0x1000) : AT(ADDR(.data) - 0xC0000000)
    {
        *(.data)
    }

    .bss : AT(ADDR(.bss) - 0xC0000000)
    {
        _sbss = .;
        *(COMMON)
        *(.bss)
        _ebss = .;  /* end of the kernel */
    }
}
