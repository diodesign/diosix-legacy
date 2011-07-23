/* kernel/ports/i386/include/lowevel.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _LOWLEVEL_H
#define   _LOWLEVEL_H

/* define pointer dereferencer if needed */
#ifndef NULL
#define NULL      (0)
#endif

/* Programmable interval timer IO ports */
#define X86_PIT_CHANNEL0      (0x40)
#define X86_PIT_CHANNEL1      (0x41)
#define X86_PIT_CHANNEL2      (0x42)
#define X86_PIT_COMMAND       (0x43)

/* Programmable interrupt controller IO ports */
#define PIC1               (0x20)   /* IO base address for master PIC */
#define PIC2               (0xA0)   /* IO base address for slave PIC */
#define PIC1_COMMAND       (PIC1)
#define PIC1_DATA          (PIC1+1)
#define PIC2_COMMAND       (PIC2)
#define PIC2_DATA          (PIC2+1)

#define ICW1_ICW4          (0x01)   /* ICW4 (not) needed */
#define ICW1_SINGLE        (0x02)   /* Single (cascade) mode */
#define ICW1_INTERVAL4     (0x04)   /* Call address interval 4 (8) */
#define ICW1_LEVEL         (0x08)   /* Level triggered (edge) mode */
#define ICW1_INIT          (0x10)   /* Initialization - required! */

#define ICW4_8086          (0x01)   /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO          (0x02)   /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE     (0x08)   /* Buffered mode/slave */
#define ICW4_BUF_MASTER    (0x0C)   /* Buffered mode/master */
#define ICW4_SFNM          (0x10)   /* Special fully nested (not) */

/* serial port handling */
void serial_writebyte(unsigned char c);
void serial_initialise(void);

/* low level */
#define X86_CMOS_RESET_BYTE   (0xf0)
#define X86_CMOS_RESET_WARM   (0x0a)
#define X86_CMOS_RESET_COLD   (0x00)
#define X86_CMOS_ADDR_PORT    (0x70)
#define X86_CMOS_DATA_PORT    (0x71)

/* IO port access */
#define X86_IOPORT_MAXWORDS   (2048) /* number of 32bit words in (2^16)-bit IO port access bitmap */
#define X86_IOPORT_BITMAPSIZE (X86_IOPORT_MAXWORDS * sizeof(unsigned int))

/* probe the CPU for features, return data in eax,ebx,ecx,edx */
#define x86_cpuid(func,ax,bx,cx,dx) \
   __asm__ __volatile__ ("cpuid" : "=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) : "a" (func));

unsigned x86_inportb(unsigned short port);
void x86_outportb(unsigned port, unsigned val);
kresult x86_ioports_clear_all(process *p);
kresult x86_ioports_clear(process *p, unsigned int index, unsigned int value);
kresult x86_ioports_clone(process *target, process *source);
kresult x86_ioports_new(process *p);
kresult x86_ioports_enable(thread *t);
kresult x86_ioports_disable(thread *t);
kresult x86_ioports_check(process *p, unsigned short port);
void x86_cmos_write(unsigned char addr, unsigned char value);
void x86_load_cr3(void *ptr);
unsigned int x86_read_cr2(void);
void x86_load_idtr(unsigned int *ptr); /* defined in start.s */
void x86_load_tss(void); /* defined in start.s */
void x86_load_gdtr(unsigned int ptr); /* defined in start.s */
void x86_timer_init(unsigned char freq);
void x86_enable_interrupts(void);
void x86_disable_interrupts(void);
#define lowlevel_disable_interrupts x86_disable_interrupts
#define lowlevel_enable_interrupts x86_enable_interrupts
void x86_change_tss(gdtptr_descr *cpugdt, gdt_entry *gdt, tss_descr *tss, unsigned char flags);
kresult x86_init_tss(thread *toinit);
void x86_start_ap(void);
void x86_start_ap_end(void);
unsigned long long x86_read_cyclecount(void);
void lowlevel_thread_switch(thread *now, thread *next, int_registers_block *regs);
void lowlevel_proc_preinit(void);
void lowlevel_stacktrace(void);
void lowlevel_kickstart(void);
void lowlevel_ioports_clone(process *new, process *current);
void lowlevel_ioports_new(process *new);
unsigned int x86_test_and_set(unsigned int value, volatile unsigned int *lock); /* defined in start.s */

#endif
