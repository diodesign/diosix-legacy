/* user/sbin/init/init.c
 * First process to run; spawn system managers and boot system
 * Author : Chris Williams
 * Date   : Wed,21 Oct 2009.10:37:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <unistd.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"

/* process_kernel_signals
   Handle incoming signals from the kernel
*/
void process_signals(void)
{
   diosix_msg_info msg;
   
   /* accept signals telling us of process teardowns */
   diosix_signals_kernel(SIGX_ENABLE(SIGXPROCKILLED));
   diosix_signals_kernel(SIGX_ENABLE(SIGXPROCEXIT));
   
   /* prepare to sleep until an interrupt comes in */
   msg.role = msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_KERNELONLY;
   
   while(1)
      /* block until a signal comes in from the kernel */
      if(diosix_msg_receive(&msg) == success)
      {
         switch(msg.signal.number)
         {
            case SIGXPROCEXIT:
            case SIGXPROCKILLED:
               diosix_debug_write("process needs killing\n");
               diosix_kill(msg.signal.number);
               break;
               
            default:
               break;
         }
      }
}

int main(void)
{   
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_SYSTEM_EXECUTIVE);

   if(diosix_thread_fork() > 0) process_kernel_signals();
   
   while(1); /* idle */
}
