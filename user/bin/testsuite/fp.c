/* user/bin/testsuite/fp.c
 * Testsuite of the kernel's ability to handle floating point numbers in userspace
 * Author : Chris Williams
 * Date   : Mon,9 May 2011.11:06:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

volatile char fp_child_flag;

/* TEST: test__fp_addition
   Test that a process can add up using floating-point while
   not affecting other processes.
*/
kresult test__fp_addition(void)
{
   int sibling;
   volatile float x = 0.00;
   
   fp_child_flag = 0;
   
   sibling = diosix_thread_fork();
   if(sibling == -1) return e_failure;
   
   if(sibling)
   {
      x = 1.01;

      /* spin the older sibling waiting for the worker thread to finish */
      while(!fp_child_flag) diosix_thread_yield();
   }
   else
   {
      /* get the younger sibling to do some FP */
      volatile float y = 0.00;
      
      while(y < 100.00)
      {
         y = y + 23.01;
         diosix_thread_yield();
      }
      
      /* signal we're done and clean up */
      fp_child_flag = 1;
      diosix_thread_exit(0);
   }
   
   /* check to see that this thread's FP context is untampered */
   if(x != 1.01) return e_failure;
   
   return success;
}
