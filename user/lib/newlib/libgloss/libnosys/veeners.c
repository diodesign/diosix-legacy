/* user/lib/newlib/libc/sys/diosix-i386/veeners.c
 * the syscall veneers for i386 applications to talk to the microkernel
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
   
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_EXIT));
   
   /* execution shouldn't reach here */
   while(1) diosix_thread_yield();
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

unsigned int diosix_alarm(unsigned int ticks)
/* send a SIGALRM signal to the calling process after the given number of scheduler clock ticks */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (ticks), "d" (SYSCALL_ALARM));
   return retval;
}

/* --------------- threading basics -------------------- */

void diosix_thread_yield(void)
/* give up the processor now for another thread */
{
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_THREAD_YIELD));
}

unsigned int diosix_thread_exit(unsigned int code)
/* end execution of this thread with the given
 return code - will also kill the process if there are no
 other threads running */
{
   /* send a message to the sysexec with the return code
    if it is non-zero */
   
   __asm__ __volatile__("int $0x90" : : "d" (SYSCALL_THREAD_EXIT));
   
   /* execution shouldn't reach here */
   while(1) diosix_thread_yield();
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

unsigned int diosix_thread_sleep(unsigned int ticks)
/* block for the given number of scheduler clock ticks. There are SCHED_FREQUENCY ticks a second */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (ticks), "d" (SYSCALL_THREAD_SLEEP));
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

/* -------------- process rights and privileges basics ------------ */
unsigned int diosix_priv_layer_up(void)
/* move up the privilege stack: the higher a process the less privileged it is */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_PRIV_LAYER_UP), "d" (SYSCALL_PRIVS));
   return retval;  
}

unsigned int diosix_rights_clear(unsigned int bits)
/* give up previously afforded process rights */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_RIGHTS_CLEAR), "b" (bits), "d" (SYSCALL_PRIVS));
   return retval;
}

unsigned int diosix_set_pg_id(unsigned int pid, unsigned int pgid)
/* alter the process's process group id */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETPGID), "b" (pid), "c" (pgid), "d" (SYSCALL_SET_ID));
   return retval;
}

unsigned int diosix_set_session_id(void)
/* alter the process's session id */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETSID), "d" (SYSCALL_SET_ID));
   return retval;
}

unsigned int diosix_set_eid(unsigned char flag, unsigned int eid)
/* alter the process's effective user id if flag is 1 or effective group id if flag is 0 */
{
   unsigned int retval;
   if(flag == DIOSIX_SET_USER)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETEUID), "b" (eid), "d" (SYSCALL_SET_ID));
   else
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETEGID), "b" (eid), "d" (SYSCALL_SET_ID));
   return retval;
}

unsigned int diosix_set_reid(unsigned char flag, unsigned int eid, unsigned int rid)
/* alter the process's real and effective user ids if flag is 1 or real and effective group ids if flag is 0 */
{
   unsigned int retval;
   if(flag == DIOSIX_SET_USER)
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETREUID), "b" (eid), "c" (rid), "d" (SYSCALL_SET_ID));
   else
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETREGID), "b" (eid), "c" (rid), "d" (SYSCALL_SET_ID));
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
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETRESUID), "b" (&ids), "d" (SYSCALL_SET_ID));
   else
      __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SETRESGID), "b" (&ids), "d" (SYSCALL_SET_ID));
   return retval;
}

unsigned int diosix_set_role(unsigned int role)
/* assign an operating system role to this process */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_SET_ROLE), "b" (role), "d" (SYSCALL_SET_ID));
   return retval;
}

unsigned int diosix_iorights_remove(void)
/* remove the previously afforded process right to access IO ports entirely */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_IORIGHTS_REMOVE), "d" (SYSCALL_PRIVS));
   return retval;
}

unsigned int diosix_iorights_clear(unsigned int index, unsigned int bits)
/* remove selected IO port rights for the process */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_IORIGHTS_CLEAR), "b" (index), "c" (bits), "d" (SYSCALL_PRIVS));
   return retval;
}

unsigned int diosix_signals_unix(unsigned int mask)
/* bitfield to enable unix signals to be received, a set bit indicates the signal will be accepted */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_UNIX_SIGNALS), "b" (mask), "d" (SYSCALL_PRIVS));
   return retval;   
}

unsigned int diosix_signals_kernel(unsigned int mask)
/* bitfield to enable kernel-generated signals to be received, a set bit indicates the signal will be accepted */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_KERNEL_SIGNALS), "b" (mask), "d" (SYSCALL_PRIVS));
   return retval;   
}

/* --------------------------- driver management -------------------- */
unsigned int diosix_driver_register(void)
/* remove the previously afforded process right to access IO ports entirely */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_REGISTER), "d" (SYSCALL_DRIVER));
   return retval;
}

unsigned int diosix_driver_deregister(void)
/* remove the previously afforded process right to access IO ports entirely */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_DEREGISTER), "d" (SYSCALL_DRIVER));
   return retval;
}

unsigned int diosix_driver_map_phys(diosix_phys_request *block)
/* map some physical memory into process space, for driver threads with PROC_FLAG_CANMAPPHYS set only */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_MAP_PHYS), "b" (block), "d" (SYSCALL_DRIVER));
   return retval;
}

unsigned int diosix_driver_unmap_phys(diosix_phys_request *block)
/* unmap some physical memory into process space, for driver threads with PROC_FLAG_CANMAPPHYS set only */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_UNMAP_PHYS), "b" (block), "d" (SYSCALL_DRIVER));
   return retval;
}

unsigned int diosix_driver_register_irq(unsigned char irq)
/* tell the kernel to send irq signals to the calling thread */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_REGISTER_IRQ), "b" (irq), "d" (SYSCALL_DRIVER));
   return retval;
}

unsigned int diosix_driver_deregister_irq(unsigned char irq)
/* tell the kernel to stop sending irq signals to the calling thread */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_DRIVER_DEREGISTER_IRQ), "b" (irq), "d" (SYSCALL_DRIVER));
   return retval;
}

/* --------------- get information out of the kernel ---------------- */
unsigned int diosix_get_thread_info(diosix_thread_info *block)
/* get info about this thread */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_THREAD_INFO), "d" (SYSCALL_INFO));  
   return retval;
}

unsigned int diosix_get_process_info(diosix_process_info *block)
/* get info about this thread */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_PROCESS_INFO), "d" (SYSCALL_INFO));  
   return retval;
}

unsigned int diosix_get_kernel_info(diosix_kernel_info *block)
/* get information about this kernel and the system it's running on */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (block), "b" (DIOSIX_KERNEL_INFO), "d" (SYSCALL_INFO));  
   return retval;
}

/* ----------------------- virtual memory management ---------------- */
unsigned int diosix_memory_create(void *ptr, unsigned int size)
/* create a new virtual memory area at address ptr of size bytes */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_CREATE), "b" (ptr), "c" (size), "d" (SYSCALL_MEMORY));  
   return retval;  
}

unsigned int diosix_memory_destroy(void *ptr)
/* destroy a VMA that has the address ptr */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_DESTROY), "b" (ptr), "d" (SYSCALL_MEMORY));  
   return retval; 
}

unsigned int diosix_memory_resize(void *ptr, signed int change)
/* increase or decrease by change bytes the size of a VMA given by ptr */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_RESIZE), "b" (ptr), "c" (change), "d" (SYSCALL_MEMORY));  
   return retval; 
}

unsigned int diosix_memory_access(void *ptr, unsigned int bits)
/* set the VMA_ACCESS_MASK access flags using bits for a VMA given by ptr */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_ACCESS), "b" (ptr), "c" (bits), "d" (SYSCALL_MEMORY));  
   return retval; 
}

unsigned int diosix_memory_locate(void **ptr, unsigned int type)
/* write the address of the VMA identified by the type bits into the ptr pointer */
{
   unsigned int retval;
   __asm__ __volatile__("int $0x90" : "=a" (retval) : "a" (DIOSIX_MEMORY_LOCATE), "b" (ptr), "c" (type), "d" (SYSCALL_MEMORY));  
   return retval; 
}