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
#include <sys/stat.h> 
#include <fcntl.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"

unsigned int init_pid; /* PID of this init process */

/* process_kernel_signals
   Handle incoming signals from the kernel
*/
void process_kernel_signals(void)
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
               /* don't kill yourself... */
               if(msg.signal.extra && msg.signal.extra != init_pid)
                  diosix_kill(msg.signal.extra);
               break;
               
            default:
               break;
         }
      }
}

int main(void)
{
   int kernel_signals_thread;
   int handle, count;
   unsigned char data[100];
   
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_SYSTEM_EXECUTIVE);

   init_pid = getpid();
      
   kernel_signals_thread = diosix_thread_fork();
   if(kernel_signals_thread == 0) process_kernel_signals();
   
   handle = open("/dev/ata/0", O_RDONLY);
   while(handle < 0)
   {
      printf("tried to open /dev/ata, got error code %i\n", handle);
      diosix_thread_sleep(100);
      handle = open("/dev/ata/0", O_RDONLY);
   }
   
   printf("opened ATA drive: %i\n", handle);
   count = read(handle, data, 100);
   while(count < 0)
   {
      diosix_thread_sleep(100);
      count = read(handle, data, 100);
   }
   
   printf("reading... count = %i data = %x %x %x %x\n",
          count, data[0], data[1], data[2], data[3]);
   
   while(1); /* idle */
}
