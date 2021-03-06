/* kernel/ports/arm/syscalls/sys_thread.c
 * ARM-specific software interrupt handling for creating and killing threads
 * Author : Chris Williams
 * Date   : Sun,17 Apr 2011.18:24:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

         
/* syscall: yield - give up the processor to another thread, if it exists. A thread that
   has run out of work for the time being can use this syscall to immediately
   release the cpu rather than spin waiting until its timeslice runs out.
   This call will return when the scheduler picks the thread to run again, 
   which could be immediately if there's no other threads waiting.
   NOTE: this syscall should really be called thread_yield :-/
   => no parameters required
   <= all registers preserved
*/
void syscall_do_yield(int_registers_block *regs)
{
   SYSCALL_DEBUG("[sys:%i] SYSCALL_YIELD called by process %i (%p) (thread %i)\n",
            CPU_ID, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
            cpu_table[CPU_ID].current->tid);

   sched_priority_calc(cpu_table[CPU_ID].current, priority_punish);
   sched_move_to_end(CPU_ID, cpu_table[CPU_ID].current);

   /* the syscall dispatch will call sched_pick() for us */
}

/* syscall thread_exit - end termination of the currently running thread.
   if it's the only thread in the process, the whole process will exit */
void syscall_do_thread_exit(int_registers_block *regs)
{
   SYSCALL_DEBUG("[sys:%i] SYSCALL_THREAD_EXIT called by process %i (%p) (thread %i threads %i)\n",
           CPU_ID, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid, cpu_table[CPU_ID].current->proc->thread_count);
   
   /* check to see if this process has only one thread */
   if(cpu_table[CPU_ID].current->proc->thread_count < 2)
   {
      syscall_do_exit(regs);
      return;
   }
   
   /* request to be killed by the system executive */
   msg_send_signal(proc_role_lookup(DIOSIX_ROLE_SYSTEM_EXECUTIVE), NULL, SIGXTHREADEXIT, cpu_table[CPU_ID].current->proc->pid);
   
   /* remove from the run queue and mark as dying */
   sched_remove(cpu_table[CPU_ID].current, dead);   
}

/* syscall: thread_fork - duplicate the current thread inside the currently-running
   process, much like a process fork()
   <= r0 = -1 for failure, 0 for the new thread or the new thread's TID for the caller */
void syscall_do_thread_fork(int_registers_block *regs)
{
   thread *new, *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_THREAD_FORK called by process %i (%p) (thread %i user sp %x)\n",
                 CPU_ID, current->proc->pid, current->proc, current->tid, regs->r13);

   /* create new thread and mark it as ready to run in usermode */
   new = thread_new(current->proc);
   
   if(!new) SYSCALL_RETURN(POSIX_GENERIC_FAILURE);
   
   new->flags |= THREAD_FLAG_INUSERMODE;
   
   /* sort out the driver stuff */
   if(current->flags & THREAD_FLAG_ISDRIVER)
      new->flags |= THREAD_FLAG_ISDRIVER;

   /* copy the state of the caller thread into the state of the new thread */
   vmm_memcpy(&(new->regs), regs, sizeof(int_registers_block));

   /* fix up the stack pointer and duplicate the stack */
   new->regs.r13 = new->stackbase - (current->stackbase - regs->r13);
   
   SYSCALL_DEBUG("[sys:%i] cloning stack: target sp %x source sp %x (%i bytes) (target stackbase %x source stackbase %x)\n",
                 CPU_ID, new->regs.r13, regs->r13, current->stackbase - regs->r13, new->stackbase, current->stackbase);
   
   vmm_memcpy((void *)(new->regs.r13), (void *)(regs->r13), current->stackbase - regs->r13);
   
   /* zero r0 on the new thread and set the child PID in r0 for the parent */
   new->regs.r0 = 0;
   regs->r0 = new->tid;
   
   /* run the new thred */
   sched_add(new->proc->cpu, new);   
}

/* syscall: thread_kill - terminate the given thread. threads can only kill threads
   within the same process using this syscall. if a thread wishes to terminate
   itself, it should ask the executive via syscall:thread_exit
   => r0 = TID for the thread to kill
   <= r0 = 0 for success or -1 for a failure
*/
void syscall_do_thread_kill(int_registers_block *regs)
{
   kresult err;
   process *owner = cpu_table[CPU_ID].current->proc;
   thread *victim = thread_find_thread(owner, regs->r0);
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_THREAD_KILL(%i) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->r0, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);         
   
   /* bail out if we can't find the given thread or we're trying to kill ourselves */
   if(!victim || (victim == cpu_table[CPU_ID].current)) SYSCALL_RETURN(POSIX_GENERIC_FAILURE);
   
   /* remove the thread */
   err = thread_kill(owner, victim);
   if(err) SYSCALL_RETURN(POSIX_GENERIC_FAILURE);

   /* stop this thread from running and mark it as dying for the sysexec to clear up */
   sched_remove(victim, dead);
   
   if(cpu_table[CPU_ID].current->proc != proc_role_lookup(DIOSIX_ROLE_SYSTEM_EXECUTIVE))
      /* inform the system executive that a thread has been killed */
      msg_send_signal(proc_role_lookup(DIOSIX_ROLE_SYSTEM_EXECUTIVE), NULL, SIGXTHREADKILLED, owner->pid);
}

/* syscall: thread_sleep - block the current thread for a given number of scheduling ticks.
   the thread is placed on the run queue again once the time has elapsed.
   => r0 = number of ticks to sleep for, or 0 to cancel any outstanding alarms
   <= r0 = 0 for success (after blocking), or an error code
*/
void syscall_do_thread_sleep(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_THREAD_SLEEP(%i) called by process %i (%p) (thread %i)\n",
                 CPU_ID, regs->r0, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
                 cpu_table[CPU_ID].current->tid);
   
   SYSCALL_RETURN(sched_add_snoozer(current, regs->r0, wake));
}
