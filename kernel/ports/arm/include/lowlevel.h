/* kernel/ports/arm/include/lowlevel.h
 * prototypes and structures for the ARM port of the kernel 
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

/* serial port handling */
void serial_writeline(const unsigned char *s);
void serial_writebyte(unsigned char c);
void serial_initialise(void);

/* i2c eeprom reading */
unsigned char eeprom_read(unsigned char bus_addr, unsigned int mem_addr);

void lowlevel_thread_switch(thread *now, thread *next, int_registers_block *regs);
void lowlevel_enable_interrupts(void);
void lowlevel_disable_interrupts(void);
void lowlevel_proc_preinit(void);
void lowlevel_stacktrace(void);
void lowlevel_kickstart(void);

#endif
