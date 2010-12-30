/* user/sbin/vfs/main.c
 * Core functions for the virtual filesystem process
 * Author : Chris Williams
 * Date   : Sun,19 Dec 2010.04:27:00

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

/* echo_keyboard
   Forward signals from the keyboard driver to the video driver */
void echo_keyboard(void)
{
   diosix_msg_info msg, smsg;
   
   /* set up the message block to send a signal to the screen */
   smsg.role = DIOSIX_ROLE_CONSOLEVIDEO;
   smsg.tid = DIOSIX_MSG_ANY_THREAD;
   smsg.pid = DIOSIX_MSG_ANY_PROCESS;
   smsg.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_SENDASUSR;
   smsg.signal.number = 64;
   
   /* set up the message block to receive a signal from the keyboard */
   msg.role = DIOSIX_ROLE_CONSOLEKEYBOARD;
   msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_SIGNAL;
   
   while(1)
   {
      if(diosix_msg_receive(&msg) == success && msg.signal.number == 64)
      {
         /* forward the signal */
         smsg.signal.extra = msg.signal.extra;
         diosix_msg_send(&smsg);
         
         /* prevent multiple role lookups */
         if(smsg.pid) smsg.role = DIOSIX_ROLE_NONE;
         if(msg.pid) msg.role = DIOSIX_ROLE_NONE;
      }
   }
}

int main(void)
{   
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_VFS);

   /* spawn a thread to deal with the keyboard and sceen */
   if(diosix_thread_fork() == 0) echo_keyboard();
   
   while(1); /* wait */
}
