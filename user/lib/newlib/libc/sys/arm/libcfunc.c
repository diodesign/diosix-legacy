/* user/lib/newlib/libc/sys/arm/libcfunc.c
 * Aux C functions for the ARM port of Newlib 1.18.0's libc
 * Author : Chris Williams
 * Date   : Fri,29 Apr 2011.15:00:00
 
 Copyright (c) Chris Williams and individual contributors
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
 
*/

/* diosix_atomic_exchange
   Acquire lock word pointed to by ptr in a thread-safe manner. This will 
   set the word to 1 and return its previous value atomically.
   => ptr = pointer to 32bit lock variable
   <= 1 for lock held by some other code, or 0 for lock acquired
*/
unsigned int diosix_atomic_exchange(volatile unsigned int *ptr)
{
   unsigned int old_val;

   /* this code is valid only for non-SMP ARMv5 processors! */
   __asm__ __volatile__("mov r1, #1; mov r2, %1; swpb r0, r1, [r2]; mov %0, r0" : "=r" (old_val) : "r" (ptr));
   
   return old_val;
}
