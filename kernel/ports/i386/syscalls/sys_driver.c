/* kernel/ports/i386/syscalls/sys_driver.c
 * i386-specific software interrupt handling for interrupts
 * Author : Chris Williams
 * Date   : Sat,29 Nov 2009.18:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>


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
            DIOSIX_DRIVER_IOREQUEST: access an IO port, privileged drivers only
               => ebx = pointer to IO port request block
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
            
            /* update the thread's priority now */
            sched_move_to_end(current->cpu, current);
            
            SYSCALL_DEBUG("[sys:%i] thread registered as a driver\n", CPU_ID);
            SYSCALL_RETURN(success);
         }
         
      case DIOSIX_DRIVER_DEREGISTER:
         if(current->flags & THREAD_FLAG_ISDRIVER)
         {
            err = x86_ioports_disable(current);
            if(err) SYSCALL_RETURN(err);

            current->flags &= (~THREAD_FLAG_ISDRIVER);
            
            /* update the thread's priority now */
            sched_move_to_end(current->cpu, current);
            
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
            if(vmm_find_vma(current->proc, (unsigned int)req->vaddr, req->size))
               SYSCALL_RETURN(e_vma_exists);
                        
            /* sanatise the settings flags */
            flags = req->flags & (VMA_WRITEABLE | VMA_NOCACHE | VMA_SHARED);
            flags |= VMA_FIXED | VMA_GENERIC; /* do not release the physical page frames */
            
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
         
      case DIOSIX_DRIVER_IOREQUEST:
         if(current->flags & THREAD_FLAG_ISDRIVER)
         {
            kresult err;
            diosix_ioport_request *req = (diosix_ioport_request *)regs->ebx;
            
            /* sanity checks */
            if(!req) SYSCALL_RETURN(e_bad_params);
            if((unsigned int)req + MEM_CLIP(req, sizeof(diosix_ioport_request)) >= KERNEL_SPACE_BASE)
               SYSCALL_RETURN(e_bad_params);
            
            /* check the thread has the correct IO port access */
            err = x86_ioports_check(current->proc, req->port);
            if(err) SYSCALL_RETURN(err);
            
            /* decode the request action */
            switch(req->type)
            {
               case ioport_read:
               {
                  switch(req->size)
                  {
                     case 1:
                        req->data_in = x86_inportb(req->port);
                        break;
                        
                     /* bad read width */
                     default:
                        SYSCALL_RETURN(e_bad_params);
                  }
               }
               break;
                  
               case ioport_write:
               {
                  switch(req->size)
                  {
                     case 1:
                        x86_outportb(req->port, req->data_out);
                        break;
                        
                     /* bad write width */
                     default:
                        SYSCALL_RETURN(e_bad_params);
                  }
               }
               break;
               
               /* bad access type */
               default:
                  SYSCALL_RETURN(e_bad_params);
            }
            
            /* will be success */
            SYSCALL_RETURN(err);
         }
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}
