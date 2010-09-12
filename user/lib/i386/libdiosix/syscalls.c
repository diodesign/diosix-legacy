/* user/lib/i386/libdiosix/syscalls.h
 * the syscall veneers for applications to talk to the microkernel
 * Author : Chris Williams
 * Date   : Sat,14 Nov 2009.17:17:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <diosix.h>
#include <functions.h>

/* veneers to syscalls - for full usage, see the kernel
   source for comments or check the documentation */

/* --------------- process basics -------------------- */

unsigned int diosix_exit(unsigned int code)
/* end execution of this process with the given
   return code */
{
   /* send a message to the sysexec with the return code
      if it is non-zero */
   
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_EXIT));
   
   /* execution shouldn't reach here */
   while(1) diosix_yield();
}

int diosix_fork(void)
/* fork and create a child process, returns 0 for the child,
   the new child PID for the parent, or -1 for failure */
{
   int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "d" (SYSCALL_FORK));  
   return retval;
}

unsigned int diosix_kill(unsigned int pid)
/* attempt to kill a process with a matching pid.
   check the documentation on what you can and can't kill */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (pid), "d" (SYSCALL_KILL));  
   return retval;
}

void diosix_yield(void)
/* give up the processor now for another thread */
{
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_YIELD));
}

/* --------------- threading basics -------------------- */

unsigned int diosix_thread_exit(unsigned int code)
/* end execution of this thread with the given
 return code - will also kill the process if there are no
 other threads running */
{
   /* send a message to the sysexec with the return code
    if it is non-zero */
   
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_THREAD_EXIT));
   
   /* execution shouldn't reach here */
   while(1) diosix_yield();
}

int diosix_thread_fork(void)
/* fork the current thread within the process. returns 0 for
   the child or the new thread id (tid) for the parent, or -1 for failure */
{
   int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "d" (SYSCALL_THREAD_FORK));
   return retval;   
}

unsigned int diosix_thread_kill(unsigned int tid)
/* attempt to kill a thread with a matching tid within this process.
   check the documentation on what you can and can't kill */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (tid), "d" (SYSCALL_THREAD_KILL));
   return retval;
}

/* --------------- message basics -------------------- */

unsigned int diosix_msg_send(diosix_msg_info *info)
/* send a message and block until a reply, or return with a failure code
   => info = pointer to msg info block, filled in with reply info
*/
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (info), "d" (SYSCALL_MSG_SEND));
   return retval;
}

unsigned int diosix_msg_receive(diosix_msg_info *info)
/* block until a message arrives, or return with a failure code
   => info = pointer to msg info block, filled in with received message info
*/
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (info), "d" (SYSCALL_MSG_RECV));  
   return retval;
}

unsigned int diosix_msg_reply(diosix_msg_info *info)
/* reply to a message to unblock the sender, or return with a failure code */
{
   info->flags |= DIOSIX_MSG_REPLY;
   return diosix_msg_send(info);
}
