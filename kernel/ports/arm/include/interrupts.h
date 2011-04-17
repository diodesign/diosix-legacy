/* kernel/ports/arm/include/portdefs.h
 * prototypes and structures for the ARM port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _INTS_H
#define   _INTS_H

/* interrupt support */
#define IRQ_DRIVER_PROCESS  (1 << 0)   /* this driver is a specific userland thread */
#define IRQ_DRIVER_FUNCTION (1 << 1)   /* this driver is an in-kernel function */
#define IRQ_DRIVER_TYPEMASK (IRQ_DRIVER_FUNCTION | IRQ_DRIVER_PROCESS)

#define IRQ_DRIVER_IGNORE   (1 << 2)   /* this driver is not interested at the moment */
#define IRQ_DRIVER_LAST     (1 << 3)   /* run this driver after the default handler */

#define IRQ_MAX_LINES        (256) /* maximum of 256 (0-255) IRQ lines */

/* linked list of registered irq drivers */
struct irq_driver_entry
{
   /* describe this irq entry */
   unsigned int flags;
   unsigned int irq_num;
   
   /* process to send signal to */
   process *proc;
   
   /* function to call in kernelspace */
   kresult (*func)(unsigned char intnum, int_registers_block *regs);
   
   /* linked list of entries attached to an IRQ */
   irq_driver_entry *previous, *next;
   
   irq_driver_entry *proc_previous, *proc_next;
};

/* interrupt-related functions */
kresult int_initialise(void);
void irq_initialise(void);
kresult irq_register_driver(unsigned int irq_num, unsigned int flags, process *proc, kresult (*func)(unsigned char intnum, int_registers_block *regs));
kresult irq_deregister_driver(unsigned int irq_num, unsigned int type, process *proc, kresult (*func)(unsigned char intnum, int_registers_block *regs));

#define int_enable   lowlevel_enable_interrupts
#define int_disable  lowlevel_disable_interrupts

/* interprocessor interrupts */
#define INT_IPI_RESCHED    (142) /* pick a new running thread */
#define INT_IPI_FLUSHTLB   (143) /* reload page directory */



#endif
