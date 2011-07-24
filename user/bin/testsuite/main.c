/* user/bin/testsuite/main.c
 * Testsuite of kernel and OS APIs
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
#include "async.h"
#include "roles.h"
#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

/* ------------------------------------------------------------------------
      list of tests to perform
   ------------------------------------------------------------------------ */

test_def test_list[] = { test__is_running, "testsuite running",
                         test__diosix_fork, "direct fork syscall",
                         test__fp_addition, "fp: addition",
                         NULL, "" }; /* last item */

/* ------------------------------------------------------------------------ */

/* TEST: test_is_running: Establish execution
   Simply prove that userland execution has been reached and that
   the debug output channel is working.
   Expected result: successully return
*/
kresult test__is_running(void)
{
   return success;
}

/* ------------------------------------------------------------------------ */

/* do_test
   Attempt to perform a test and write the output to the log
   => nr = test ID number
*/
void do_test(test_nr nr)
{
   kresult result;
   int (*test)(void);
   char buffer[LOG_MAX_LINE_LENGTH];
   
   /* locate the test and run it */
   test = test_list[nr].func;
   if(!test) return;
   result = test();
   
   if(result == success)
      snprintf(buffer, LOG_MAX_LINE_LENGTH, LOG "test%i OK # %s \n", nr, test_list[nr].comment);
   else
      snprintf(buffer, LOG_MAX_LINE_LENGTH, LOG "test%i FAIL # %s \n", nr, test_list[nr].comment);
   
   /* push result of test out to the kernel's debug channel */
   diosix_debug_write(buffer);
}

volatile unsigned char data = 0;

int main(void)
{
#if 0
   test_nr test_id = 0;
   
   while(test_list[test_id].func)
   {
      do_test(test_id);
      test_id++;
   }
#endif
   
   data = 1;
   
   diosix_debug_write("hello world!\n");
   
   diosix_fork();
   
   diosix_debug_write("process post-fork()\n");
   
   data = 0;
   
   diosix_debug_write("process still running\n");
      
   while(1); /* idle */
}

