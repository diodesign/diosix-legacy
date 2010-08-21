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
#define	_LOWLEVEL_H

/* define pointer dereferencer if needed */
#ifndef NULL
#define NULL      (0)
#endif

/* serial port handling */
void serial_writebyte(unsigned char c);
void serial_initialise(void);

/* low level */
#define X86_CMOS_RESET_BYTE	(0xf0)
#define X86_CMOS_RESET_WARM	(0x0a)
#define X86_CMOS_RESET_COLD	(0x00)
#define X86_CMOS_ADDR_PORT		(0x70)
#define X86_CMOS_DATA_PORT		(0x71)

unsigned x86_inportb(unsigned short port);
void x86_outportb(unsigned port, unsigned val);
void x86_cmos_write(unsigned char addr, unsigned char value);
void x86_pic_remap(unsigned int offset1, unsigned int offset2);
void x86_pic_reset(unsigned char pic);
void x86_load_cr3(void *ptr);
unsigned int x86_read_cr2(void);
void x86_load_idtr(unsigned int *ptr); /* defined in start.s */
void x86_load_tss(void); /* defined in start.s */
void x86_load_gdtr(unsigned int ptr); /* defined in start.s */
void x86_timer_init(unsigned char freq);
void x86_enable_interrupts(void);
void x86_disable_interrupts(void);
void x86_change_tss(gdtptr_descr *cpugdt, gdt_entry *gdt, tss_descr *tss);
void x86_init_tss(thread *toinit);
void x86_start_ap(void);
void x86_start_ap_end(void);
unsigned long long x86_read_cyclecount(void);
void lowlevel_thread_switch(thread *now, thread *next, int_registers_block *regs);
void lowlevel_proc_preinit(void);
void lowlevel_kickstart(void);
unsigned int x86_test_and_set(unsigned int value, volatile unsigned int *lock); /* defined in start.s */

#endif
