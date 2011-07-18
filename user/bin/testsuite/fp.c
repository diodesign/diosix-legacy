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

volatile kresult test__fp_addition_child_result;
volatile char test__fp_addition_child_flag = 0;

/* TEST: test__fp_addition
   Test that a process can add up using floating-point while
   not affecting other processes.
*/
kresult test__fp_addition(void)
{
   int child, parent_loop = 0;
   float control_var = 1.00, addition_var = 0.00;
   
   child = diosix_thread_fork();
   if(child == -1) return e_failure;
      
   if(child == 0)
   {
      /* get the child to do some addition */
      unsigned int loop;
      
      for(loop = 0; loop < 10; loop++)
      {
         addition_var += 1.23;
         diosix_thread_yield();
      }
      
      if(addition_var == (10 * 1.23))
         test__fp_addition_child_result = success;
      else
         test__fp_addition_child_result = e_failure;
      
      /* mark the child as done and kill (or park) the thread */
      test__fp_addition_child_flag = 1;
      diosix_thread_exit(0);
      while(1);
   }
   else
   {      
      /* get the parent to wait, crudely, on a flag */
      while(!test__fp_addition_child_flag)
      {
         control_var += 0.33;
         parent_loop++;
         diosix_thread_yield();
      }
   }
   
   /* the control variable must be preserved */
   if(control_var == (0.33 * parent_loop) &&
      test__fp_addition_child_result == success)
      return success;
   
   /* fall through to failure */
   return e_failure;
}
