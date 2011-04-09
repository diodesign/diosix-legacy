/* kernel/ports/arm/versatilepb/serial.c
 * ARM-specific low-level UART serial port routines for the Versatile PB dev board
 * Author : Chris Williams
 * Date   : Sat,12 Mar 2011.00:29:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* address of the first UART's data register */
#define UART0DR   KERNEL_PHYS2LOG(0x101f1000)

/* serial_writeline
 Write a string out out to the serial port
 => s = NULL-terminated string to output
 */
void serial_writeline(const unsigned char *s)
{
   if(!s) return;
   
   while(*s)
      *(volatile unsigned int *)UART0DR = (unsigned int)*s++; /* bang out char */
}

/* serial_writebyte
   Write a byte out out to the serial port
   => c = character to output
*/
void serial_writebyte(unsigned char c)
{
   *(volatile unsigned int *)UART0DR = (unsigned int)c; /* bang out char */
}

/* serial_initialise
   Start up the serial debugging output */
void serial_initialise(void)
{
   /* do nothing for the moment */
}
