/* kernel/ports/i386/hw/pic.c
 * i386-specific interrupt handling
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

/* hardware interrupts */
extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

/* the default PIC interrupt handler */
kresult pic_irq_default(unsigned char intnum, int_registers_block *regs)
{
   /* clear the int from the chip */
   if(intnum >= PIC_SLAVE_VECTOR_BASE)
      pic_reset(PIC_SLAVE); /* reset the slave if necessary */
   pic_reset(PIC_MASTER); /* as well as the master */
   
   PIC_DEBUG("[pic:%i] default irq handler called: int %i\n", CPU_ID, intnum);
   return success;
}

/* pic_remap
   Reinitialise the two motherboard PIC chips to reprogram basic
   interrupt routing to the kernel using an offset.
   => offset1 = vector offset for master PIC
                vectors on the master become offset1 to offset1 + 7
   => offset2 = same for slave PIC: offset2 to offset2 + 7
*/
void pic_remap(unsigned int offset1, unsigned int offset2)
{
   unsigned char a1, a2;
   
   /* save the masks */
   a1 = x86_inportb(PIC1_DATA);
   a2 = x86_inportb(PIC2_DATA);
   
   /* reinitialise the chipset */
   x86_outportb(PIC1_COMMAND, ICW1_INIT + ICW1_ICW4);
   x86_outportb(PIC2_COMMAND, ICW1_INIT + ICW1_ICW4);
   /* send the new offsets */
   x86_outportb(PIC1_DATA, offset1);
   x86_outportb(PIC2_DATA, offset2);
   /* complete the reinitialisation sequence */
   x86_outportb(PIC1_DATA, 4);
   x86_outportb(PIC2_DATA, 2);
   x86_outportb(PIC1_DATA, ICW4_8086);
   x86_outportb(PIC2_DATA, ICW4_8086);
   
   /* restore saved masks */
   x86_outportb(PIC1_DATA, a1);
   x86_outportb(PIC2_DATA, a2);
}

/* pic_reset
   Send an end-of-interrupt/reset signal to a PIC
   => pic = PIC_MASTER for master
            PIC_SLAVE for slave
*/
void pic_reset(unsigned char pic)
{
   if(pic == PIC_MASTER)
      x86_outportb(PIC1_COMMAND, 0x20);
   
   if(pic == PIC_SLAVE)
      x86_outportb(PIC2_COMMAND, 0x20);
}

/* pic_read_irq
   Read the given register from both PICs
   => reg = ICW3_READ_IRR = read their interrupt request registers
            ICW3_READ_ISR = read their interrupt in-service registers
   <= master PIC's data in the low byte, slave's in the high byte
*/
unsigned short pic_read_irq(unsigned char reg)
{
   x86_outportb(PIC1_COMMAND, reg);
   x86_outportb(PIC2_COMMAND, reg);
   return x86_inportb(PIC1_COMMAND) | (x86_inportb(PIC2_COMMAND) << 8);
}

/* pic_discover_irq
   Return the highest priority interrupt in-service in the PICs,
   ie: call this to find out which source is caused the PICs to fire an IRQ
   <= pending interrupt source requiring source (0-15) or -1 for no IRQ
*/
signed char pic_discover_irq(void)
{
   unsigned char loop;
   unsigned short isr = pic_read_irq(ICW3_READ_ISR);
      
   /* scan through the ISR register looking for set bits to indicate
      an IRQ has been delivered to the CPU */
   for(loop = 0; loop < PIC_MAX_IRQS; loop++)
   {
      if(isr & 1) return loop;
      isr = isr >> 1;
   }
   
   /* fall through to return no IRQ source found */
   return -1;
}


/* pic_mask_disable
   Mask out the given IRQ line to disable the PIC from processing it
*/
void pic_mask_disable(unsigned char irq)
{
   unsigned short port;
   unsigned char value;
   
   /* bail out if irq is bonkers, there's only 0-15 PIC IRQs */
   if(irq > 0x0f) return;
   
   /* select the right PIC, master or slave */
   if(irq < 8)
      port = PIC1_DATA;
   else
   {
      port = PIC2_DATA;
      irq -= 8;
   }

   /* read in the current mask value, set the bit to disable 
      and update the mask register */
   value = x86_inportb(port) | (1 << irq);
   x86_outportb(port, value);        
}

/* pic_mask_enable
   Mask in the given IRQ line to enable the PIC to process it
*/
void pic_mask_enable(unsigned char irq)
{
   unsigned short port;
   unsigned char value;
   
   /* bail out if irq is bonkers, there's only 0-15 PIC IRQs */
   if(irq > 0x0f) return;
   
   /* select the right PIC, master or slave */
   if(irq < 8)
      port = PIC1_DATA;
   else
   {
      port = PIC2_DATA;
      irq -= 8;
   }
   
   /* read the current mask value, clear the bit to enable the
      IRQ and updatet he mask register */
   value = x86_inportb(port) & ~(1 << irq);
   x86_outportb(port, value);        
}

/* initialise the common entries in the int table for uni and multiproc systems */
void pic_initialise(void)
{
   unsigned int loop;
   
   PIC_DEBUG("[pic:%i] initialising common x86 interrupt handlers... \n", CPU_ID);
   
   /* reprogram the basic PICs so that any old-world interrupts
    get sent to the 16 vectors above the cpu exceptions.
    Run the PIC vectors from 32 (0x20) to 47 (0x2F) */
   pic_remap(PIC_MASTER_VECTOR_BASE, PIC_SLAVE_VECTOR_BASE);
   
   /* ..and direct the old-world hardware interrupts at the right vectors */
   int_set_gate(PIC_MASTER_VECTOR_BASE + 0, (unsigned int)irq0,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 1, (unsigned int)irq1,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 2, (unsigned int)irq2,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 3, (unsigned int)irq3,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 4, (unsigned int)irq4,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 5, (unsigned int)irq5,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 6, (unsigned int)irq6,  0x18, 0x8E, 0);
   int_set_gate(PIC_MASTER_VECTOR_BASE + 7, (unsigned int)irq7,  0x18, 0x8E, 0);
   
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 0, (unsigned int)irq8,  0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 1, (unsigned int)irq9,  0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 2, (unsigned int)irq10, 0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 3, (unsigned int)irq11, 0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 4, (unsigned int)irq12, 0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 5, (unsigned int)irq13, 0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 6, (unsigned int)irq14, 0x18, 0x8E, 0);
   int_set_gate(PIC_SLAVE_VECTOR_BASE + 7, (unsigned int)irq15, 0x18, 0x8E, 1);
   
   /* install the default handler, which will ack interrupts */
   for(loop = 0; loop < 8; loop++)
      irq_register_driver(PIC_MASTER_VECTOR_BASE + loop, IRQ_DRIVER_FUNCTION, 0, &pic_irq_default);
   
   for(loop = 0; loop < 8; loop++)
      irq_register_driver(PIC_SLAVE_VECTOR_BASE + loop, IRQ_DRIVER_FUNCTION, 0, &pic_irq_default);
}
