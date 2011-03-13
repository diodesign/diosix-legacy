/* kernel/ports/arm/boot.c
 * ARM-specific low-level start-up routines
 * Author : Chris Williams
 * Date   : Fri,11 Mar 2011.16:54:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* welcome to Earth */
int main()
{
   unsigned int x, y, z = 0;
   
   if(DEBUG) debug_initialise();
   dprintf("[core] %s rev %s" " " __TIME__ " " __DATE__ " (built with GCC " __VERSION__ ")\n", KERNEL_IDENTIFIER, SVN_REV);
   
   *(volatile unsigned int *)(0x1000001C) = 0x2CAC; /* timing magic */
   *(volatile unsigned int *)(0x10120000) = 0x1313A4C4;
   *(volatile unsigned int *)(0x10120004) = 0x0505F657;
   *(volatile unsigned int *)(0x10120008) = 0x071F1800;
   *(volatile unsigned int *)(0x10120010) = (1 * 1024 * 1024); /* base addr of frame buffer */
   *(volatile unsigned int *)(0x10120018) = 0x82b; /* control bits */
   
   while(1)
   {
      for(x = 0; x < 800; x++)
         for(y = 0; y < 600; y++)
            *(volatile unsigned int *)((1 * 1024 * 1024) + 4 * (x + (y * 600))) = ((x % 256) | ((y % 256) << 8) | ((z % 256) << 16));
      z += 4;
   }
   
   return 0;
}
