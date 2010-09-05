/* kernel/ports/i386/hw/ioapic.c
 * i386-specific IOAPIC chip support
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

/* a messy fixed table of IOAPIC info */
chip_ioapic ioapics[MAX_IOAPICS];

/* hardware interrupts for the system's IOAPICs */
extern void irq23(); extern void irq24(); extern void irq25(); extern void irq26();
extern void irq27(); extern void irq28(); extern void irq29(); extern void irq30();
extern void irq31(); extern void irq32(); extern void irq33(); extern void irq34();
extern void irq35(); extern void irq36(); extern void irq37(); extern void irq38();
extern void irq39(); extern void irq40(); extern void irq41(); extern void irq42();
extern void irq43(); extern void irq44(); extern void irq45(); extern void irq46();

/* default handler for IOAPIC IRQs - just EOI the interrupt */
kresult ioapic_irq_default(unsigned char intnum, int_registers_block *regs)
{
	/* the local APIC generated this interrupt, so satisfy it */
	lapic_end_interrupt();
	return success;
}

/* int_ioapic_read
 Read from an IOAPIC register
 => id = system id of IOAPIC to program
    reg = IOAPIC register number to write to
 <= the 32bit word in the selected register
*/
unsigned int ioapic_read(unsigned char id, unsigned char reg)
{
	unsigned int addr_reg = IOAPIC_REG_SELECT + (id * 0x100);
	unsigned int addr_val = IOAPIC_WINDOW_SELECT + (id * 0x100);
	
	/* write the target register into the select register */
	*((volatile unsigned int *)(addr_reg)) = reg;
	
	/* ..then read the data */
	return *((volatile unsigned int *)(addr_val));
}


/* int_ioapic_write
   Write to an IOAPIC register
   => id = system id of IOAPIC to program
      reg = IOAPIC register number to write to
      value = the 32bit word to write in
   <= assumes success
*/
void ioapic_write(unsigned char id, unsigned char reg, unsigned int value)
{
	unsigned int addr_reg = IOAPIC_REG_SELECT + (id * 0x100);
	unsigned int addr_val = IOAPIC_WINDOW_SELECT + (id * 0x100);
	
	/* write the target register into the select register */
	*((volatile unsigned int *)(addr_reg)) = reg;
	
	/* ..then the data */
	*((volatile unsigned int *)(addr_val)) = value;
}

/* ioapic_route_set
	Set the interrupt routing on an IOAPIC by pointing an IRQ line at a
   particular CPU's APIC
   => id = system id of IOAPIC to program
      entry = IRQ line on the IOAPIC
      cpu = system id of CPU to route the interrupt to
      flags = flags to set in the IO redirection table
   <= assumes success
*/
void ioapic_route_set(unsigned char id, unsigned char entry, unsigned char cpu, unsigned int flags)
{
	IOAPIC_DEBUG("[ioapic:%i] routing IOAPIC %i IRQ %i to CPU %i (flags %x)\n",
			  CPU_ID, id, entry, cpu, flags);
	
	/* the chip's IO redirection table is 64 bits wide:
	   the target cpu is in bits 63:56, the flags in bits 16:0 */
	ioapic_write(id, IOAPIC_REDIR_BASE + (entry * 2) + 0, flags);
	ioapic_write(id, IOAPIC_REDIR_BASE + (entry * 2) + 1, cpu << 24);
}

/* int_initialise_ioapic
	Set up an IOAPIC on the system
   => id = ID of IOAPIC to initialise
*/
void ioapic_initialise(unsigned char id)
{		
	unsigned int this_cpu = CPU_ID, loop;
	
	IOAPIC_DEBUG("[ioapic:%i] initialising chip %i\n", id);
	
	/* ensure any IOAPICs present are enabled and in SMP mode. From the Intel manaual:
	 The IMCR is supported by two read/writable or write-only I/O ports, 22h and
	 23h, which receive address and data respectively. To access the IMCR, write a
	 value of 70h to I/O port 22h, which selects the IMCR. Then write the data to
	 I/O port 23h. The power-on default value is zero, which connects the NMI and
	 8259 INTR lines directly to the BSP. Write a value of 1 to route 8259/PIT
	 interrupts via the IOAPIC */
	x86_outportb(0x22, 0x70); /* begin IMCR access */
	x86_outportb(0x23, 1);    /* set bit 1 for SMP mode */
	
	/* point the IOAPIC interrupt lines at specific reserved vectors and route
		to the boot processor for now */
	int_set_gate(IOAPIC_VECTOR_BASE + 0, (unsigned int)irq23, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 1, (unsigned int)irq24, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 2, (unsigned int)irq25, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 3, (unsigned int)irq26, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 4, (unsigned int)irq27, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 5, (unsigned int)irq28, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 6, (unsigned int)irq29, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 7, (unsigned int)irq30, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 8, (unsigned int)irq31, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 9, (unsigned int)irq32, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 10, (unsigned int)irq33, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 11, (unsigned int)irq34, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 12, (unsigned int)irq35, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 13, (unsigned int)irq36, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 14, (unsigned int)irq37, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 15, (unsigned int)irq38, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 16, (unsigned int)irq39, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 17, (unsigned int)irq40, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 18, (unsigned int)irq41, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 19, (unsigned int)irq42, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 20, (unsigned int)irq43, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 21, (unsigned int)irq44, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 22, (unsigned int)irq45, 0x18, 0x8E, 0);
	int_set_gate(IOAPIC_VECTOR_BASE + 23, (unsigned int)irq46, 0x18, 0x8E, 1); /* reload IDT */

	/* register the default handlers */
	for(loop = 0; loop < 24; loop++)
		irq_register_driver(IOAPIC_VECTOR_BASE + loop, IRQ_DRIVER_FUNCTION, 0, &ioapic_irq_default);
		
	/* route the IRQ lines to our boot cpu */
	for(loop = 0; loop < 24; loop++)
		ioapic_route_set(id, loop, this_cpu, (IOAPIC_VECTOR_BASE + loop));
}
