/* user/sbin/drivers/i386/common/lowlevel.c
 * Low-level x86-specific routines
 * Author : Chris Williams
 * Date   : Tue,7 Dec 2010.06:55:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"
#include "io.h"

/* read a byte from an IO port via the kernel */
unsigned char kernel_ioread_byte(unsigned short port)
{
   diosix_ioport_request req;
   req.type = ioport_read;
   req.size = sizeof(char);
   req.port = port;
   diosix_driver_iorequest(&req);
   return req.data_in;
}

/* write a byte to an IO port via the kernel */
void kernel_iowrite_byte(unsigned short port, unsigned char val)
{
   diosix_ioport_request req;
   req.type = ioport_write;
   req.size = sizeof(char);
   req.port = port;
   req.data_out = (unsigned int)val & 0xff;
   diosix_driver_iorequest(&req);
}

unsigned char read_port_byte(unsigned short port)
{
   unsigned char ret_val;
   
   __asm__ __volatile__("inb %1,%0"
                        : "=a"(ret_val)
                        : "dN"(port));
   return ret_val;
}

unsigned short read_port(unsigned short port)
{
   unsigned char ret_val;
   
   __asm__ __volatile__("in %1,%0"
                        : "=a"(ret_val)
                        : "dN"(port));
   return ret_val;
}

unsigned int read_port_word(unsigned short port)
{
   unsigned int ret_val;
   
   __asm__ __volatile__("inl %1,%0"
                        : "=a"(ret_val)
                        : "dN"(port));
   return ret_val;
}

void write_port_byte(unsigned short port, unsigned char val)
{
   __asm__ __volatile__("outb %0,%1"
                        :
                        : "a"(val), "dN"(port));
}


void write_port(unsigned short port, unsigned short val)
{
   __asm__ __volatile__("out %0,%1"
                        :
                        : "a"(val), "dN"(port));
}

void write_port_word(unsigned short port, unsigned int val)
{
   __asm__ __volatile__("outl %0,%1"
                        :
                        : "a"(val), "dN"(port));
}

/* lock_spin
   Atomically write 1 into the given byte and read the previous contents.
   If the contents is 1 then retry until the previous contents is 0
   This forms a primitive atomic lock
*/
void lock_spin(volatile unsigned char *byte)
{
   unsigned char i = 1;
   
   while(i)
   {
      __asm__ __volatile__("lock; xchgb %0, %1"
                           : "=a"(i), "=m"(*byte)
                           : "0"(i));
      
      /* don't hog the cpu, let another thread run if we're blocking */
      if(i) diosix_thread_yield();
   }
}

/* unlock_spin
   Set the given lock variable to 0
*/
void unlock_spin(volatile unsigned char *byte)
{
   unsigned char i = 0;
   __asm__ __volatile__("lock; xchgb %0, %1"
                        : "=a"(i), "=m"(*byte)
                        : "0"(i));
}
