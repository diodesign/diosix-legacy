ENTRY(_Reset)

phys = 0x00010000;

SECTIONS
{
   . = 0x10000;
   _knlstart = .;

   .startup . :
   {
      kernel/ports/arm/objs/startup.o(.text)
   }

   .text : 
   { 
      *(.text)
   }

   .data ALIGN (0x1000) : 
   { 
      *(.data)
   }

   .bss ALIGN (0x1000) : 
   {
      _sbss = .;
      *(.bss)
      . = . + 4096; /* 4kB of stack memory */
      _ebss = .;
      KernelBootStackBase = .;
   }
}
