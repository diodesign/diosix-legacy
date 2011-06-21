/* user/lib/newlib/libc/sys/diosix-i386/veeners.c
 * the syscall veneers for applications to talk to the microkernel
 * Author : Chris Williams
 * Date   : Sat,14 Nov 2009.17:17:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"

/* veneers to syscalls - for full usage, see the kernel
   source for comments or check the documentation */

/* --------------- process basics -------------------- */

unsigned int diosix_exit(unsigned int code)
/* end execution of this process with the given
   return code */
{
   /* send a message to the sysexec with the return code
      if it is non-zero */
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_EXIT));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %0; swi $0x0" : : "i" (SYSCALL_EXIT));
#endif
   
   /* execution shouldn't reach here */
   while(1) diosix_thread_yield();
}

int diosix_fork(void)
/* fork and create a child process, returns 0 for the child,
   the new child PID for the parent, or -1 for failure */
{
   int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "d" (SYSCALL_FORK));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (SYSCALL_FORK));
#endif
   return retval;
}

unsigned int diosix_kill(unsigned int pid)
/* attempt to kill a process with a matching pid.
   check the documentation on what you can and can't kill */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (pid), "d" (SYSCALL_KILL));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (pid), "i" (SYSCALL_KILL));
#endif
   return retval;
}

unsigned int diosix_alarm(unsigned int ticks)
/* send a SIGALRM signal to the calling process after the given number of scheduler clock ticks */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (ticks), "d" (SYSCALL_ALARM));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (ticks), "i" (SYSCALL_ALARM));
#endif
   return retval;
}

/* --------------- threading basics -------------------- */

void diosix_thread_yield(void)
/* give up the processor now for another thread */
{
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_THREAD_YIELD));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %0; swi $0x0" : : "i" (SYSCALL_THREAD_YIELD));
#endif
}

unsigned int diosix_thread_exit(unsigned int code)
/* end execution of this thread with the given
   return code - will also kill the process if there are no
   other threads running */
{
   /* send a message to the sysexec with the return code
    if it is non-zero */
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_THREAD_EXIT));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %0; swi $0x0" : : "i" (SYSCALL_THREAD_EXIT));
#endif
   
   /* execution shouldn't reach here */
   while(1) diosix_thread_yield();
}

int diosix_thread_fork(void)
/* fork the current thread within the process. returns 0 for
   the child or the new thread id (tid) for the parent, or -1 for failure */
{
   int retval;
   
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "d" (SYSCALL_THREAD_FORK));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (SYSCALL_THREAD_FORK));
#endif  

   return retval;   
}

unsigned int diosix_thread_kill(unsigned int tid)
/* attempt to kill a thread with a matching tid within this process.
   check the documentation on what you can and can't kill */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (tid), "d" (SYSCALL_THREAD_KILL));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (tid), "i" (SYSCALL_THREAD_KILL));
#endif
   return retval;
}

unsigned int diosix_thread_sleep(unsigned int ticks)
/* block for the given number of scheduler clock ticks. There are SCHED_FREQUENCY ticks a second */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (ticks), "d" (SYSCALL_THREAD_SLEEP));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (ticks), "i" (SYSCALL_THREAD_SLEEP));
#endif
   return retval;
}

/* --------------- message basics -------------------- */

unsigned int diosix_msg_send(diosix_msg_info *info)
/* send a message and block until a reply, or return with a failure code
   => info = pointer to msg info block, filled in with reply info
*/
{
   unsigned int retval;

   while(1)
   {
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (info), "d" (SYSCALL_MSG_SEND));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (info), "i" (SYSCALL_MSG_SEND));
#endif
      
      /* if a message is marked to queue then keep trying to resend
         if the receiver is not available */
      if(info->flags & DIOSIX_MSG_QUEUEME)
      {
         if(retval != e_no_receiver) return retval;
      }
      else
         return retval;
      
      /* don't hog the cpu if there's no need - sleep for a bit before trying again */
      diosix_thread_sleep(DIOSIX_SCHED_MSGWAIT);
   }
}

unsigned int diosix_msg_receive(diosix_msg_info *info)
/* block until a message arrives, or return with a failure code
   => info = pointer to msg info block, filled in with received message info
*/
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (info), "d" (SYSCALL_MSG_RECV));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (info), "i" (SYSCALL_MSG_RECV));
#endif
   return retval;
}

unsigned int diosix_msg_reply(diosix_msg_info *info)
/* reply to a message to unblock the sender, or return with a failure code */
{
   info->flags |= DIOSIX_MSG_REPLY;
   return diosix_msg_send(info);
}

/* -------------- process rights and privileges basics ------------ */
unsigned int diosix_priv_layer_up(unsigned int count)
/* move up the privilege stack by count number of layers: the higher a process the less privileged it is */
{
   unsigned int retval;
   while(count)
   {
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_PRIV_LAYER_UP), "d" (SYSCALL_PRIVS));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_PRIV_LAYER_UP), "i" (SYSCALL_PRIVS));
#endif
      count--;
   }
   return retval;  
}

unsigned int diosix_rights_clear(unsigned int bits)
/* give up previously afforded process rights */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_RIGHTS_CLEAR), "b" (bits), "d" (SYSCALL_PRIVS));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_RIGHTS_CLEAR), "r" (bits), "i" (SYSCALL_PRIVS));
#endif
   return retval;
}

unsigned int diosix_set_pg_id(unsigned int pid, unsigned int pgid)
/* alter the process's process group id */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETPGID), "b" (pid), "c" (pgid), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %4; mov r2, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETPGID), "r" (pid), "r" (pgid), "i" (SYSCALL_SET_ID));
#endif
   return retval;
}

unsigned int diosix_set_session_id(void)
/* alter the process's session id */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETSID), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETSID), "i" (SYSCALL_SET_ID));
#endif
   return retval;
}

unsigned int diosix_set_eid(unsigned char flag, unsigned int eid)
/* alter the process's effective user id if flag is 1 or effective group id if flag is 0 */
{
   unsigned int retval;
   if(flag == DIOSIX_SET_USER)
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETEUID), "b" (eid), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETEUID), "r" (eid), "i" (SYSCALL_SET_ID));
#endif
   else
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETEGID), "b" (eid), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETEGID), "r" (eid), "i" (SYSCALL_SET_ID));
#endif
   return retval;
}

unsigned int diosix_set_reid(unsigned char flag, unsigned int eid, unsigned int rid)
/* alter the process's real and effective user ids if flag is 1 or real and effective group ids if flag is 0 */
{
   unsigned int retval;
   if(flag == DIOSIX_SET_USER)
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETREUID), "b" (eid), "c" (rid), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %4; mov r2, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETREUID), "r" (eid), "r" (rid), "i" (SYSCALL_SET_ID));
#endif
   else
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETREGID), "b" (eid), "c" (rid), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %4; mov r2, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETREGID), "r" (eid), "r" (rid), "i" (SYSCALL_SET_ID));
#endif
   return retval;
}

unsigned int diosix_set_resid(unsigned char flag, unsigned eid, unsigned int rid, unsigned sid)
/* alter all process's user ids if flag is 1 or all its group ids if flag is 0 */
{
   unsigned int retval;
   posix_id_set ids;
   
   /* copy values into block */
   ids.effective = eid;
   ids.real      = rid;
   ids.saved     = sid;
   
   if(flag == DIOSIX_SET_USER)
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETRESUID), "b" (&ids), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
      __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETRESUID), "r" (&ids), "i" (SYSCALL_SET_ID));
#endif
   else
#if defined (__i386__)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETRESGID), "b" (&ids), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
      __asm__ __volatile__( "mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SETRESGID), "r" (&ids), "i" (SYSCALL_SET_ID));
#endif
   return retval;
}

unsigned int diosix_set_role(unsigned int role)
/* assign an operating system role to this process */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SET_ROLE), "b" (role), "d" (SYSCALL_SET_ID));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_SET_ROLE), "r" (role), "i" (SYSCALL_SET_ID));
#endif
   return retval;
}

unsigned int diosix_iorights_remove(void)
/* remove the previously afforded process right to access IO ports entirely */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_IORIGHTS_REMOVE), "d" (SYSCALL_PRIVS));
#elif defined (__arm__)
   /* ARM doesn't support IO ports */
   retval = e_notimplemented;
#endif
   return retval;
}

unsigned int diosix_iorights_clear(unsigned int index, unsigned int bits)
/* remove selected IO port rights for the process */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_IORIGHTS_CLEAR), "b" (index), "c" (bits), "d" (SYSCALL_PRIVS));
#elif defined (__arm__)
   /* ARM doesn't support IO ports */
   retval = e_notimplemented;
#endif
   return retval;
}

unsigned int diosix_signals_unix(unsigned int mask)
/* bitfield to enable unix signals to be received, a set bit indicates the signal will be accepted */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_UNIX_SIGNALS), "b" (mask), "d" (SYSCALL_PRIVS));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0"  : "=r" (retval) : "i" (DIOSIX_UNIX_SIGNALS), "r" (mask), "i" (SYSCALL_PRIVS));
#endif
   return retval;   
}

unsigned int diosix_signals_kernel(unsigned int mask)
/* bitfield to enable kernel-generated signals to be received, a set bit indicates the signal will be accepted */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_KERNEL_SIGNALS), "b" (mask), "d" (SYSCALL_PRIVS));
#elif defined (__arm__)
   __asm__ __volatile__( "mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_KERNEL_SIGNALS), "r" (mask), "i" (SYSCALL_PRIVS));
#endif
   return retval;   
}

/* --------------------------- driver management -------------------- */
unsigned int diosix_driver_register(void)
/* request access to hardware from suerspace */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_REGISTER), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_REGISTER), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_deregister(void)
/* remove the previously afforded process right to access hardware entirely */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_DEREGISTER), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_DEREGISTER), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_map_phys(diosix_phys_request *block)
/* map some physical memory into process space, for driver threads with PROC_FLAG_CANMAPPHYS set only */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_MAP_PHYS), "b" (block), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_MAP_PHYS), "r" (block), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_unmap_phys(diosix_phys_request *block)
/* unmap some physical memory into process space, for driver threads with PROC_FLAG_CANMAPPHYS set only */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_UNMAP_PHYS), "b" (block), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_UNMAP_PHYS), "r" (block), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_req_phys(unsigned short pages, unsigned int *addr)
/* request a block of contiguous physical memory, store base address in addr */
{
   unsigned int retval, data;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval), "=c" (data) : "a" (DIOSIX_DRIVER_REQ_PHYS), "b" (pages), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %4; mov r1, %3; mov r0, %2; swi $0x0; mov %0, r0; mov %1, r1" : "=r" (retval), "=r" (data) : "i" (DIOSIX_DRIVER_REQ_PHYS), "r" (pages), "i" (SYSCALL_DRIVER));
#endif
   
   *addr = data;
   return retval;
}

unsigned int diosix_driver_ret_phys(unsigned int addr)
/* release a previously allocated block of physical memory */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_RET_PHYS), "b" (addr), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_RET_PHYS), "r" (addr), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_register_irq(unsigned char irq)
/* tell the kernel to send irq signals to the calling thread */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_REGISTER_IRQ), "b" (irq), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_REGISTER_IRQ), "r" (irq), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_deregister_irq(unsigned char irq)
/* tell the kernel to stop sending irq signals to the calling thread */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_DEREGISTER_IRQ), "b" (irq), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DRIVER_DEREGISTER_IRQ), "r" (irq), "i" (SYSCALL_DRIVER));
#endif
   return retval;
}

unsigned int diosix_driver_iorequest(diosix_ioport_request *req)
/* ask the kernel to perform an IO port access */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_IOREQUEST), "b" (req), "d" (SYSCALL_DRIVER));
#elif defined (__arm__)
   /* not supported in the ARM architecture */
   return e_notimplemented;
#endif
   
   return retval;
}

/* --------------- get information out of the kernel ---------------- */
unsigned int diosix_get_thread_info(diosix_thread_info *block)
/* get info about this thread */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_THREAD_INFO), "d" (SYSCALL_INFO));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (block), "i" (DIOSIX_THREAD_INFO), "i" (SYSCALL_INFO));
#endif
   return retval;
}

unsigned int diosix_get_process_info(diosix_process_info *block)
/* get info about this thread */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_PROCESS_INFO), "d" (SYSCALL_INFO));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "r" (block), "i" (DIOSIX_PROCESS_INFO), "i" (SYSCALL_INFO));
#endif
   return retval;
}

unsigned int diosix_get_kernel_info(diosix_kernel_info *block)
/* get information about this kernel and the system it's running on */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_KERNEL_INFO), "d" (SYSCALL_INFO));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0"  : "=r" (retval) : "r" (block), "i" (DIOSIX_KERNEL_INFO), "i" (SYSCALL_INFO));
#endif
   return retval;
}

unsigned int diosix_get_kernel_stats(diosix_kernel_stats *block)
/* get statistics about this kernel */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_KERNEL_STATISTICS), "d" (SYSCALL_INFO));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0"  : "=r" (retval) : "r" (block), "i" (DIOSIX_KERNEL_STATISTICS), "i" (SYSCALL_INFO));
#endif
   return retval;
}

/* ----------------------- virtual memory management ---------------- */
unsigned int diosix_memory_create(void *ptr, unsigned int size)
/* create a new virtual memory area at address ptr of size bytes */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_CREATE), "b" (ptr), "c" (size), "d" (SYSCALL_MEMORY));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %4; mov r2, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_MEMORY_CREATE), "r" (ptr), "r" (size), "i" (SYSCALL_MEMORY));
#endif
   return retval;  
}

unsigned int diosix_memory_destroy(void *ptr)
/* destroy a VMA that has the address ptr */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_DESTROY), "b" (ptr), "d" (SYSCALL_MEMORY));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0"  : "=r" (retval) : "i" (DIOSIX_MEMORY_DESTROY), "r" (ptr), "i" (SYSCALL_MEMORY));
#endif
   return retval; 
}

unsigned int diosix_memory_resize(void *ptr, signed int change)
/* increase or decrease by change bytes the size of a VMA given by ptr */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_RESIZE), "b" (ptr), "c" (change), "d" (SYSCALL_MEMORY));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %4; mov r2, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0"  : "=r" (retval) : "i" (DIOSIX_MEMORY_RESIZE), "r" (ptr), "r" (change), "i" (SYSCALL_MEMORY));
#endif
   return retval; 
}

unsigned int diosix_memory_access(void *ptr, unsigned int bits)
/* set the VMA_ACCESS_MASK access flags using bits for a VMA given by ptr */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_ACCESS), "b" (ptr), "c" (bits), "d" (SYSCALL_MEMORY));
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %4; mov r3, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0"  : "=r" (retval) : "i" (DIOSIX_MEMORY_ACCESS), "r" (ptr), "r" (bits), "i" (SYSCALL_MEMORY));
#endif
   return retval; 
}

unsigned int diosix_memory_locate(void **ptr, unsigned int type)
/* write the address of the VMA identified by the type bits into the ptr pointer */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_LOCATE), "b" (ptr), "c" (type), "d" (SYSCALL_MEMORY));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %4; mov r2, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_MEMORY_LOCATE), "r" (ptr), "r" (type), "i" (SYSCALL_MEMORY));
#endif
   return retval; 
}

/* ----------------------- debugging support ---------------- */
unsigned int diosix_debug_write(const char *ptr)
/* write C-string ptr out to the kernel's debug channel (such as serial IO) (root-only) */
{
   unsigned int retval;
#if defined (__i386__)
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DEBUG_WRITE), "b" (ptr), "d" (SYSCALL_USRDEBUG));  
#elif defined (__arm__)
   __asm__ __volatile__("mov r4, %3; mov r1, %2; mov r0, %1; swi $0x0; mov %0, r0" : "=r" (retval) : "i" (DIOSIX_DEBUG_WRITE), "r" (ptr), "i" (SYSCALL_USRDEBUG));
#endif
   return retval;  
}
