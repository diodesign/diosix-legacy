/* user/bin/testsuite/msg.c
 * Testsuite of the message passsing interface
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

/* TEST: test__msg_send
   Test that a process can fork, receive a message from a child and reply.
*/
kresult test__msg_send(void)
{
   int child_pid;
   unsigned int data = 0;
   diosix_msg_info msg;

   child_pid = diosix_fork();
   if(child_pid == -1) return e_failure;
   
   memset(&msg, 0, sizeof(diosix_msg_info));
   
   if(child_pid == 0)
   {
      /* we're the child so send a message to the parent */
      diosix_process_info proc_info;
      
      /* find out the parent's PID */
      diosix_get_process_info(&proc_info);
      
      while(data < 10)
      {
         /* craft a simple message to the parent */
         msg.pid = proc_info.parentpid;
         msg.tid   = DIOSIX_MSG_ANY_THREAD;
         msg.flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_QUEUEME | DIOSIX_MSG_SENDASUSR;
         msg.send  = &data;
         msg.send_size = sizeof(data);
         msg.recv  = &data;
         msg.recv_max_size = sizeof(data);
         
         if(diosix_msg_send(&msg) != success) diosix_exit(1);
      }
      
      diosix_exit(0);
   }
   
   /* we're the parent so wait to receive a message */
   while(data < 10)
   {
      msg.flags = DIOSIX_MSG_GENERIC;
      msg.recv = &data;
      msg.recv_max_size = sizeof(data);
      
      if(diosix_msg_receive(&msg) != success) return e_failure;
      
      /* incrememt the variable and ping it back as a reply to unlock the child */
      data++;
      msg.send = &data;
      msg.send_size = sizeof(data);
      diosix_msg_reply(&msg);
   }
   
   return success;
}
