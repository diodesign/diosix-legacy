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
      x86_pic_reset(2); /* reset the slave if necessary */
   x86_pic_reset(1); /* as well as the master */
   
   PIC_DEBUG("[pic:%i] default irq handler called: int %i\n", CPU_ID, intnum);
   
   return success;
}

/* initialise the common entries in the int table for uni and multiproc systems */
void pic_initialise(void)
{
   unsigned int loop;
   
   PIC_DEBUG("[pic:%i] initialising common x86 interrupt handlers... \n", CPU_ID);
   
   /* reprogram the basic PICs so that any old-world interrupts
    get sent to the 16 vectors above the cpu exceptions.
    Run the PIC vectors from 32 (0x20) to 47 (0x2F) */
   x86_pic_remap(PIC_MASTER_VECTOR_BASE, PIC_SLAVE_VECTOR_BASE);
   
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
