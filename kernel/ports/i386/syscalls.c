/* kernel/ports/i386/syscalls.c
 * i386-specific software interrupt handling
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
   msg_send_signal(proc_sys_executive, NULL, SIGXTHREADEXIT, cpu_table[CPU_ID].current->proc->pid);
   
   /* remove from the run queue and mark as dying */
   sched_remove(cpu_table[CPU_ID].current, dead);   
}

/* syscall: thread_fork - duplicate the current thread inside the currently-running
   process, much like a process fork()
   <= eax = -1 for failure, 0 for the new thread or the new thread's TID for the caller */
void syscall_do_thread_fork(int_registers_block *regs)
{
   thread *new, *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_THREAD_FORK called by process %i (%p) (thread %i usresp %x)\n",
           CPU_ID, current->proc->pid, current->proc, current->tid, regs->useresp);

   /* create new thread and mark it as ready to run in usermode */
   new = thread_new(current->proc);
   
   if(!new) SYSCALL_RETURN(POSIX_GENERIC_FAILURE);
   
   new->flags |= THREAD_FLAG_INUSERMODE;
   
   /* sort out the io bitmap stuff */
   if(current->flags & THREAD_FLAG_ISDRIVER)
   {
      new->flags |= THREAD_FLAG_ISDRIVER;
      x86_ioports_enable(new);
   }

   /* copy the state of the caller thread into the state of the new thread */
   vmm_memcpy(&(new->regs), regs, sizeof(int_registers_block));
   
   /* new threads start with an IO bitmap if the calling thread has one */
   x86_init_tss(new);

   /* fix up the stack pointer and duplicate the stack  - only fix up the
      ebp if it looks like an active frame pointer, ugh :( */
   new->regs.useresp = new->stackbase - (current->stackbase - regs->useresp);
   
   if((regs->ebp <= current->stackbase) &&
      (regs->ebp > (current->stackbase - (THREAD_MAX_STACK * MEM_PGSIZE))))
   {
      new->regs.ebp = new->stackbase - (current->stackbase - regs->ebp);
      SYSCALL_DEBUG("[sys:%i] fixing up ebp from %x to %x\n",
                    CPU_ID, regs->ebp, new->regs.ebp);
   }

   SYSCALL_DEBUG("[sys:%i] cloning stack: target esp %x ebp %x source esp %x ebp %x (%i bytes) (target stackbase %x source stackbase %x)\n",
           CPU_ID, new->regs.useresp, new->regs.ebp, regs->useresp, regs->ebp, current->stackbase - regs->useresp, new->stackbase, current->stackbase);
   
   vmm_memcpy((void *)(new->regs.useresp), (void *)(regs->useresp), current->stackbase - regs->useresp);
   
   /* zero eax on the new thread and set the child PID in eax for the parent */
   new->regs.eax = 0;
   regs->eax = new->tid;
   
   /* run the new thred */
   sched_add(new->proc->cpu, new);   
}

/* syscall: thread_kill - terminate the given thread. threads can only kill threads
   within the same process using this syscall. if a thread wishes to terminate
   itself, it should ask the executive via syscall:thread_exit
   => eax = TID for the thread to kill
   <= eax = 0 for success or -1 for a failure
*/
void syscall_do_thread_kill(int_registers_block *regs)
{
   kresult err;
   process *owner = cpu_table[CPU_ID].current->proc;
   thread *victim = thread_find_thread(owner, regs->eax);
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_THREAD_KILL(%i) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);         
   
   /* bail out if we can't find the given thread or we're trying to kill ourselves */
   if(!victim || (victim == cpu_table[CPU_ID].current)) SYSCALL_RETURN(POSIX_GENERIC_FAILURE);
   
   /* remove the thread */
   err = thread_kill(owner, victim);
   if(err) SYSCALL_RETURN(POSIX_GENERIC_FAILURE);

   /* stop this thread from running and mark it as dying for the sysexec to clear up */
   sched_remove(victim, dead);
   
   if(cpu_table[CPU_ID].current->proc != proc_sys_executive)
      /* inform the system executive that a thread has been killed */
      msg_send_signal(proc_sys_executive, NULL, SIGXTHREADKILLED, owner->pid);
}

/* syscall:msg_send - send a message to a process and block until a reply is received
   => eax = pointer to message description block
   <= eax = 0 for success or a diosix-specific error code
*/
void syscall_do_msg_send(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   diosix_msg_info *msg = (diosix_msg_info *)regs->eax;
   unsigned int send_result;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_MSG_SEND(%x) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);

   /* sanitise the input data while we're here */
   if(!msg || ((unsigned int)msg + MEM_CLIP(msg, sizeof(diosix_msg_info)) >= KERNEL_SPACE_BASE))
      SYSCALL_RETURN(e_bad_address);
   
   /* do the actual sending */
   send_result = msg_send(current, msg);
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_MSG_SEND(%x) msg_send() returned code %i\n",
                 CPU_ID, regs->eax, send_result);
   
   switch(send_result)
   {
      case success:
         /* reward the thread for multitasking */
         sched_priority_calc(current, priority_reward);
         
         /* should we wait for a follow-up message if this was a reply? */
         if((msg->flags & DIOSIX_MSG_REPLY) && (msg->flags & DIOSIX_MSG_RECVONREPLY))
         {         
            /* clear message flags to perform a recv */
            msg->flags &= DIOSIX_MSG_TYPEMASK;
            
            /* zero the send info and preserve everything else */
            msg->send_size = 0;
            msg->send = NULL;
            
            syscall_do_msg_recv(regs); /* will update eax when it returns */      
         }
         else
            SYSCALL_RETURN(success); /* let the sender know what happened */
         
      /* can't write into the receiver's buffer */
      case e_bad_target_address:
         SYSCALL_RETURN(e_no_receiver);
         
      default:
         /* fall through to returning msg_send()'s error code */
         SYSCALL_RETURN(send_result);
   }
}

/* syscall:msg_recv - receive a message or block until a message is received
   => eax = pointer to message description block
   <= eax = 0 for success or a diosix-specific error code
*/
void syscall_do_msg_recv(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   diosix_msg_info *msg = (diosix_msg_info *)regs->eax;

   SYSCALL_DEBUG("[sys:%i] SYSCALL_MSG_RECV(%x) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);
   
   /* sanitise the input data while we're here */
   if(!msg || ((unsigned int)msg >= KERNEL_SPACE_BASE)) SYSCALL_RETURN(e_bad_address);
   
   /* do the actual receiving, syscall_post_msg_recv() will set return code in eax */
   msg_recv(current, msg);
}

/* syscall_post_msg_recv
   Perform the final stage of syscall:msg_recv just before the call returns.
   It is called from the context of the sending thread.
   => receiver = thread that blocked awaiting a message
      result = return code from the syscall
*/
void syscall_post_msg_recv(thread *receiver, kresult result)
{
   diosix_msg_info *msg;
   
   if(!receiver) return; /* sanity check */
   
   /* get the address of the receiver's block - it's been verified by this point */
   msg = (diosix_msg_info *)receiver->regs.eax;
   
   MSG_DEBUG("[msg:%i] updating receiver %p (tid %i pid %i) msg %p result %i\n",
             CPU_ID, receiver, receiver->tid, receiver->proc->pid, msg, result);
   
   /* copy the receiver's updated message block back into its address space */
   if(vmm_memcpyuser(msg, receiver->proc, &(receiver->msg), NULL, sizeof(diosix_msg_info)))
      receiver->regs.eax = e_bad_address;
   else
      receiver->regs.eax = result;
}

/* syscall:privs - alter the privs/rights of a process
   => eax = DIOSIX_PRIV_LAYER_UP: move up a privilege layer
            DIOSIX_RIGHTS_CLEAR:
              => ebx = set a bit to remove the corresponding PROC_FLAGS rights flag
            DIOSIX_IORIGHTS_REMOVE: remove *all* the process's IO port access
            DIOSIX_IORIGHTS_CLEAR: remove selected IO port access from a process
              => ebx = index (in 32bit words) into 8bit x (2^16) bitmap
                 ecx = word to apply to bitmap. Set bits to remove corresponding port access
            DIOSIX_UNIX_SIGNALS:
              => ebx = bitfield of POSIX-compatible signals that process will accept
            DIOSIX_KERNEL_SIGNALS:
              => ebx = bitfield of kernel-generated signals that process will accept
   <= eax = 0 for success or an error code
*/
void syscall_do_privs(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_PRIVS(%i) called by process %i (thread %i)\n",
                 CPU_ID, regs->eax, current->proc->pid, current->tid);
   
   switch(regs->eax)
   {
      case DIOSIX_PRIV_LAYER_UP:
         SYSCALL_RETURN(proc_layer_up(current->proc));
         
      case DIOSIX_RIGHTS_CLEAR:
         SYSCALL_RETURN(proc_clear_rights(current->proc, regs->ebx));
         
      case DIOSIX_IORIGHTS_REMOVE:
         SYSCALL_RETURN(x86_ioports_clear_all(current->proc));
         
      case DIOSIX_IORIGHTS_CLEAR:
         SYSCALL_RETURN(x86_ioports_clear(current->proc, regs->ebx, regs->ecx));
         
      case DIOSIX_UNIX_SIGNALS:
         current->proc->unix_signals_accepted = regs->ebx;
         SYSCALL_RETURN(success);
         
      case DIOSIX_KERNEL_SIGNALS:
         current->proc->kernel_signals_accepted = regs->ebx;
         SYSCALL_RETURN(success);
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}

/* syscall:driver - perform driver management operations (layer 1 threads only)
   => eax = DIOSIX_DRIVER_REGISTER: register this thread as a driver capable
                                    of receiving interrupts, mapping phys ram
                                    and access selected IO ports.
            DIOSIX_DRIVER_DEREGISTER: deregister a thread as a driver.
            DIOSIX_DRIVER_MAP_PHYS: map some physical memory into the driver's virtual space
               => ebx = pointer to phys map request block
            DIOSIX_DRIVER_REGISTER_IRQ: route the given IRQ to the caller as a signal
               => ebx = IRQ number to register
            DIOSIX_DRIVER_DEREGISTER_IRQ: stop routing IRQ signals to the caller
               => ebx = IRQ number to deregister
   <= eax = 0 for success or an error code
*/
void syscall_do_driver(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   kresult err;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_DRIVER(%i) called by process %i (thread %i)\n",
                 CPU_ID, regs->eax, current->proc->pid, current->tid);
   
   /* bail out now if the thread isn't sufficiently privileged */
   if(current->proc->layer != LAYER_DRIVERS) SYSCALL_RETURN(e_no_rights);
   if(!(current->proc->flags & PROC_FLAG_CANBEDRIVER)) SYSCALL_RETURN(e_no_rights);
   
   switch(regs->eax)
   {
      case DIOSIX_DRIVER_REGISTER:
         if(!(current->flags & THREAD_FLAG_ISDRIVER))
         {
            err = x86_ioports_enable(current);
            if(err) SYSCALL_RETURN(err);
            
            current->flags |= THREAD_FLAG_ISDRIVER;
            
            SYSCALL_DEBUG("[sys:%i] thread registered as a driver\n", CPU_ID);
            SYSCALL_RETURN(success);
         }
         
      case DIOSIX_DRIVER_DEREGISTER:
         if(current->flags & THREAD_FLAG_ISDRIVER)
         {
            err = x86_ioports_disable(current);
            if(err) SYSCALL_RETURN(err);

            current->flags &= (~THREAD_FLAG_ISDRIVER);
            
            SYSCALL_DEBUG("[sys:%i] thread deregistered as a driver\n", CPU_ID);
            SYSCALL_RETURN(success);
         }
         
      case DIOSIX_DRIVER_MAP_PHYS:
         if((current->flags & THREAD_FLAG_ISDRIVER) &&
            (current->proc->flags & PROC_FLAG_CANMAPPHYS))
         {
            unsigned int flags, pgflags, pgloop;
            diosix_phys_request *req = (diosix_phys_request *)regs->ebx; 
            
            /* sanity checks */
            if(!req) SYSCALL_RETURN(e_bad_params);
            if((unsigned int)req + MEM_CLIP(req, sizeof(diosix_phys_request)) >= KERNEL_SPACE_BASE)
               SYSCALL_RETURN(e_bad_params);
            if((unsigned int)req->vaddr + MEM_CLIP(req->vaddr, req->size) >= KERNEL_SPACE_BASE)
               SYSCALL_RETURN(e_bad_params);
            if(req->paddr >= req->paddr + req->size)
               SYSCALL_RETURN(e_bad_params);
            if(!(req->vaddr) || !(req->size)) SYSCALL_RETURN(e_bad_params);
                        
            /* check the alignments */
            if(!(MEM_IS_PG_ALIGNED(req->paddr)) ||
               !(MEM_IS_PG_ALIGNED(req->vaddr)) ||
               !(MEM_IS_PG_ALIGNED(req->size)))
               SYSCALL_RETURN(e_bad_params);
            
            /* protect the kernel's physical critical section */
            if((unsigned int)req->paddr >= KERNEL_CRITICAL_BASE && (unsigned int)req->paddr < KERNEL_CRITICAL_END)
               SYSCALL_RETURN(e_bad_params);
            if((unsigned int)req->paddr < KERNEL_CRITICAL_BASE && ((unsigned int)req->paddr + req->size) > KERNEL_CRITICAL_BASE)
               SYSCALL_RETURN(e_bad_params);
                        
            /* check there's no conflict in mapping this area in */
            if(vmm_find_vma(current->proc, (unsigned int)req->vaddr))
               SYSCALL_RETURN(e_vma_exists);
            if(vmm_find_vma(current->proc, (unsigned int)req->vaddr + req->size))
               SYSCALL_RETURN(e_vma_exists);
                        
            /* sanatise the settings flags */
            flags = req->flags & (VMA_WRITEABLE | VMA_NOCACHE | VMA_FIXED);
            
            lock_gate(&(current->proc->lock), LOCK_WRITE);
            
            /* phew - let's create a new virtual memory area for the physical mapping */
            if(vmm_add_vma(current->proc, (unsigned int)req->vaddr, req->size, flags, 0))
            {
               unlock_gate(&(current->proc->lock), LOCK_WRITE);
               SYSCALL_RETURN(e_failure);
            }
            
            /* build page flags */
            pgflags = PG_PRESENT | PG_PRIVLVL;
            if(flags & VMA_WRITEABLE) pgflags |= PG_RW;
            if(flags & VMA_NOCACHE) pgflags |= PG_CACHEDIS;
            
            /* loop through the pages to map in, adding page table entries */
            for(pgloop = 0; pgloop < req->size; pgloop += MEM_PGSIZE)
            {
               if(pg_add_4K_mapping(current->proc->pgdir,
                                    (unsigned int)req->vaddr + pgloop,
                                    (unsigned int)req->paddr + pgloop,
                                    pgflags))
               {
                  unlock_gate(&(current->proc->lock), LOCK_WRITE);
                  SYSCALL_RETURN(e_failure);
               }
            }
                        
            /* tell the processor to reload the process's page tables 
               and warn other cores running this driver process */
            x86_load_cr3(KERNEL_LOG2PHYS(current->proc->pgdir));
            mp_interrupt_process(current->proc, INT_IPI_FLUSHTLB);
            
            unlock_gate(&(current->proc->lock), LOCK_WRITE);
            
            SYSCALL_DEBUG("[sys:%i] successfully mapped physical %p to logical %p size %i bytes\n",
                          CPU_ID, req->paddr, req->vaddr, req->size);
            SYSCALL_RETURN(success);
            
         }
         else SYSCALL_RETURN(e_no_rights);
         
      case DIOSIX_DRIVER_REGISTER_IRQ:
         if(current->flags & THREAD_FLAG_ISDRIVER)
         {
            kresult err;
            unsigned char irq = regs->ebx;
            
            /* let the IRQ code take care of it including outputting debug */
            err = irq_register_driver(irq, IRQ_DRIVER_PROCESS, current->proc, NULL);
            SYSCALL_RETURN(err);
         }
         
      case DIOSIX_DRIVER_DEREGISTER_IRQ:
         if(current->flags & THREAD_FLAG_ISDRIVER)
         {
            kresult err;
            unsigned char irq = regs->ebx;
            
            /* let the IRQ code take care of it including outputting debug */
            err = irq_deregister_driver(irq, IRQ_DRIVER_PROCESS, current->proc, NULL);
            SYSCALL_RETURN(err);
         }
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}

/* syscall:info - read information about a thread/process/system
   => eax = DIOSIX_THREAD_INFO: read info about the currently running thread
            DIOSIX_PROCESS_INFO: read info about the currently running process
            DIOSIX_KERNEL_INFO: read info about the currently running kernel
      ebx = pointer to empty diosix_thread_info/diosix_process_info/diosix_kernel_info
            structure for kernel to fill in
   <= eax = 0 for succes or an error code
*/
void syscall_do_info(int_registers_block *regs)
{
   diosix_info_block *block = (diosix_info_block *)(regs->ebx);
   thread *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_INFO(%i) called by process %i (thread %i)\n",
                 CPU_ID, regs->eax, current->proc->pid, current->tid);

   /* sanity check pointer for badness */
   if(!block || (regs->ebx + MEM_CLIP(block, sizeof(diosix_info_block))) > KERNEL_SPACE_BASE)
      SYSCALL_RETURN(e_bad_address);
   
   /* fill out the block, a bad pointer will page fault in the caller's virtual space */
   switch(regs->eax)
   {
      case DIOSIX_THREAD_INFO:
         block->data.t.tid      = current->tid;
         block->data.t.cpu      = current->cpu;
         block->data.t.priority = current->priority;
         SYSCALL_RETURN(success);
         
      case DIOSIX_PROCESS_INFO:
         block->data.p.pid       = current->proc->pid;
         block->data.p.parentpid = current->proc->parentpid;
         block->data.p.flags     = current->proc->flags;
         block->data.p.privlayer = current->proc->layer;
         SYSCALL_RETURN(success);
         
      /* these are defined externally in the makefile */
      case DIOSIX_KERNEL_INFO:
      {
         vmm_memcpy(&(block->data.k.identifier), KERNEL_IDENTIFIER, vmm_nullbufferlen(KERNEL_IDENTIFIER));
         block->data.k.release_major       = KERNEL_RELEASE_MAJOR;
         block->data.k.release_minor       = KERNEL_RELEASE_MINOR;
         block->data.k.kernel_api_revision = KERNEL_API_REVISION;
         SYSCALL_RETURN(success);
      }
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}
