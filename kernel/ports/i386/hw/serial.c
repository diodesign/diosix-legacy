/* kernel/ports/i386/hw/serial.c
 * i386-specific run-time debug output to the serial console
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

 Apologies for the magic numbers in this code. It is a quick and simple way
 to get debugging info out of the kernel when there's no other sane way.
 For more information on PC serial ports:
  http://www.nondot.org/sabre/os/files/Communication/ser_port.txt

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* location of first serial port in x86 pc hardware world */
#define SERIAL_HW   (0x3f8)

/* serial_writebyte
   Write a byte out out to the serial port
   => c = character to output
*/
void serial_writebyte(unsigned char c)
{
   /* loop waiting for bit 5 of the line status register to set, indicating
      data can be written */
   while((x86_inportb(SERIAL_HW + 5) & 0x20) == 0) __asm__ __volatile("pause");
   x86_outportb(SERIAL_HW + 0, c);
}

/* serial_initialise
   Start up the serial debugging output */
void serial_initialise(void)
{
   /* set baud, etc */
   x86_outportb(SERIAL_HW + 3, 0x80);
   x86_outportb(SERIAL_HW + 0, 0x30);
   x86_outportb(SERIAL_HW + 1, 0x00);
   x86_outportb(SERIAL_HW + 3, 0x03);
   x86_outportb(SERIAL_HW + 4, 0x00);
   x86_inportb(SERIAL_HW);
}
