/* kernel/ports/arm/armv5pg.c
 * manipulate page tables for ARM v5-compatible processors
 * Author : Chris Williams
 * Date   : Sun,17 Apr 2011.02:06.00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

/* the management of the paging system is slightly different to more conventional 
   setups (ie: the i386_pc tree). the ARMv5 level 1 page directory is 16K in 
   size as it contains 4096 entries, each entry is 32bit wide and represents
   a 1M section of virtual memory, totalling a 4G address space. This page directory
   must therefore be contiguous in physical memory and sit on a 16K boundary.
 
   it may not be possible to request a 16K contig block of phys memory from the
   VMM-managed free page stack so the page code must maintain an array of four
   pointers to each of the 4K pages that make up the page directory. these are
   lazily copied into the kernel's master page directory after a context switch.
   the per-process pg_process structure maintains this array and is created in
   pg_clone_pgdir().
 
   the kernel core, however, doesn't expect this and passes an unsigned int ** 
   pointer stashed in the thread's process structure to the paging code. this
   needs to be recast into a pg_process structure before it's operated on.
 
   next, the level 2 page tables are 1K in size, being 256 entries of 32bit words
   with each entry repesenting 4K, totalling an address space size of 1M. The
   lvl 2 page tables must be 1K boundary aligned. These are allocated by grabbing
   a 4K physical page and splitting it into 4 1K blocks and managing their allocation
   in the per-process pg_process structure.
*/

#include <portdefs.h>

/* ----------------------------------------------------------
                  level 1 page directory handling
   ---------------------------------------------------------- */

/* pg_lvl1_entry_address
   Calculate the kernel virtual address for the entry in the process's
   level 1 page directory for the given virtual address
   => proc = process to inspect
      virtual = address to look up
   <= kernel virtual address, or NULL for failure
*/
unsigned int *pg_lvl1_entry_address(process *proc, unsigned int virtual)
{
   /* assumes proc and virtual are sane */
   
   pg_process *pg_dir = (pg_process *)proc->pgdir;
   unsigned int *phys_addr = pg_dir->phys_frames[PG_LVL1_FRAME(virtual)];
   
   return KERNEL_PHYS2LOG(phys_addr + PG_LVL1_FRAME_ENTRY(virtual));
}

/* pg_lvl1_read
   Read the level 1 page directory entry for the given process and virtual address
   => proc = process to inspect
      virtual = address to look up
      result = pointer to word in which to write the contents of the level
               1 entry for the supplied virtual address
   <= 0 for success, or an error code
*/
kresult pg_lvl1_read(process *proc, unsigned int virtual, unsigned int *result)
{
   /* sanity checks */
   if(!proc || !virtual || !result) return e_bad_params;
   
   /* calculate the virtual address of the required level 1 page table entry,
      and then read the contents and return */
   unsigned int *addr = pg_lvl1_entry_address(proc, virtual);
   if(addr)
   {
      *result = *addr;
      return success;
   }
   
   /* fall through to return an error code */
   return e_failure;
}

/* pg_lvl1_read
   Write a value into the level 1 page directory entry for the given process and virtual address
   => proc = process to inspect
      virtual = address to look up
      data = value to write
   <= 0 for success, or an error code
*/
kresult pg_lvl1_write(process *proc, unsigned int virtual, unsigned int data)
{
   /* sanity checks */
   if(!proc || !virtual) return e_bad_params;
   
   /* calculate the virtual address of the required level 1 page table entry,
      and then write the contents and return */
   unsigned int *addr = pg_lvl1_entry_address(proc, virtual);
   if(addr)
   {
      pg_process *pg_dir = (pg_process *)proc->pgdir;
      
      /* write into the process's entry */
      *addr = data;
      
      /* mark the process's level 1 page directory as updated */
      pg_dir->frames_flags |= PG_LVL1_FRAME_UPDATE(virtual);
      
      return success;
   }
   
   /* fall through to return an error code */
   return e_failure;
}

/* ----------------------------------------------------------
                       page fault handling
   ---------------------------------------------------------- */

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
   dprintf("pg_do_fault: not implemented\n");
   return success;
   
#if 0
   vmm_decision decision;
   unsigned int *pgtable, pgentry, errflags;
   unsigned int pgtable_entry = faultaddr >> PG_1M_SHIFT;
   unsigned int pgtable_index = faultaddr >> PG_4K_SHIFT;
   unsigned char rw_flag = 0;
   unsigned int cpuflags;
   
   process *proc = target->proc;
   
   /* give up if the user process has tried to access kernel memory */
   if((faultaddr >= KERNEL_SPACE_BASE) && (cpuflags & PG_FAULT_U))
      return e_bad_address;

   /* look up the entry for this faulting address in the user page tables */
   pgtable = pg_pgdir_entry_to_vaddr(proc->pg_dir, pgtable_entry);
   if(pgtable)
   {
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
   if(pgentry & PG_RO)
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
                                    physical, PG_RO | rw_flag | PG_PRIVATE);
                  
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
                              physical, PG_RO | rw_flag);
            
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
                           new_phys, PG_RO | rw_flag | PG_PRIVATE);
         
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
                           new_phys, PG_RO | rw_flag | PG_PRIVATE);
         
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
                           pgentry & PG_4K_MASK, PG_RO | rw_flag | PG_PRIVATE);
         
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
#endif
}

/* pg_fault
   Handle an incoming page fault raised by a cpu
   => regs = faulting thread's state
   <= success if the fault was handled or a failure code if it couldn't be
*/
kresult pg_fault(int_registers_block *regs)
{
   dprintf("pg_fault: not implemented\n");
   return success;
   
#if 0
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
#endif
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
   dprintf("pg_preempt_fault: not implemented\n");
   return success;
   
#if 0
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
         if(!(pgtbl[pgtable_index] & PG_RO))
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
#endif
}

/* pg_clone_pgdir
   Create a page directory based on another dir and any pages tables too -
   maintaining links to read-only data 
   => source = pointer to page directory array for the pgdir we want to clone
   <= returns pointer to new page directory array, or NULL for failure
 */
unsigned int **pg_clone_pgdir(unsigned int **source)
{
   dprintf("pg_clone_pgdir: not implemented\n");
   return NULL;
   
#if 0
   unsigned int loop;
   unsigned int source_touched = 0;
   unsigned int **new = NULL;

   new = pg_create_pgdir(source);
   if(!new) return NULL; /* bail out if we can't get a phys page */

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
#endif
}

/* pg_new_process
   Called when a new process is being created so port-specific stuff
   can take place. 
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
   dprintf("pg_destroy_process: not implemented\n");
   return success;
   
#if 0
   unsigned int loop;
   
   lock_gate(&(victim->lock), LOCK_WRITE);
      
   /* destroy the structures describing this process and
      return the page dir's page */
   pg_destroy_pgdir(victim->pgdir);
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
#endif
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
   dprintf("pg_user2phys: not implemented\n");
   return success;
   
#if 0
   unsigned int pgdir_index = virtual >> PG_1M_SHIFT;
   unsigned int pgtable_index = (virtual >> PG_4K_SHIFT) & PG_4K_INDEX_MASK;
   unsigned int *pgtbl;
   
   if((!pgdir) || (!paddr)) return e_failure;
   
   pgtbl = (unsigned int *)((unsigned int)pgdir_index[pgdir_index] & PG_4K_TABLE_MASK);
   
   if(pgtbl)
   {
      pgtbl = KERNEL_PHYS2LOG(pgtbl);

      if((pgtbl[pgtable_index] & PG_L2TYPE_MASK) == PG_L2TYPE_4K)
      {
         *(paddr) = (pgtbl[pgtable_index] & PG_4K_TABLE_MASK) | (virtual & ~PG_4K_TABLE_MASK);
         return success;
      }
   }

   return e_not_found;
#endif
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
   dprintf("pg_user2kernel: not implemented\n");
   return success;
   
#if 0
   if((!proc) || (!kaddr)) return e_failure;
   
   if(pg_user2phys(kaddr, proc->pgdir, uaddr))
      return e_failure;

   /* got a physical addr, turn it into a kernel virtual address */
   *(kaddr) = (unsigned int)KERNEL_PHYS2LOG((unsigned int *)(*(kaddr)));
   
   return success;
#endif
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
   dprintf("pg_remove_4K_mapping: not implemented\n");
   return success;
   
#if 0
   unsigned int *pgtable, physical;
   unsigned int pgdir_index = virtual >> PG_1M_SHIFT;
   unsigned int pgtbl_index = (virtual >> PG_4K_SHIFT) & PG_4K_INDEX_MASK;
   
   PAGE_DEBUG("[page:%i] unmapping 4K: %x release flag %x dir index %x\n", 
              CPU_ID, virtual, release_flag, pgdir_index);

   /* bail out if we're not dealing with a lvl2 page table entry */
   if(((unsigned int)pgdir_index[pgdir_index] & PG_L1TYPE_MASK) != PG_L1TYPE_4KTBL)
      return e_bad_address;
   
   /* mark the 4K page entry as not present (zero bottom two bits and physical addr) */
   pgtable = KERNEL_PHYS2LOG((unsigned int)pgdir_index[pgdir_index] & PG_4K_TABLE_MASK);
   physical = pgtable[pgtbl_index];
   pgtable[pgtbl_index] &= (~PG_4K_MASK & ~PG_L2TYPE_MASK);
   
   /* determine if a page ever existed for this mapping */
   if((physical & PG_L2TYPE_MASK) == PG_L2TYPE_4K)
   {
      /* strip out the non-physical address bits */
      physical &= PG_4K_MASK;
      
      if(release_flag && (pg_read_extrabits(pgdir, virtual) & (PG_PRIVATE >> PG_EXTRA_SHIFT)))
         return vmm_return_phys_pg((void *)physical); /* release the physical frame */
   }
   
   return success;
#endif
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
   dprintf("pg_add_4K_mapping: not implemented\n");
   return success;
   
#if 0
   unsigned int *pgtable, ap;
   unsigned int pgdir_index = virtual >> PG_1M_SHIFT;
   unsigned int pgtbl_index = (virtual >> PG_4K_SHIFT) & PG_4K_INDEX_MASK;
   
   PAGE_DEBUG("[page:%i] mapping 4K: %x -> %x (%x) dir index %x\n", 
              CPU_ID, virtual, physical, flags, pgdir_index);
   
   /* if the level one page directory entry for this virtual address is empty
      then we need to allocate a level two page table */
   if(pgdir_index[pgdir_index] == NULL)
   {
      /* pg_alloc_pgtable() will also clean the array */
      unsigned int pgtbl = pg_alloc_pgtable(pgdir);
      if(!pgtbl) return e_failure;
      
      pgdir_index[pgdir_index] = (unsigned int *)(pgtbl | PG_L1TYPE_4KTBL);
   }
   
   /* bail out if we're not dealing with a lvl2 page table by now */
   if(((unsigned int)pgdir_index[pgdir_index] & PG_L1TYPE_MASK) != PG_L1TYPE_4KTBL)
      return e_bad_address;
   
   /* check to see if we've got extra flags to store. these are passed in the
      high bits of flags but masked out for the actual MMU tables */
   if(flags & PG_4K_MASK)
      pg_store_extrabits(pgdir, virtual, flags >> PG_EXTRA_SHIFT);
   
   /* replicate the access permissions in AP0 into AP1-3 */
   ap = (flags >> PG_4K_AP0_SHIFT) & PG_ACCESS_USER_RW;
   flags |= (ap << PG_4K_AP1_SHIFT) | (ap << PG_4K_AP2_SHIFT) | (ap << PG_4K_AP3_SHIFT);
   
   /* program the 4K page entry for this address into the lvl2 page table */
   pgtable = KERNEL_PHYS2LOG((unsigned int)pgdir_index[pgdir_index] & PG_4K_TABLE_MASK);
   pgtable[pgtbl_index] = (physical & PG_4K_MASK) | (flags & ~PG_4K_MASK) | PG_L2TYPE_4K;
   
   return success;
#endif
}

/* pg_add_1M_mapping
   Add or edit an existing 1M mapping to a page directory
   => pgdir = pointer to page directory to add the 1M mapping to
      virtual = the full virtual address for the start of the 1M page
      physical = the full physical address of the page frame
      flags = flag bits to be ORd with the phys addr
   <= 0 for success, anything else is a fail
*/
kresult pg_add_1M_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int physical, 
                          unsigned int flags)
{
   PAGE_DEBUG("[page:%i] mapping 1M: %x -> %x (%x) dir index %x\n", 
           CPU_ID, virtual, physical, flags, virtual >> PG_1M_SHIFT);
   
   virtual = virtual & PG_1M_MASK;
   physical = physical & PG_1M_MASK;
   
   /* create 1MB entry, read+write for kernel-only
      ensure we don't overrun the 16K level 1 page directory */
   if((virtual >> PG_1M_SHIFT) < PG_1M_ENTRIES)
   {
      unsigned int entry = (unsigned int)(pgdir[(virtual >> PG_1M_SHIFT)]);
      
      /* give up if this entry is taken by a non-1M mapping entry */
      if((entry & PG_L1TYPE_MASK) != 0)
         if((entry & PG_L1TYPE_MASK) != PG_L1TYPE_1M)
            return e_failure;
      
      pgdir[(virtual >> PG_1M_SHIFT)] = (unsigned int *)(physical | flags | PG_L1TYPE_1M);
      return success;
   }
   
   /* fall through to failure */
   return e_failure;
}

/* pg_remove_1M_mapping
   Delete an existing 1M mapping to a page directory
   => pgdir = pointer to page directory to add the 1M mapping to
      virtual = the full virtual address for the start of the 1M page
   <= 0 for success, anything else is a fail
*/
kresult pg_remove_1M_mapping(unsigned int **pgdir, unsigned int virtual)
{
   PAGE_DEBUG("[page:%i] unmapping 1M: virt %x\n", CPU_ID, virtual);
   
   virtual = virtual & PG_1M_MASK;
   
   /* remove a 1MB entry, ensure we don't overrun the 16K level 1 page directory */
   if((virtual >> PG_1M_SHIFT) < PG_1M_ENTRIES)
   {
      unsigned int entry = (unsigned int)(pgdir[(virtual >> PG_1M_SHIFT)]);
      
      /* give up if this entry is taken by a non-1M mapping entry */
      if((entry & PG_L1TYPE_MASK) != 0)
         if((entry & PG_L1TYPE_MASK) != PG_L1TYPE_1M)
            return e_failure;
      
      pgdir[(virtual >> PG_1M_SHIFT)] = NULL;
      return success;
   }
   
   /* fall through to failure */
   return e_failure;
}

/* pg_map_phys_to_kernel_space
   Read through a physical page frame stack and add entries to the kernel's
   virtual space, mapping 1M pages in at a time
   => base = base of descending stack
      top = last entry in stack
*/
void pg_map_phys_to_kernel_space(unsigned int *base, unsigned int *top)
{
   /* from kernel/ports/arm/include/memory.h */
   unsigned int **pg_dir = (unsigned int **)KernelPageDirectory;

   /* we must assume the physical page stacks are full - ie: no one has
      popped any stack frames off them. and we have to assume for now that
      the frames are in ascending order. scan through the stacks and map
      frames into the kernel's virtual space. */
   while(base >= top)
   {
      unsigned int addr, virtual_addr;
      
      addr = *base & PG_1M_MASK;

      /* perform the page table scribbling */
      virtual_addr = (unsigned int)KERNEL_PHYS2LOG((unsigned int)addr);
      
      PAGE_DEBUG("[page:%i] mapping to kernel 1M, page dir[%i] = phys %x (virt %x) (lvl1 %x)\n",
              CPU_ID, (virtual_addr >> PG_1M_SHIFT), addr, virtual_addr, pg_dir);
      
      /* create 1MB entry, cached and buffered, read+write for kernel-only */
      pg_add_1M_mapping(pg_dir, virtual_addr, addr,
                        (PG_CACHE_PAGE << PG_1M_CB_SHIFT) | (PG_ACCESS_KERNEL << PG_1M_AP_SHIFT));
      
      /* skip to next 1M boundary */
      while(base >= top)
      {
         base--;
         if((*base & PG_1M_MASK) != addr) break;
      }
   }
}

/* pg_load_pgdir
   Sync the current process's level 1 page directory with the processor's
   => pgdir = page directory to load
   <= 0 for success, or an error code
*/
kresult pg_load_pgdir(unsigned int **pgdir)
{
   dprintf("pg_load_pgdir: not implemented\n");
   return success;
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
   
   unsigned int *boot_pg_dir = (unsigned int *)KernelPageDirectory;
   
   unsigned int loop;
      
   PAGE_DEBUG("[page:%i] initialising... zeroing %i entries\n", CPU_ID, KERNEL_SPACE_BASE >> PG_1M_SHIFT);
   
   /* clear out all non-kernel space entries to start again - the kernel
      boot page directory is made up of 4096 x 1M entries */
   for(loop = 0; loop < (KERNEL_SPACE_BASE >> PG_1M_SHIFT); loop++)
      boot_pg_dir[loop] = NULL;
   
   /* ensure all of physical RAM is mapped into the kernel */
   pg_map_phys_to_kernel_space(high_base, high_ptr);
   pg_map_phys_to_kernel_space(low_base, low_ptr);

   /* now point the boot CPU's level one page directory at the kernel boot page dirctory.
      the 16K contig space for the table is in the kernel's critical section so it won't
      be reallocated. multiprocessor systems can clone theirs when needed */
   cpu_table[CPU_ID].pgdir = boot_pg_dir;
   
   BOOT_DEBUG("[page:%i] paging initialised\n", CPU_ID);
}

