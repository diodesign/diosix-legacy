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

#include <diosix.h>
#include <functions.h>

void do_listen(void)
{
   unsigned int buffer;
   diosix_msg_info msg;
   kresult err;
   
   /* listen and reply */
   while(1)
   {
      /* accept any message from any thread/process */
      msg.tid = DIOSIX_MSG_ANY_THREAD;
      msg.pid = DIOSIX_MSG_ANY_PROCESS;
      msg.flags = DIOSIX_MSG_ANY_TYPE;
      msg.recv = &buffer;
      msg.recv_max_size = sizeof(unsigned int);

      if(diosix_msg_receive(&msg) == success)
      {
         buffer++;

         msg.flags = DIOSIX_MSG_GENERIC;
         msg.send = &buffer;
         msg.send_size = sizeof(unsigned int);
         err = diosix_msg_reply(&msg);
      }
   }
}

void main(void)
{
   diosix_msg_info msg;
   unsigned int child;
   unsigned int message = 0;
   
   /* create new process to receive */
   child = diosix_fork();

   if(child == 0) do_listen(); /* child does the listening */
   
   /* send the message to the child */
   while(1)
   {
      /* set up message block */
      msg.tid = DIOSIX_MSG_ANY_THREAD;
      msg.pid = child;
      msg.flags = DIOSIX_MSG_GENERIC;
      msg.send = &message;
      msg.send_size = sizeof(unsigned int);
      msg.recv = &message;
      msg.recv_max_size = sizeof(unsigned int);
      
      /* send message any listening thread, block if successfully
         foudn a receiver */ 
      if(diosix_msg_send(&msg))
         diosix_thread_yield();
   }
}
