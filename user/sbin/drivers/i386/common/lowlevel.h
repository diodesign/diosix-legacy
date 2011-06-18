/* user/sbin/drivers/i386/common/lowlevel.h
 * Function prototypes for common x86-specific driver routines
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.23:21:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _LOWLEVEL_H
#define _LOWLEVEL_H 1

/* x86-specific stuff */
unsigned char read_port_byte(unsigned short port);
unsigned short read_port(unsigned short port);
unsigned int read_port_word(unsigned short port);
void write_port_byte(unsigned short port, unsigned char val);
void write_port(unsigned short port, unsigned short val);
void write_port_word(unsigned short port, unsigned int val);

/* atomic operations */
void lock_spin(volatile unsigned char *byte);
void unlock_spin(volatile unsigned char *byte);

/* PIC IRQ lines are mapped into the system IDT from this base number */
#define X86_PIC_IRQ_BASE   (32)

#endif
