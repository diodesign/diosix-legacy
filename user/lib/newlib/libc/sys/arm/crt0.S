/* user/bin/lib/newlib/libc/sys/arm/crt0.S
 * Start-up code for ARM EABI executables linked to run on diosix
 * Author : Chris Williams
 * Date   : Fri,29 Apr 2011.15:10:00
 
 Copyright (c) Chris Williams and individual contributors
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
 
*/

   .text
	.code 32
	.align 	0

	.global _start

/* entry conditions from the microkernel
   => r13 = pointer to thread's initial stack
*/
_start:
	bl	main
	bl	exit
   
   /* shouldn't return */
   b .
