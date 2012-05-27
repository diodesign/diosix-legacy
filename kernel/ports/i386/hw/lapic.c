/* kernel/ports/i386/hw/lapic.c
 * i386-specific local APIC handling
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

/* hardware interrupts for the local APIC */
extern void irq16(); extern void irq17(); extern void irq18(); extern void irq19();
extern void irq20(); extern void irq21(); extern void irq22();

/* inter-processor interrupts (142-143) - sent to warn other cores of changes */
extern void isr142(); extern void isr143();

/* local apic timer measurements for multiprocessor systems */
volatile unsigned char lapic_preflight_timer_pass = 0;
volatile unsigned int lapic_preflight_timer_lap[APIC_TIMER_PASSES];
volatile unsigned int lapic_preflight_timer_init = 0;

/* default handler for local APIC's IRQs - just EOI the interrupt */
kresult lapic_irq_default(unsigned char intnum, int_registers_block *regs)
{
   LAPIC_DEBUG("[lapic:%i] default lAPIC IRQ handler called: %i\n", CPU_ID, intnum);
   lapic_end_interrupt();
   return success;
}

/* Local APIC timer handler during timer preflight check - used to calibrated the
   lAPIC timer for the scheduler */
kresult lapic_preflight_timer(unsigned char intnum, int_registers_block *regs)
{
   /* sample the lAPIC's current timer value and store it while the index variable 
      is valid for the array */
   if(lapic_preflight_timer_pass < APIC_TIMER_PASSES)
      lapic_preflight_timer_lap[lapic_preflight_timer_pass++] = (*LAPIC_TIMERNOW) / APIC_TIMER_PASSES;
   
   /* if the index variable hits the APIC_TIMER_PASSES limit then disable further interrupts
      to give the boot thread a chance to detect this test loop's end condition */
   if(lapic_preflight_timer_pass >= APIC_TIMER_PASSES)
      regs->eflags = regs->eflags & ~X86_EFLAGS_INTERRUPT_ENABLE;
   
   /* reload to max value */
   lapic_write(LAPIC_TIMERINIT, 0xffffffff);

   return success;
}

/* lapic_end_interrupt 
   Signal the end of handling an interrupt in this lAPIC */
void lapic_end_interrupt(void)
{
   LAPIC_DEBUG("[lapic:%i] ending interrupt\n", CPU_ID);
   lapic_write(LAPIC_EOI, 0);
}

/* lapic_write
   Write to an APIC register. Due to a silly Pentium bug, it's best to
   read from the register before wriitng to it..
   => addr  = address to write to
      value = 32bit value to write
*/
void lapic_write(volatile unsigned int volatile *addr, volatile unsigned int value)
{
#ifdef BUGS_PENTIUM_LAPIC_RW
   volatile unsigned int read = *addr;
   LAPIC_DEBUG("[lapic:%i] Pentium bug workaround: read %x from lapic address %p before writing %x\n",
               CPU_ID, read, addr, value);
#endif
   
   /* simple, eh? */
   *addr = value;
}

/* int_apic_icr_ready
   Wait until the local APIC's IPI-pending flag is zero */
void lapic_icr_ready(void)
{
   while((*LAPIC_ICR_LO) & LAPIC_PENDING) __asm__ __volatile__("pause");
}

/* lapic_ipi_broadcast
   Broadcast a given interrupt to all other processors in the system */
void lapic_ipi_broadcast(unsigned char vector)
{
   LAPIC_DEBUG("[lapic:%i] sending IPI %i to all cores\n", CPU_ID, vector);
   
   lapic_icr_ready(); /* wait for the command reg to be ready */
   
   /* write to HI, then LO to send */
   lapic_write(LAPIC_ICR_HI, 0xff000000); /* destination: all */
   lapic_write(LAPIC_ICR_LO, LAPIC_ALLBUTSEL | LAPIC_ASSERT | vector);
}

/* lapic_ipi_send
   Send a given IPI to a given core */
void lapic_ipi_send(unsigned char destination, unsigned char vector)
{
   LAPIC_DEBUG("[lapic:%i] sending IPI %i to core %i\n", CPU_ID, vector, destination);
   
   lapic_icr_ready(); /* wait for the command reg to be ready */
   
   /* write to HI, then LO to send */
   lapic_write(LAPIC_ICR_HI, destination << LAPIC_DEST_SHIFT);
   lapic_write(LAPIC_ICR_LO, LAPIC_ASSERT | vector);
}

/* lapic_ipi_send_init
   Send an INIT IPI to the given processor
*/
void lapic_ipi_send_init(unsigned char destination)
{
   LAPIC_DEBUG("[lapic:%i] sending INIT IPI (%x) to AP %i..\n",
               CPU_ID, LAPIC_TYPE_INIT | LAPIC_TRG_LEVEL | LAPIC_ASSERT, destination);

   lapic_icr_ready(); /* wait for the command reg to be ready */
   
   /* write to HI, then LO to send */
   lapic_write(LAPIC_ICR_HI, destination << LAPIC_DEST_SHIFT);
   lapic_write(LAPIC_ICR_LO, LAPIC_TYPE_INIT | LAPIC_TRG_LEVEL | LAPIC_ASSERT);
   
   /* don't forget to deassert the INIT */
   lapic_write(LAPIC_ICR_LO, LAPIC_TYPE_INIT | LAPIC_TRG_LEVEL | LAPIC_ALLINCSEL);
}

/* int_ipi_send_startup
   Send a startup IPI to the given processor using the given vector
*/
void lapic_ipi_send_startup(unsigned char destination, unsigned char vector)
{
   LAPIC_DEBUG("[lapic:%i] sending startup IPI (%x) to AP %i..\n",
               CPU_ID, vector | LAPIC_TYPE_START | LAPIC_ASSERT, destination);
   
   lapic_icr_ready(); /* wait for the command reg to be ready */

   /* write to HI, then LO to send */
   lapic_write(LAPIC_ICR_HI, destination << LAPIC_DEST_SHIFT);
   lapic_write(LAPIC_ICR_LO, vector | LAPIC_TYPE_START | LAPIC_ASSERT);
}

/* lapic_initialise
   Set up a cpu's local APIC on a multiprocessor system
   => flags = flag up who's calling this function
              INT_IAMBSP = 1 for the BSP, which will create the int table
*/
void lapic_initialise(unsigned char flags)
{   
   LAPIC_DEBUG("[lapic:%i] programming APIC on cpu %i\n", CPU_ID, CPU_ID);
      
   if(flags & INT_IAMBSP)
   {
      /* install the APIC int handlers */
      int_set_gate(IRQ_APIC_TIMER, (unsigned int)irq16, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_TIMER, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);

      int_set_gate(IRQ_APIC_LINT0, (unsigned int)irq17, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_LINT0, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);

      int_set_gate(IRQ_APIC_LINT1, (unsigned int)irq18, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_LINT1, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);

      int_set_gate(IRQ_APIC_PCINT, (unsigned int)irq19, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_PCINT, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);

      int_set_gate(IRQ_APIC_SPURIOUS, (unsigned int)irq20, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_SPURIOUS, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);

      int_set_gate(IRQ_APIC_THERMAL, (unsigned int)irq21, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_THERMAL, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);

      int_set_gate(IRQ_APIC_ERROR, (unsigned int)irq22, 0x18, 0x8E, 0);
      irq_register_driver(IRQ_APIC_ERROR, IRQ_DRIVER_FUNCTION, 0, &lapic_irq_default);
      
      /* catch IPI messages */
      int_set_gate(INT_IPI_RESCHED, (unsigned int)isr142, 0x18, 0x8E, 0);
      int_set_gate(INT_IPI_FLUSHTLB, (unsigned int)isr143, 0x18, 0x8E, 1); /* reload idt */
   }
      
   /* program the APIC's registers so that interrupts point towards
    the correct entries in the table of handlers  - start with the 
    destination and task priority registers before enabling the APIC */
   lapic_write(LAPIC_DESTID,  0xff000000);
   lapic_write(LAPIC_DESTFMT, 0xffffffff); /* force into flat SMP mode */
   lapic_write(LAPIC_TASKPRI, 0); /* clear the task priority reg so all ints are handled */  
   
   lapic_write(LAPIC_SPURIOUS_REG, IRQ_APIC_SPURIOUS | LAPIC_ENABLE);
   lapic_write(LAPIC_LVT_TIMER,    IRQ_APIC_TIMER);
   lapic_write(LAPIC_LVT_LINT0,    IRQ_APIC_LINT0);
   lapic_write(LAPIC_LVT_LINT1,    IRQ_APIC_LINT1 | LAPIC_LVT_NMI);
   lapic_write(LAPIC_LVT_PCOUNTER, IRQ_APIC_PCINT | LAPIC_LVT_MASK);
   lapic_write(LAPIC_LVT_THERMAL,  IRQ_APIC_THERMAL);
   lapic_write(LAPIC_LVT_ERROR,    IRQ_APIC_ERROR);
      
   /* allow the boot processor to perform one-time system init and pre-flight checks */
   if(flags & INT_IAMBSP)
   {
      unsigned int addition_loop;
            
      /* calculate average CPU bus speed and, thus, the local APIC's timer period */
      LAPIC_DEBUG("[lapic:%i] measuring APIC timer in pre-flight checks...\n", CPU_ID);
      
      /* attach the preflight timer handler to IRQ line. */
      irq_register_driver(IRQ_APIC_LINT0, IRQ_DRIVER_FUNCTION, 0, &lapic_preflight_timer);
      irq_register_driver(IOAPIC_VECTOR_BASE, IRQ_DRIVER_FUNCTION, 0, &lapic_preflight_timer);
      
      /* set the old-world timer to fire every at the rate expcted by the scheduler
       and see how far the CPU's APIC counts down in those periods */
      lapic_write(LAPIC_TIMERDIV, LAPIC_DIV_128); /* divide down the bus clock by 128 */
      lapic_write(LAPIC_TIMERINIT, 0xffffffff);
      x86_timer_init(SCHED_FREQUENCY);
      x86_enable_interrupts();

      /* loop until all done - don't optimise it out */
      while(lapic_preflight_timer_pass < APIC_TIMER_PASSES) __asm__ __volatile__("pause");
      
      /* tear down this preflight check and ensure the timer is disabled */
      x86_disable_interrupts();
      x86_timer_init(0);
      irq_deregister_driver(IRQ_APIC_LINT0, IRQ_DRIVER_FUNCTION, 0, &lapic_preflight_timer);
      irq_deregister_driver(IOAPIC_VECTOR_BASE, IRQ_DRIVER_FUNCTION, 0, &lapic_preflight_timer);
      
      /* calculate the average init value for the apic timer - each value has
         already been divided by APIC_TIMER_PASSES */
       for(addition_loop = 0; addition_loop < APIC_TIMER_PASSES; addition_loop++)
         lapic_preflight_timer_init += lapic_preflight_timer_lap[addition_loop];

      lapic_preflight_timer_init = 0xffffffff - lapic_preflight_timer_init;
      
      /* register the LAPIC timer driver */
      irq_register_driver(IRQ_APIC_TIMER, IRQ_DRIVER_FUNCTION | IRQ_DRIVER_LAST, 0, &int_common_timer);
   }
   
   LAPIC_DEBUG("[lapic:%i] programming APIC timer with reload value of %x\n", CPU_ID, lapic_preflight_timer_init);

   /* sanity check this */
   if(lapic_preflight_timer_init > 0xffffff00)
      debug_panic("lAPIC timer measurement is not sane");
   
   /* program the apic timer to fire every so many ticks */
   lapic_write(LAPIC_LVT_TIMER, IRQ_APIC_TIMER | LAPIC_TIMER_TP);
   lapic_write(LAPIC_TIMERDIV,  LAPIC_DIV_128); /* divide down the bus clock by 128 */
   lapic_write(LAPIC_TIMERINIT, lapic_preflight_timer_init);
}

/* lapic_is_present
   Use Intel's cpuid instruction to look up the presence of a local APIC.
   <= non-zero if a local APIC is present with this core, 0 for not found
*/
unsigned char lapic_is_present(void)
{
   unsigned int eax, ebx, ecx, edx;
   x86_cpuid(X86_CPUID_FEATURES, eax, ebx, ecx, edx);
   return edx & X86_CPUID_EDX_LAPIC;
}
