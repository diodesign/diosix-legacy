/* user/sbin/drivers/i386/serial/main.c
 * Driver to read and write from the serial port
 * Author : Chris Williams
 * Date   : Sun,29 May 2011.06:11:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "lowlevel.h"

#define SERIAL_HW   (0x3f8)

/* serial_write_char
   Write a byte out out to the serial port
   => c = character to output
*/
void serial_write_char(unsigned char c)
{
   /* loop waiting for bit 5 of the line status register to set, indicating
    data can be written */
   while((read_port_byte(SERIAL_HW + 5) & 0x20) == 0) diosix_thread_yield();
   write_port_byte(SERIAL_HW + 0, c);
   
   if(c == '\n') serial_write_char('\r');
}

/* serial_read_char
   Wait until a character enters the serial port
   <= byte read
*/
unsigned char serial_read_char(void)
{
   /* loop waiting for bit 1 of the line status register is set, indicating
      data is ready to be read */
   while((read_port_byte(SERIAL_HW + 5) & 1) == 0) diosix_thread_yield();
   return read_port_byte(SERIAL_HW + 0);
}

/* serial_write_string
   Output the given string of characters to the serial port
   => str = null-terminated string to output
*/
void serial_write_string(unsigned char *str)
{
   while(*str) serial_write_char(*str++);
}

int main(void)
{
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   serial_write_string("welcome to diosix...\n");
   
   while(1)
   {
      serial_write_char(serial_read_char());
   }
}
