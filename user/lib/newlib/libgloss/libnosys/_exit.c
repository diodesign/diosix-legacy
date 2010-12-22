/* user/lib/newlib/libgloss/libnosys/_exit.c
 * portable interface of the _exit() syscall between libc and the diosix microkernel
 * Author : Chris Williams
 * Date   : Wed,22 Dec 2010.01:27:00
 
 Copyright (c) Chris Williams and individual contributors
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
 
*/

/* libc definitions */
#include <limits.h>
#include "config.h"
#include <_ansi.h>
#include <_syslist.h>

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"

/* _exit()
   summary: end execution of the calling program
   reference: http://pubs.opengroup.org/onlinepubs/009695399/functions/exit.html */

_VOID
_DEFUN (_exit, (rc),
	int rc)
{
   /* call the kernel, keeping just the low byte of the return code */
   diosix_exit(rc & 0xff);
   
   /* does not return */
   while(1);
}
