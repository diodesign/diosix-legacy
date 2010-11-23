/* kernel/ports/i386/syscalls/sys_proc.c
 * i386-specific software interrupt handling for creating and killing processes
 * Author : Chris Williams
 * Date   : Sat,29 Nov 2009.18:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* syscall: exit - terminate execution of the current process */
void syscall_do_exit(int_registers_block *regs)
{
   SYSCALL_DEBUG("[sys:%i] SYSCALL_EXIT called by process %i (%p) (thread %i)\n",
              CPU_ID, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
              cpu_table[CPU_ID].current->tid);

   /* request to be killed by the system executive */
   msg_send_signal(proc_sys_executive, NULL, SIGXPROCEXIT, cpu_table[CPU_ID].current->proc->pid);

   /* remove from the run queue and mark as dying */
   sched_remove(cpu_table[CPU_ID].current, dead);
}

/* syscall: fork - duplicate the currently running process as per:
   http://www.opengroup.org/onlinepubs/009695399/functions/fork.html
   <= eax = -1 for failure, 0 for the child or the new child's PID for the parent */
void syscall_do_fork(int_registers_block *regs)
{
   process *new;
   thread *source = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_FORK called by process %i (%p) (thread %i)\n",
            CPU_ID, source->proc->pid, source->proc, source->tid);
   
   new = proc_new(source->proc, source);
   
   /* return the right info */
   if(!new) SYSCALL_RETURN(POSIX_GENERIC_FAILURE); /* POSIX_GENERIC_FAILURE == -1 */
   
   process *current = source->proc;
   
   /* sort out io port access map stuff */
   if(current->ioport_bitmap && (current->flags & PROC_FLAG_CANBEDRIVER))
      x86_ioports_clone(new, current);

   thread *tnew = thread_find_thread(new, source->tid);
   
   /* sort out the io bitmap stuff for the thread */
   if(source->flags & THREAD_FLAG_ISDRIVER)
   {
      new->flags |= THREAD_FLAG_ISDRIVER;
      x86_ioports_enable(tnew);
   }
   
   /* don't forget to init the tss for the new thread in the new process and duplicate the 
      state of the current thread and its tss */
   vmm_memcpy(&(tnew->regs), regs, sizeof(int_registers_block));
   x86_init_tss(tnew);
   
   /* zero eax on the new thread and set the child PID in eax for the parent */
   tnew->regs.eax = 0;
   regs->eax = new->pid;
   
   /* run the new thread */
   sched_add(tnew->proc->cpu, tnew);
}
         
/* syscall: kill - terminate the given process. processes can only kill processes
   in the layers above them or their children. if a process wants
   to terminate itself, it should ask the executive or its parent via syscall:exit
   => eax = PID for the process to kill
   <= eax = 0 for success or -1 for a failure
*/
void syscall_do_kill(int_registers_block *regs)
{
   unsigned int victim = regs->eax;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_KILL(%i) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);         
   
   regs->eax = proc_kill(victim, cpu_table[CPU_ID].current->proc);
   
   if(!(regs->eax) && cpu_table[CPU_ID].current->proc != proc_sys_executive)
      /* inform the system executive that a process has been killed */
      msg_send_signal(proc_sys_executive, NULL, SIGXPROCKILLED, victim);
}
