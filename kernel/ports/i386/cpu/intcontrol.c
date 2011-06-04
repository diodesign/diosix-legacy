/* kernel/ports/i386/cpu/intcontrol.c
 * i386-specific interrupt initialisation and table control
 * Author : Chris Williams
 * Date   : Tues,13 Oct 2009.23:23:00

Cribbed from... http://www.jamesmolloy.co.uk/tutorial_html/
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* interrupt table entries */
typedef struct
{
   unsigned short base_lo;             // the lower 16 bits of the address to jump to when this interrupt fires
   unsigned short segment;             // kernel segment selector
   unsigned char  always0;             // This must always be zero
   unsigned char  flags;
   unsigned short base_hi;             // the upper 16 bits of the address to jump to
} __attribute__((packed)) int_idt_descr;

/* pointer to array of interrupt handlers suitable for lidtr */
typedef struct 
{
   unsigned short limit;
   unsigned int  base;                 // pointer to table of int_idt_descr blocks
} __attribute__((packed)) int_idt_ptr;

/* here lies our main interrupt handler table pointer and the table */
int_idt_ptr int_table_ptr;
int_idt_descr int_idt_table[256];

/* int_reload_idtr
   Reload the idtr on this cpu */
void int_reload_idtr(void)
{
   INT_DEBUG("[int:%i] loading idtr with %p (%x %x)\n",
             CPU_ID, &int_table_ptr, int_table_ptr.base, int_table_ptr.limit);   
   
   x86_load_idtr((unsigned int *)&int_table_ptr); /* in start.s */
}

/* int_set_gate
   Update the interrupt handler table (int_idt_table) with new data
   => intnum  = interrupt number
      base    = address of routine
      segment = kernel segment selector for routine
      flags   = handler descriptor flags
      flush   = set to non-zero to force a reload of the cpu idt reg
*/
void int_set_gate(unsigned char intnum, unsigned int base, unsigned short segment, unsigned char flags, unsigned char flush)
{
   INT_DEBUG("[int:%i] setting gate: #%i = %x:%x\n", CPU_ID, intnum, segment, base);
   
   int_idt_table[intnum].base_lo  = base & 0xFFFF;
   int_idt_table[intnum].base_hi  = (base >> 16) & 0xFFFF;
   int_idt_table[intnum].segment = segment;
   int_idt_table[intnum].always0  = 0; /* mandatory zero */
   int_idt_table[intnum].flags    = flags;
   
   if(flush)
      int_reload_idtr();
}

/* default timer handler for the scheduler */
kresult int_common_timer(unsigned char intnum, int_registers_block *regs)
{   
   /* nudge the timeslice counters for the current thread */
   sched_tick(regs);
   return success;
}

/* int_initialise_common
   initialise the common entries in the int table for uni and multiproc systems */
void int_initialise_common(void)
{
   INT_DEBUG("[int:%i] initialising interrupt vector table... \n", CPU_ID);
   
   /* initialise table pointer and idt table */
   int_table_ptr.limit = (sizeof(int_idt_descr) * 256) - 1;
   int_table_ptr.base  = (unsigned int)&int_idt_table;
   vmm_memset(&int_idt_table, 0, (sizeof(int_idt_descr) * 256));   
   
   /* get exceptions handled */
   exceptions_initialise();
}

/* int_initialise
   Set up interrupt handling and device management */
kresult int_initialise(void)
{
   /* initialise the basic PIC chipset */
   pic_initialise();

   /* on a uniproc machine? */
   if(!mp_is_smp)
   {      
      /* set up a 100Hz ticker for the scheduler  */
      INT_DEBUG("[int:%i] uniproc: set up timer (%iHz)...\n", CPU_ID, SCHED_FREQUENCY);
      x86_timer_init(SCHED_FREQUENCY);
      irq_register_driver(PIC_8254_IRQ, IRQ_DRIVER_FUNCTION | IRQ_DRIVER_LAST, 0, &int_common_timer);
   }

   /* initialise the smp system's IOAPIC */
   if(mp_ioapics) ioapic_initialise(0);

   /* initialise the boot cpu's local APIC if we're on a multiprocessor machine */
   if(mp_is_smp) lapic_initialise(INT_IAMBSP);
   
   return success;
}
