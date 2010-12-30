/* user/lib/newlib/libgloss/libnosys/kill.c
 * portable interface of the kill() syscall between libc and the diosix microkernel
 * Author : Chris Williams
 * Date   : Wed,22 Dec 2010.01:27:00
 
 Copyright (c) Chris Williams and individual contributors
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
 
*/

/* portable libc definitions */
#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <string.h>
#include <errno.h>
#undef errno
extern int errno;

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"

/* kill()
   summary: send a signal to a given process
   reference: http://pubs.opengroup.org/onlinepubs/009695399/functions/kill.html */

int
_DEFUN (_kill, (pid, sig),
        int pid  _AND
        int sig)
{
   diosix_msg_info msg;
   unsigned int result;
   
   /* zero the message block to send the signal */
   memset(&msg, 0, sizeof(diosix_msg_info));
   
   /* fill out the signal's details */
   msg.pid = pid; /* target process */
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.signal.number = sig;
   msg.signal.extra = 0;
   
   /* interpret pid appropriately */
   msg.flags = DIOSIX_MSG_SIGNAL;
   if(!pid) msg.flags |= DIOSIX_MSG_INMYPROCGRP;
   if(pid < -1)
   {
      msg.pid = abs(msg.pid);
      msg.flags |= DIOSIX_MSG_INAPROCGRP;
   }
   
   /* send the signal and return */
   result = diosix_msg_send(&msg);
   if(result) return -1; /* failure */
   return 0; /* success */
}


