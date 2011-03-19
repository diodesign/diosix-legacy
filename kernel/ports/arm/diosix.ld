ENTRY(_Reset)

phys = 0x00010000;
virt = 0xc0010000;

SECTIONS
{
   . = 0xc0010000;
   _knlstart = .;

   .startup . :
   {
      kernel/ports/arm/objs/startup.o(.text)
   }

   .text : 
   { 
      *(.text)
   }

   .data : 
   { 
      *(.data)
   }

   .bss : 
   {
      _sbss = .;
      *(.bss)
      . = . + 4096; /* 4kB of stack memory */
      _ebss = .;
      KernelBootStackBase = .;
   }
}
