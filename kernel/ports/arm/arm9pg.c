/* kernel/ports/arm/arm9page.c
 * manipulate page tables for the ARM9 processor
 * Author : Chris Williams
 * Date   : Sun,17 Apr 2011.02:06.00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/


#include <portdefs.h>

/* pg_load_pgdir
   Tell the CPU to use this new page directory
*/
void pg_load_pgdir(void *physaddr)
{
   KOOPS_DEBUG("pg_load_pgdir(%p): not yet implemented\n", physaddr);
   return;
}

unsigned int pg_fault_addr(void)
{
   KOOPS_DEBUG("pg_fault_addr: not yet implemented\n");
   return 0;
}

unsigned char page_fatal_flag = 0; /* set to 1 when handling a fatal kernel fault, to avoid infinite loops */

/* pg_do_fault
   Do the actual hard work of fixing up a thread after a page fault, or is about to cause a page fault
   => target = thread that caused the fault
      faultaddr = virtual address of the fault
      vector = ARM-specific vector number that raised the fault
   <= success or an error code if the fault could not be handled
*/
kresult pg_do_fault(thread *target, unsigned int faultaddr, arm_vector_num vector)
{
   vmm_decision decision;
   unsigned int *pgtable, pgentry, errflags;
   unsigned int pgdir_index = (faultaddr >> PG_DIR_BASE) & PG_INDEX_MASK;
   unsigned int pgtable_index = (faultaddr >> PG_TBL_BASE) & PG_INDEX_MASK;
   unsigned char rw_flag = 0;
   unsigned int cpuflags;
   
   process *proc = target->proc;
   
   /* give up if the user process has tried to access kernel memory */
   if((faultaddr >= KERNEL_SPACE_BASE) && (cpuflags & PG_FAULT_U))
      return e_bad_address;

   /* look up the entry for this faulting address in the user page tables */
   pgtable = (unsigned int *)((unsigned int)proc->pgdir[pgdir_index] & PG_4K_MASK);
   if(pgtable)
   {
      pgtable = KERNEL_PHYS2LOG(pgtable);
      pgentry = pgtable[pgtable_index];
   }
   else
      pgentry = 0;
   
   /* inspect the page table flags */
   if(pgentry & PG_EXTERNAL)
      goto pg_fault_external;
   
   /* convert x86 page fault flags into generic vmm flags */
   if(cpuflags & PG_FAULT_W)
      errflags = VMA_WRITEABLE;
   else
      errflags = VMA_READABLE;
   
   /* hint to the vmm that the page already has physical memory */
   if(pgentry & PG_PRESENT)
      errflags |= VMA_HASPHYS;
   
   /* ask the vmm for a decision */
   decision = vmm_fault(proc, faultaddr, errflags, &rw_flag);
   
   /* use to mark new pages as read-only or read-write */
   if(rw_flag) rw_flag = PG_RW;
   
   switch(decision)
   {
      case newsharedpage:
      {
         unsigned int physical = 0;
         unsigned int offset;
         vmm_area_mapping *search = NULL;
         vmm_tree *node = vmm_find_vma(proc, faultaddr, sizeof(char));
         
         /* each process will map a vma at potentially different virtual addresses
            so resolve the faulting address into an offset from a vma mapping base */
         offset = faultaddr - node->base;

         /* scan through a vma's process mappings to find a physical page */
         for(;;)
         {
            search = vmm_next_in_pool(search, node->area->mappings);
            if(search)
            {
               /* don't bother checking this current process for a present mapping */
               if(search->proc != proc)
                  if(pg_user2phys(&physical, search->proc->pgdir, search->base + offset) == success)
                     break;
            }
            else break;
         }
         
         /* if physical is still zero then a physical page must be found for all processes */
         if(!physical)
         {
            if(vmm_req_phys_pg((void **)&physical, 1))
               return e_failure; /* bail out if we can't get a phys page */
            
            /* run back through all the processes linked to the vma... */
            search = NULL;
            for(;;)
            {
               search = vmm_next_in_pool(search, node->area->mappings);
               if(search)
               {
                  /* ...and map the physical page in for them all right now */
                  pg_add_4K_mapping(search->proc->pgdir, (search->base + offset) & PG_4K_MASK,
                                    physical, PG_PRESENT | rw_flag | PG_PRIVLVL | PG_PRIVATE);
                  
                  if(search->proc == proc)
                     /* tell this processor to reload the page tables */
                     pg_load_pgdir(KERNEL_LOG2PHYS(proc->pgdir));
                  else
                     /* warn any cores running the process of the page table changes */
                    mp_interrupt_process(search->proc, INT_IPI_FLUSHTLB);
               }
               else break;
            }
         }
         else
         {
            /* found an existing physical page from another process so map it in */
            pg_add_4K_mapping(proc->pgdir, faultaddr & PG_4K_MASK,
                              physical, PG_PRESENT | rw_flag | PG_PRIVLVL);
            
            /* tell the processor to reload the page tables */
            pg_load_pgdir(KERNEL_LOG2PHYS(proc->pgdir));
         }
         
         return success;
      }
         
      case newpage:
      { 
         unsigned int new_phys;
         
         /* grab a new (blank) physical page */
         if(vmm_req_phys_pg((void **)&new_phys, 1))
            return e_failure; /* bail out if we can't get a phys page */
         
         /* map this new physical page in, remembering to set write access */
         pg_add_4K_mapping(proc->pgdir, faultaddr & PG_4K_MASK,
                           new_phys, PG_PRESENT | rw_flag | PG_PRIVLVL | PG_PRIVATE);
         
         /* tell the processor to reload the page tables */
         pg_load_pgdir(KERNEL_LOG2PHYS(proc->pgdir));
         
         PAGE_DEBUG("[page:%i] mapped new page for process %i: virtual %x -> physical %x\n",
                    CPU_ID, proc->pid, faultaddr & PG_4K_MASK, new_phys);
         
         return success;
      }
         
      case clonepage:
      {
         unsigned int new_phys, new_virt, source_virt;
         
         /* grab a new (blank) physical page */
         if(vmm_req_phys_pg((void **)&new_phys, 1))
            return e_failure; /* bail out if we can't get a phys page */
         
         /* copy physical page to another via kernel virtual addresses */
         new_virt = (unsigned int)KERNEL_PHYS2LOG(new_phys);
         source_virt = (unsigned int)KERNEL_PHYS2LOG(pgentry & PG_4K_MASK);
         vmm_memcpy((unsigned int *)new_virt, (unsigned int *)source_virt, MEM_PGSIZE);
         
         /* map this new physical page in, remembering to set write access */
         pg_add_4K_mapping(proc->pgdir, faultaddr & PG_4K_MASK,
                           new_phys, PG_PRESENT | rw_flag | PG_PRIVLVL | PG_PRIVATE);
         
         /* tell the processor to reload the page tables */
         pg_load_pgdir(KERNEL_LOG2PHYS(proc->pgdir));
         
         PAGE_DEBUG("[page:%i] cloned page for process %i: virtual %x -> physical %x\n",
                    CPU_ID, proc->pid, faultaddr & PG_4K_MASK, new_phys);
         
         return success;
      }
         
      case makewriteable:
      {            
         /* it's safe to just set write access on this page */
         pg_add_4K_mapping(proc->pgdir, faultaddr & PG_4K_MASK,
                           pgentry & PG_4K_MASK, PG_PRESENT | rw_flag | PG_PRIVLVL | PG_PRIVATE);
         
         /* tell the processor to reload the page tables */
         pg_load_pgdir(KERNEL_LOG2PHYS(proc->pgdir));
         
         PAGE_DEBUG("[page:%i] made page writeable for process %i: virtual %x -> physical %x\n",
                    CPU_ID, proc->pid, faultaddr & PG_4K_MASK, pgentry & PG_4K_MASK);
         
         return success;
      }
         
      case external:
         goto pg_fault_external;
         
      case badaccess:
         PAGE_DEBUG("[page:%i] can't handle fault at %x for process %i\n",
                    CPU_ID, faultaddr, proc->pid);
   }

   /* fall through to indicating a run-time error caused the fault */
   return e_failure;
   
pg_fault_external:
   PAGE_DEBUG("[page:%i] delegating fault at %x for process %i to userspace page manager\n",
              CPU_ID, faultaddr, proc->pid);
   return e_failure;
}

/* pg_fault
   Handle an incoming page fault raised by a cpu
   => regs = faulting thread's state
   <= success if the fault was handled or a failure code if it couldn't be
*/
kresult pg_fault(int_registers_block *regs)
{
   unsigned int faultaddr = pg_fault_addr();
   
   /* give up if we're in early system initialisation */
   if(!cpu_table) goto pf_fault_bad;
   if(!(cpu_table[CPU_ID].current)) goto pf_fault_bad;
   
   /* get the fault handled */
   if(pg_do_fault(cpu_table[CPU_ID].current, faultaddr, regs->vector) == success)
      return success; /* job done */

pf_fault_bad:
   /* give up completely if the kernel's faulting within its own space */
   if((regs->pc >= KERNEL_SPACE_BASE) && (faultaddr >= KERNEL_SPACE_BASE))
   {
      /* dump details about the fault */
      if(!page_fatal_flag)
      {
         page_fatal_flag = 1;
         pg_postmortem(regs);
         pg_dump_pagedir((unsigned int *)(cpu_table[CPU_ID].current->proc->pgdir));
         debug_stacktrace();
      }
      debug_panic("unhandled serious page fault in the kernel");
      /* shouldn't return */
   }
   
   /* fall through to indicating that the process has made a run-time error */
   return e_failure;
}

/* pg_preempt_fault
   The kernel's been supplied an address by a user thread and is about to access it.
   Sanity check the address and attempt to fix-up the fault now, if possible.
   => test = pointer to thread to test
      virtualaddress = thread's supplied virtual address
      size = range in bytes to test from the address
      flags = access type flags (VMA_WRITEABLE set to indicate a write or clear for a read)
   <= success or a failure code
*/
kresult pg_preempt_fault(thread *test, unsigned int virtualaddr, unsigned int size, unsigned int flags)
{
   unsigned int virtualloop, virtual_aligned_min, virtual_aligned_max;
   unsigned int pgdir_index, pgtable_index, *pgtbl;
   unsigned int **pgdir;
   unsigned int page_write_flag = 0;
   
   /* simple params check - zero page is always invalid */
   if(!test || !size || !virtualaddr) return e_bad_params;
   
   if((virtualaddr + MEM_CLIP(virtualaddr, size)) >= KERNEL_SPACE_BASE)
      return e_bad_address;
   
   virtual_aligned_min = (unsigned int)MEM_PGALIGN(virtualaddr);
   virtual_aligned_max = (unsigned int)MEM_PGALIGN(virtualaddr + size);
   
   pgdir = test->proc->pgdir;
   
   if(flags & VMA_WRITEABLE) page_write_flag = PG_FAULT_W;
   
   /* run through all the pages in this range of address to check */
   for(virtualloop = virtual_aligned_min; virtualloop <= virtual_aligned_max; virtualloop += MEM_PGSIZE)
   {
      pgdir_index = (virtualloop >> PG_DIR_BASE) & PG_INDEX_MASK;
      pgtable_index = (virtualloop >> PG_TBL_BASE) & PG_INDEX_MASK;

      /* get the page table entry for this virtual address */
      pgtbl = (unsigned int *)((unsigned int)pgdir[pgdir_index] & PG_4K_MASK);
      
      if(pgtbl)
      {
         pgtbl = KERNEL_PHYS2LOG(pgtbl);
         
         /* is the page present? */
         if(!(pgtbl[pgtable_index] & PG_PRESENT))
            if(pg_do_fault(test, virtualloop, PG_FAULT_U | page_write_flag))
               return e_bad_address;
         
         /* is the page write-protected and we want to do a write? */
         if((!(pgtbl[pgtable_index] & PG_RW)) && page_write_flag)
            if(pg_do_fault(test, virtualloop, PG_FAULT_P | PG_FAULT_U | page_write_flag))
               return e_bad_address;
      }
   }
   
   /* fall through to returning success */
   return success;
}

/* pg_clone_pgdir
   Create a page directory based on another dir and any pages tables too -
   maintaining links to read-only data 
   => source = pointer to page directory array for the pgdir we want to clone
   <= returns pointer to new page directory array, or NULL for failure
 */
unsigned int **pg_clone_pgdir(unsigned int **source)
{
   unsigned int loop;
   unsigned int source_touched = 0;
   unsigned int **new = NULL;

   /* ask the vmm for a physical page */
   if(vmm_req_phys_pg((void **)&new, 1))
      return NULL; /* bail out if we can't get a phys page */
   
   new = (unsigned int **)KERNEL_PHYS2LOG(new);
   
   /* link the kernelspace mappings */
   for(loop = (KERNEL_SPACE_BASE >> PG_DIR_BASE); loop < 1024; loop++)
      new[loop] = source[loop];
   
   /* link read-only user areas, link r/w user areas but mark them
       to copy-on-write and disable the write bit - we'll fault on
       write and then the fault handler can demand copy the pages.
       also, it's assumed all userspace pages are handled as 4K entries */
   for(loop = 0; loop < (KERNEL_SPACE_BASE >> PG_DIR_BASE); loop++)
   {
      if(source[loop])
      {
         unsigned *pgtable_phys, *pgtable, *src_table = source[loop];
         unsigned int page;
         
         /* make a new copy of the page table */
         if(vmm_req_phys_pg((void **)&pgtable_phys, 1))
            return NULL; /*bail out if we can't get a phys page */
      
         /* calc the logical addresses, strip flags and start modifying the new table */
         pgtable = (unsigned int *)(KERNEL_PHYS2LOG(pgtable_phys));
         src_table = (unsigned int *)((unsigned int)KERNEL_PHYS2LOG(src_table) & PG_4K_MASK);
         
         for(page = 0; page < 1024; page++)
         {
            pgtable[page] = src_table[page];
            
            /* if page is writeable, then disable writing so we can later fault and
               perform the copy-on-write */
            if(pgtable[page] & PG_RW)
            {
               /* in the child */
               pgtable[page] = pgtable[page] & (~(PG_RW | PG_PRIVATE)); /* clear R/W + private flags */
               
               /* in the parent */
               src_table[page] = src_table[page] & (~(PG_RW | PG_PRIVATE)); /* clear R/W + private flags */
               source_touched = 1;
            }
         }
         
         /* save the phys address into the directory with suitable flags */
         new[loop] = (unsigned int *)((unsigned int)pgtable_phys | ((unsigned int)(source[loop]) & ~PG_4K_MASK));
      }
      else
         new[loop] = NULL;
   }
   
   /* if the parent page tables were altered then the core running it should reload 
       it page directory */
   if(source_touched)
   {
      if(cpu_table[CPU_ID].current->proc->pgdir == source)
         pg_load_pgdir(KERNEL_LOG2PHYS(source));
   }
   
   return new;
}

/* pg_new_process
 Called when a new process is being created so port-specific stuff
 can take place. We'll base 
 => new = pointer to process structure
 current = pointer or NULL for initial system processes
 <= 0 for success or an error code
 */
kresult pg_new_process(process *new, process *current)
{
   unsigned int **pgdir;

   lock_gate(&(new->lock), LOCK_WRITE);
   
   /* pick the right page directory */
   if(current)
   {
      lock_gate(&(current->lock), LOCK_READ);
      pgdir = current->pgdir;
   }
   else
      pgdir = (unsigned int **)KernelPageDirectory;
   
   /* grab a copy of the parent page directory or
    bail out if there's a failure. if there is no
    parent, we're create new processes from scratch
    and the kernel's page directory should be empty
    for userspace... */
   new->pgdir = pg_clone_pgdir(pgdir);
   if(new->pgdir == NULL)
   {
      unlock_gate(&(new->lock), LOCK_WRITE);
      if(current) unlock_gate(&(current->lock), LOCK_READ);
      return e_failure;
   }

   if(current) unlock_gate(&(current->lock), LOCK_READ);
   unlock_gate(&(new->lock), LOCK_WRITE);
   
   /* notify the userspace page manager that a process is starting up */
   if(current && proc_role_lookup(DIOSIX_ROLE_PAGER))
   {
      if(msg_send_signal(proc_role_lookup(DIOSIX_ROLE_PAGER), NULL, SIGXPROCCLONED, new->pid))
      {
         KOOPS_DEBUG("[page:%i] OMGWTF userspace page manager has gone AWOL\n"
                     "          tried talking to proc %i (%p) while cloning %i (%p)\n",
                     CPU_ID, proc_role_lookup(DIOSIX_ROLE_PAGER)->pid, proc_role_lookup(DIOSIX_ROLE_PAGER),
                     new->pid, new);
      }
   }   
   
   PAGE_DEBUG("[page:%i] created new pgdir %p for new process %i\n", 
           CPU_ID, new->pgdir, new->pid);
   
   return success;
}

/* pg_destroy_process
   Called to deconstruct a process's memory map by releasing
   the pages holding the page directory and page tables.
   => victim = process to tear down
   <= success or e_failure if something went wrong
*/
kresult pg_destroy_process(process *victim)
{
   unsigned int loop;
   unsigned int **pgdir;
   
   lock_gate(&(victim->lock), LOCK_WRITE);
   
   pgdir = victim->pgdir;
   
   /* run through the userspace of the page directory */
   for(loop = 0; loop < (KERNEL_SPACE_BASE >> PG_DIR_BASE); loop++)
   {
      if((unsigned int)(pgdir[loop]) & PG_PRESENT)
         /* return the page holding the table */
         vmm_return_phys_pg((unsigned int *)((unsigned int)(pgdir[loop]) & PG_4K_MASK));
   }
   
   /* return the page dir's page */
   vmm_return_phys_pg(KERNEL_LOG2PHYS(victim->pgdir));
   victim->pgdir = NULL;
   
   /* bump the userspace page manager */
   if(proc_role_lookup(DIOSIX_ROLE_PAGER))
   {
      if(msg_send_signal(proc_role_lookup(DIOSIX_ROLE_PAGER), NULL, SIGXPROCKILLED, victim->pid))
      {
         KOOPS_DEBUG("[page:%i] OMGWTF userspace page manager has gone AWOL\n"
                     "       tried signalling proc %i (%p) while tearing down %i (%p)\n",
                     CPU_ID, proc_role_lookup(DIOSIX_ROLE_PAGER)->pid, proc_role_lookup(DIOSIX_ROLE_PAGER),
                     victim->pid, victim);
      }
   }
   
   unlock_gate(&(victim->lock), LOCK_WRITE);
                     
   return success;
}

/* pg_user2phys
   Translate a userspace address in a given process into a physical
   address - if the physical page doesn't exist, then an error is
   returned
   => paddr = pointer to word to fill in with the physical address
      pgdir = pgdir belonging to a process
      virtual = virtual address to translate
   <= success or failure code
*/
kresult pg_user2phys(unsigned int *paddr, unsigned int **pgdir, unsigned int virtual)
{
   unsigned int pgdir_index = (virtual >> PG_DIR_BASE) & PG_INDEX_MASK;
   unsigned int pgtable_index = (virtual >> PG_TBL_BASE) & PG_INDEX_MASK;
   unsigned int *pgtbl;
   
   if((!pgdir) || (!paddr)) return e_failure;
   
   pgtbl = (unsigned int *)((unsigned int)pgdir[pgdir_index] & PG_4K_MASK);
   
   if(pgtbl)
   {
      pgtbl = KERNEL_PHYS2LOG(pgtbl);

      if(pgtbl[pgtable_index] & PG_PRESENT)
      {
         *(paddr) = (pgtbl[pgtable_index] & PG_4K_MASK) + (virtual & ~PG_4K_MASK);
         return success;
      }
   }

   return e_not_found;
}

/* pg_user2kernel
   Translate a userspace virtual address into a physical address
   and then resolve into a kernel virtual address so it can be
   accessed by the kernel - this will only work on present pages
   => kaddr = pointer to 32bit word to store the final kernel
              address in
      uaddr = userspace address to translate
      proc  = process owning the userspace to interrogate
   <= 0 for success or an error code
*/
kresult pg_user2kernel(unsigned int *kaddr, unsigned int uaddr, process *proc)
{
   if((!proc) || (!kaddr)) return e_failure;
   
   if(pg_user2phys(kaddr, proc->pgdir, uaddr))
      return e_failure;

   /* got a physical addr, turn it into a kernel virtual address */
   *(kaddr) = (unsigned int)KERNEL_PHYS2LOG((unsigned int *)(*(kaddr)));
   
   return success;
}

/* pg_remove_4K_mapping
   Remove a 4K mapping from a page directory (if present). Release the page to the free stack
   if it is marked private and release_flag is zero
   => pgdir pointer to page directory to remove the 4K mapping from
      virtual = the full virtual address for the start of the 4K page
      release_flag = 0 to keep it from the free stack or 1 to obey the PG_PRIVATE flag
   <= 0 for success or an error code
*/
kresult pg_remove_4K_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int release_flag)
{
   unsigned int pgdir_index = (virtual >> PG_DIR_BASE) & PG_INDEX_MASK;
   unsigned int pgtable_index = (virtual >> PG_TBL_BASE) & PG_INDEX_MASK;
   unsigned int *pgtbl, physical;
   
   if(!pgdir || ((unsigned int)pgdir < KERNEL_SPACE_BASE))
   {
      KOOPS_DEBUG("[page:%i] OMGWTF bad page directory pointer to pg_remove_4K_mapping!\n"
                  "              pgdir %p virtual %x\n"
                  "              pgdir_index %i pgtable_index %i\n",
                  CPU_ID, pgdir, virtual, pgdir_index, pgtable_index);
      debug_stacktrace();
      return e_failure; /* bail out now if we get a bad pointer */
   }
   
   /* if a 4M mapping already exists for this virtual address then bail out */
   if((unsigned int)(pgdir[pgdir_index]) & PG_SIZE)
   {
      KOOPS_DEBUG("[page:%i] OMGWTF tried to remove a 4M page entry in pg_remove_4K_mapping!\n"
                  "              pgdir %p virtual %x\n"
                  "              pgdir_index %i pgtable_index %i\n",
                  CPU_ID, pgdir, virtual, pgdir_index, pgtable_index);
      debug_stacktrace();
      return e_failure;
   }
   
   /* get the page table entry */
   pgtbl = (unsigned int *)KERNEL_PHYS2LOG(pgdir[pgdir_index]);
   pgtbl = (unsigned int *)((unsigned int)pgtbl & PG_TBL_MASK); /* clean out lower bits */
   physical = pgtbl[pgtable_index];
   
   /* was the page ever present? */
   if(physical & PG_PRESENT)
   {
      /* set page as not present and strip out the physical address */
      pgtbl[pgtable_index] &= (~PG_TBL_MASK & ~PG_PRESENT); 

      /* mask off all the low bits in the table entry to get the physical address */
      physical &= PG_TBL_MASK;

      PAGE_DEBUG("[page:%i] removing 4K page at %x in pd %p, freeing physical frame %x\n",
                 CPU_ID, virtual, pgdir, physical);
      
      /* only obey the PG_PRIVATE flag if release_flag is set */
      if(release_flag && (pgtbl[pgtable_index] & PG_PRIVATE))
         return vmm_return_phys_pg((void *)physical); /* release the physical frame */
      else
         return success;
   }
   
   return success;
}

/* pg_add_4K_mapping
   Add or edit an existing 4K mapping to a page directory, allocating a
   new page table if needed.
   => pgdir = pointer to page directory to add the 4K mapping to
      virtual = the full virtual address for the start of the 4K page
      physical = the full physical address of the page frame
      flags = flag bits to be ORd with the phys addr
   <= 0 for success, anything else is a fail
*/
kresult pg_add_4K_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int physical, 
                          unsigned int flags)
{
   unsigned int pgdir_index = (virtual >> PG_DIR_BASE) & PG_INDEX_MASK;
   unsigned int pgtable_index = (virtual >> PG_TBL_BASE) & PG_INDEX_MASK;
   unsigned int *pgtbl;

   if(!pgdir || ((unsigned int)pgdir < KERNEL_SPACE_BASE))
   {
      KOOPS_DEBUG("[page:%i] OMGWTF bad page directory pointer to pg_add_4K_mapping!\n"
                  "              pgdir %p virtual %x physical %x flags %i\n"
                  "              pgdir_index %i pgtable_index %i\n",
                  CPU_ID, pgdir, virtual, physical, flags, pgdir_index, pgtable_index);
      debug_stacktrace();
      return e_failure; /* bail out now if we get a bad pointer */
   }
   
   /* if a 4M mapping already exists for this virtual address then bail out */
   if((unsigned int)(pgdir[pgdir_index]) & PG_SIZE)
      return success;

   /* find the entry in the page directory for the page table for this 4K page and
      allocate a page table if it doesn't exist */
   if(!(pgdir[pgdir_index]))
   {
      unsigned int loop;
      unsigned int *newtable, *cleantable;
      
      kresult err = vmm_req_phys_pg((void **)&newtable, 1);
      if(err) return err; /* failed */
         
      PAGE_DEBUG("[page:%i] new page table created: %p\n", CPU_ID, newtable);

      /* PLEASE PLEASE don't forget that all kernel addresses are
         logical! So you must convert phys to log before accessing
         the referenced memory */
      /* zero the new page table */
      cleantable = (unsigned int *)KERNEL_PHYS2LOG(newtable);
      for(loop = 0; loop < 1024; loop++)
         cleantable[loop] = 0;
      
      /* write into the page directory entry the new tbl addr and flags */
      /* force the r/w bit high so that if a particular page is marked read-only
         the whole directory entry isn't */
      pgdir[pgdir_index] = (unsigned int *)((unsigned int)newtable | PG_RW | flags);
   }
   
   /* align address and clean lower bits */
   physical = physical & PG_4K_MASK;
   
   PAGE_DEBUG("[page:%i] mapping 4K: %x -> %x (%x) dir index %x table index %x table base %p (%x)\n", 
           CPU_ID, virtual, physical, flags, pgdir_index, pgtable_index,
           pgdir[pgdir_index], KERNEL_PHYS2LOG(pgdir[pgdir_index]));
   
   pgtbl = (unsigned int *)KERNEL_PHYS2LOG(pgdir[pgdir_index]);
   pgtbl = (unsigned int *)((unsigned int)pgtbl & PG_TBL_MASK); /* clean out lower bits */
   
   pgtbl[pgtable_index] = physical | flags;
   
   return 0;
}

/* pg_add_4M_mapping
 Add or edit an existing 4M mapping to a page directory
 => pgdir = pointer to page directory to add the 4K mapping to
    virtual = the full virtual address for the start of the 4K page
    physical = the full physical address of the page frame
    flags = flag bits to be ORd with the phys addr
 <= 0 for success, anything else is a fail
 */
kresult pg_add_4M_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int physical, 
                          unsigned int flags)
{   
   PAGE_DEBUG("[page:%i] mapping 4M: %x -> %x (%x) dir index %x\n", 
           CPU_ID, virtual, physical, flags, virtual >> PG_DIR_BASE);
   
   virtual = virtual & PG_4M_MASK;
   physical = physical & PG_4M_MASK;
   
   /* create 4MB entry, read+write for kernel-only */
   if((virtual >> PG_DIR_BASE) < 1024)
   {
      unsigned int entry = (unsigned int)(pgdir[(virtual >> PG_DIR_BASE)]);
      
      /* give up if this entry is taken by a 4K mapping entry */
      if(entry != 0)
         if((entry & PG_SIZE) == 0)
            return e_failure;
      
      pgdir[(virtual >> PG_DIR_BASE)] = (unsigned int *)(physical | PG_SIZE | flags);
      return success;
   }
   
   /* fall through to failure */
   return e_failure;
}

/* pg_map_phys_to_kernel_space
   Read through a physical page frame stack and add entries to the kernel's
   virtual space.
   => base = base of descending stack
      top = last entry in stack
      granularity = 0 to add 4KB page entries, 1 to add 4M page entries
*/
void pg_map_phys_to_kernel_space(unsigned int *base, unsigned int *top, unsigned char granularity)
{
   unsigned int **pg_dir = (unsigned int **)KernelPageDirectory; /* from start.s */
   kresult err;

   /* we must assume the physical page stacks are full - ie: no one has
      popped any stack frames off them. and we have to assume for now that
      the frames are in ascending order. scan through the stacks and map
      frames into the kernel's virtual space. */
   while(base >= top)
   {
      unsigned int addr, logical_addr;
      unsigned char this_granularity = granularity;
      
      if(this_granularity == 1)
         addr = *base & PG_4M_MASK;
      else
         addr = *base & PG_4K_MASK; 

      /* perform the page table scribbling */
      if(this_granularity == 1)
      {
         logical_addr = (unsigned int)KERNEL_PHYS2LOG((unsigned int)addr);
         
         PAGE_DEBUG("[page:%i] mapped to kernel 4M, page dir[%i] = %x (%x) (%x)\n",
                 CPU_ID, (logical_addr >> PG_DIR_BASE), addr, logical_addr, pg_dir);
         
         /* create 4MB entry, read+write for kernel-only */
         pg_add_4M_mapping(pg_dir, logical_addr, addr, PG_PRESENT | PG_RW);
         
         /* skip to next 4M boundary aka next */
         while(base >= top)
         {
            base--;
            if((*base & PG_4M_MASK) != addr) break;
         }
      }
      else
      {

         PAGE_DEBUG("[page:%i] mapped to kernel 4K, phys %x log %x (%x)\n",
                 CPU_ID, *base, KERNEL_PHYS2LOG(*base), pg_dir);
                     
         /* create a 4KB entry, read+write for kernel-only */
         err = pg_add_4K_mapping((unsigned int **)pg_dir, (unsigned int)KERNEL_PHYS2LOG(*base),
                                 *base, PG_PRESENT | PG_RW);
          if(err)
          {
             PAGE_DEBUG("*** failed to map virtual %x to physical %x into kernel! halting.",
                     (unsigned int)KERNEL_PHYS2LOG(*base), *base);
             while(1);
          }

         base--;
      }
   }
}

/* pg_init
   Start up the underlying pagination system for the vmm. This includes
   mapping as much physical ram into the kernel's virtual space as possible.
*/
void pg_init(void)
{
   /* grab a copy of the physical page stack limits because calls to
      pg_map_phys_to_kernel_space() will modify these global variables */
   unsigned int *low_base = phys_pg_stack_low_base;
   unsigned int *low_ptr = phys_pg_stack_low_ptr;
   unsigned int *high_base = phys_pg_stack_high_base;
   unsigned int *high_ptr = phys_pg_stack_high_ptr;
   
   unsigned int loop;
   unsigned int **kernel_dir = (unsigned int **)KernelPageDirectory;
   
   PAGE_DEBUG("[page:%i] initialising.. kernel page dir %x\n", CPU_ID, KernelPageDirectory);

   /* clear out all non-kernelspace entries to start again */
   for(loop = 0; loop < (KERNEL_SPACE_BASE >> PG_DIR_BASE); loop++)
      kernel_dir[loop] = NULL;
   
   /* the order of this is important: mapping in 4K pages means page tables
       will have to be found and written in. The physical pages holding these
       tables are unlikely to be mapped in unless we process the upper
       memory first, which are mapped in as 4M pages requiring no separate
       tables */
   /* map most of memory into 4M pages */
   pg_map_phys_to_kernel_space(high_base, high_ptr, 1);
   
   /* ensure the kernel critical area is mapped in - we use 4M pages to 
      maximise TLB performance */
   for(loop = KERNEL_CRITICAL_BASE; loop < (unsigned int)KERNEL_CRITICAL_END; loop += MEM_4M_PGSIZE)
      pg_add_4M_mapping(kernel_dir, (unsigned int)KERNEL_PHYS2LOG(loop), loop, PG_PRESENT | PG_RW);
   
   /* map the rest of the lowest 16M in 4K pages */
   pg_map_phys_to_kernel_space(low_base, low_ptr, 0);
   
   /* notify cpu of change in kernel directory */
   pg_load_pgdir(KERNEL_LOG2PHYS(KernelPageDirectory));
   BOOT_DEBUG("[page:%i] vmm initialised\n", CPU_ID);
}


/* pg_dump_pagedir
   For debug purposes, dump a copy of the given page directory */
void pg_dump_pagedir(unsigned int *pgdir)
{
   unsigned int i;
   
   if(!pgdir) return;
   
   PAGE_DEBUG("[page:%i] contents of pgdir at %p (%x)\n", 
           CPU_ID, pgdir, KERNEL_LOG2PHYS(pgdir));
   
   for(i = 0; i < 1024; i++)
      if(pgdir[i])
      {
         PAGE_DEBUG("       kernel_dir[%i] = %x\n", i, pgdir[i]);
      }
}

/* pg_postmortem
 Perform a diagnostic dump if a page fault can't be handled */
void pg_postmortem(int_registers_block *regs)
{
   KOOPS_DEBUG("pg_postmortem: not implemented yet\n");
}

