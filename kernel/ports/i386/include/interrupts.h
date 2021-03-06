/* kernel/ports/i386/include/portdefs.h
 * prototypes and structures for the i386 port of the kernel 
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

/* APIC register locations */
#define LAPIC_BASE           (0xFEE00000)
#define LAPIC_ID_REG         ((volatile unsigned int *)(LAPIC_BASE + 0x020))
#define LAPIC_VER_REG        ((volatile unsigned int *)(LAPIC_BASE + 0x030))
#define LAPIC_TASKPRI        ((volatile unsigned int *)(LAPIC_BASE + 0x080))
#define LAPIC_PROCPRI        ((volatile unsigned int *)(LAPIC_BASE + 0x0a0))
#define LAPIC_EOI            ((volatile unsigned int *)(LAPIC_BASE + 0x0b0))
#define LAPIC_DESTID         ((volatile unsigned int *)(LAPIC_BASE + 0x0d0))
#define LAPIC_DESTFMT        ((volatile unsigned int *)(LAPIC_BASE + 0x0e0))
#define LAPIC_SPURIOUS_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x0f0))
#define LAPIC_INSERVC0_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x100))
#define LAPIC_INSERVC1_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x110))
#define LAPIC_INSERVC2_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x120))
#define LAPIC_INSERVC3_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x130))
#define LAPIC_INSERVC4_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x140))
#define LAPIC_INSERVC5_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x150))
#define LAPIC_INSERVC6_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x160))
#define LAPIC_INSERVC7_REG   ((volatile unsigned int *)(LAPIC_BASE + 0x170))
#define LAPIC_INTREQ0_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x200))
#define LAPIC_INTREQ1_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x210))
#define LAPIC_INTREQ2_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x220))
#define LAPIC_INTREQ3_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x230))
#define LAPIC_INTREQ4_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x240))
#define LAPIC_INTREQ5_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x250))
#define LAPIC_INTREQ6_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x260))
#define LAPIC_INTREQ7_REG    ((volatile unsigned int *)(LAPIC_BASE + 0x270))
#define LAPIC_ERRSTATUS_REG  ((volatile unsigned int *)(LAPIC_BASE + 0x280))
#define LAPIC_ICR_LO         ((volatile unsigned int *)(LAPIC_BASE + 0x300))
#define LAPIC_ICR_HI         ((volatile unsigned int *)(LAPIC_BASE + 0x310))
#define LAPIC_LVT_TIMER      ((volatile unsigned int *)(LAPIC_BASE + 0x320))
#define LAPIC_LVT_THERMAL    ((volatile unsigned int *)(LAPIC_BASE + 0x330))
#define LAPIC_LVT_PCOUNTER   ((volatile unsigned int *)(LAPIC_BASE + 0x340))
#define LAPIC_LVT_LINT0      ((volatile unsigned int *)(LAPIC_BASE + 0x350))
#define LAPIC_LVT_LINT1      ((volatile unsigned int *)(LAPIC_BASE + 0x360))
#define LAPIC_LVT_ERROR      ((volatile unsigned int *)(LAPIC_BASE + 0x370))
#define LAPIC_TIMERINIT      ((volatile unsigned int *)(LAPIC_BASE + 0x380))
#define LAPIC_TIMERNOW       ((volatile unsigned int *)(LAPIC_BASE + 0x390))
#define LAPIC_TIMERDIV       ((volatile unsigned int *)(LAPIC_BASE + 0x3e0))
         
/* APIC register bits */
#define LAPIC_ENABLE       (1 << 8)    /* in the spurious int vect reg */
#define LAPIC_LVT_MASK     (1 << 16)   /* in the timer/lint0/etc lvt regs */
#define LAPIC_LVT_NMI      (1 << 10)   /* in the timer/lint0/etc lvt regs */
#define LAPIC_TIMER_TP     (1 << 17)   /* in the timer lvt reg */
#define LAPIC_DIV_128      ((1 << 3) | (1 << 1)) /* in the timer divider reg */
#define LAPIC_DIV_1        ((1 << 3) | (1 << 1) | (1 << 0)) /* in the timer divider reg */
#define LAPIC_DEST_SHIFT   (24) /* in the ICR_HI reg */
#define LAPIC_PENDING      (1 << 12)  /* in the ICR_LO reg */
#define LAPIC_TYPE_INIT    ((1 << 10) | (1 << 8)) /* in the ICR_LO reg */
#define LAPIC_TYPE_START   ((1 << 10) | (1 << 9)) /* in the ICR_LO reg */
#define LAPIC_ASSERT       (1 << 14)  /* in the ICR_LO reg */
#define LAPIC_TRG_LEVEL    (1 << 15)  /* in the ICR_LO reg */
#define LAPIC_ALLINCSEL    (1 << 19)  /* in the ICR_LO reg */
#define LAPIC_ALLBUTSEL    ((1 << 19) | (1 << 18)) /* in the ICR_LO reg */

/* IOAPIC register locations */
#define IOAPIC_REG_SELECT    (0xfec00000)
#define IOAPIC_WINDOW_SELECT (0xfec00010)
#define IOAPIC_REDIR_BASE    (0x10)

/* IOAPIC register flags */
#define IOAPIC_MASK          (1 << 16)

/* interrupt support */
#define IRQ_DRIVER_PROCESS  (1 << 0)   /* this driver is a specific userland thread */
#define IRQ_DRIVER_FUNCTION (1 << 1)   /* this driver is an in-kernel function */
#define IRQ_DRIVER_TYPEMASK (IRQ_DRIVER_FUNCTION | IRQ_DRIVER_PROCESS)

#define IRQ_DRIVER_IGNORE   (1 << 2)   /* this driver is not interested at the moment */
#define IRQ_DRIVER_LAST     (1 << 3)   /* run this driver after the default handler */

#define IRQ_MAX_LINES        (256) /* maximum of 256 (0-255) IRQ lines */
#define MAX_IOAPICS          (4) /* maximum of 4 IOAPICs per system */

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

/* export the irq lock to the thread switching code */
extern rw_gate irq_lock;

/* interrupt-related functions */
kresult int_initialise(void);
void int_initialise_common(void);
kresult int_common_timer(unsigned char intnum, int_registers_block *regs);
void int_reload_idtr(void);
void int_set_gate(unsigned char intnum, unsigned int base, unsigned short segment, unsigned char flags, unsigned char flush);
void exceptions_initialise(void);
void irq_initialise(void);
void ioapic_initialise(unsigned char id);
void lapic_initialise(unsigned char flags);

void pic_initialise(void);
void pic_remap(unsigned int offset1, unsigned int offset2);
void pic_reset(unsigned char pic);
void pic_mask_disable(unsigned char irq);
void pic_mask_enable(unsigned char irq);
signed char pic_discover_irq(void);

unsigned char lapic_is_present(void);
void lapic_write(volatile unsigned int *addr, unsigned int value);
void lapic_end_interrupt(void);
void lapic_ipi_send(unsigned char destination, unsigned char vector);
void lapic_ipi_broadcast(unsigned char vector);
void lapic_ipi_send_startup(unsigned char destination, unsigned char vector);
void lapic_ipi_send_init(unsigned char destination);
unsigned int ioapic_read(unsigned char id, unsigned char reg);
void ioapic_write(unsigned char id, unsigned char reg, unsigned int value);
kresult ioapic_register_chip(unsigned int id, unsigned int physaddr);
kresult irq_register_driver(unsigned int irq_num, unsigned int flags, process *proc, kresult (*func)(unsigned char intnum, int_registers_block *regs));
kresult irq_deregister_driver(unsigned int irq_num, unsigned int type, process *proc, kresult (*func)(unsigned char intnum, int_registers_block *regs));

#define int_enable   x86_enable_interrupts
#define int_disable  x86_disable_interrupts

/* interrupt numbers and flags */
#define INT_UNDEFINSTR     (6)
#define INT_FPTRAP         (7)
#define INT_DOUBLEF        (8)
#define INT_GPF            (13)
#define INT_PF             (14)

/* interprocessor interrupts */
#define INT_IPI_RESCHED    (142) /* pick a new running thread */
#define INT_IPI_FLUSHTLB   (143) /* reload page directory */

/* syscall interface */
#define INT_KERNEL_SWI     (144)

#define PIC_MASTER         (1)
#define PIC_SLAVE          (2)

#define PIC_MASTER_VECTOR_BASE   (32)
#define PIC_SLAVE_VECTOR_BASE    (PIC_MASTER_VECTOR_BASE + 8)
#define PIC_MAX_IRQS             (16)

#define APIC_TIMER_PASSES  (4)
#define APIC_VECTOR_BASE   (48)
#define IRQ_APIC_TIMER     (APIC_VECTOR_BASE + 0)
#define IRQ_APIC_LINT0     (APIC_VECTOR_BASE + 1)
#define IRQ_APIC_LINT1     (APIC_VECTOR_BASE + 2)
#define IRQ_APIC_PCINT     (APIC_VECTOR_BASE + 3)
#define IRQ_APIC_THERMAL   (APIC_VECTOR_BASE + 4)
#define IRQ_APIC_ERROR     (APIC_VECTOR_BASE + 5)
#define IRQ_APIC_SPURIOUS  (63) /* bits 0-3 must be set */

/* the 8254 PIT's PMT scheduler IRQ is wired via a PIC chip on a uniproc machine */
#define PIC_8254_IRQ_FIXED (0)
#define PIC_8254_IRQ       (PIC_MASTER_VECTOR_BASE + PIC_8254_IRQ_FIXED)

#define IOAPIC_VECTOR_BASE (64)
#define IOAPIC_8254_IRQ    (IOAPIC_VECTOR_BASE + 2) /* the PIT is tied to INTIN2 */

/* flag whether we're the boot processor or not */
#define INT_IAMBSP         (1)
#define INT_IAMAP          (0)

#endif
