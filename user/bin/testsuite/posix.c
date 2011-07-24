/* user/bin/testsuite/posix.c
 * Testsuite of the kernel's POSIX-compatible parts of its API
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

/* <= success if diosix_fork() returns child PID to the parent */
kresult test__diosix_fork_parent(void)
{
   int child = diosix_fork();
    
   if(child == -1) return e_failure;
   if(child > 0) return success;
   
   /* kill off the child or park it here if the exit syscall is busted */
   diosix_exit(0);
   while(1);
}

/* <= success if diosix_fork() returns 0 for the child */
kresult test__diosix_fork_child(void)
{
   int child = diosix_fork();
   
   if(child == -1) return e_failure;
   if(child == 0) return success;
   
   /* kill off the parent or park it here if the exit syscall is busted */
   diosix_exit(0);
   while(1);
}

/* TEST: test_diosix_fork
   Demonstrate that a process can create a child process
   and get the child's PID.
*/
kresult test__diosix_fork(void)
{
   kresult parent, child;
   
   /* test the parent can get the child's PID */
   parent = test__diosix_fork_parent();
   if(parent != success) return e_failure;
   
   /* test the child can run */
   child = test__diosix_fork_child();
   if(child != success) return e_failure;
      
   /* fall through to success */
   return success;
}
