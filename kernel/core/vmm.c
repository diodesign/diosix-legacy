/* kernel/core/vmm.c
 * diosix portable virtual memory manager
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>
#include <multiboot.h>
#include <sglib.h>

rw_gate vmm_lock;

/* we run two physical page stacks, one for memory below the 16MB boundary
   in case DMA hardware needs it. All over physical pages above 16MB are
   pulled from a second stack. The lower stack starts at the physical 12MB
   boundary and descends by a maximum of 16K. The second stack starts at this
   point and continues descending. Of course, physical pages occupied by
   the kernel must be skipped. FIXME Doesn't seem that portable :(

   ---- 12MB ------------------------------------------------------
   -------------- phys_pg_stack_low_base -------------------------
      .... physical page stack frames for lower memory .....
   -------------- phys_pg_stack_low_limit ------------------------
   ---- 12MB - 16KB -----------------------------------------------
   -------------- phys_pg_stack_high_base ------------------------
      .... physical page stack frames for upper memory ....
   -------------- phys_pg_stack_high_limit -----------------------
                .... hole free for use ....
   ---- 8MB ----- max top of kernel -------------------------------
   ---- 4MB ----- start of kernel --------------------------------- 

*/
unsigned int *phys_pg_stack_low_base   = KERNEL_PHYS2LOG(MEM_PHYS_STACK_BASE -
                                          sizeof(unsigned int));
unsigned int *phys_pg_stack_low_ptr;
unsigned int *phys_pg_stack_low_limit;
unsigned int *phys_pg_stack_high_base  = KERNEL_PHYS2LOG(MEM_PHYS_STACK_BASE -
                                          ((MEM_DMA_REGION_MARK / MEM_PGSIZE) *
                                           sizeof(unsigned int)) -
                                           sizeof(unsigned int));
unsigned int *phys_pg_stack_high_ptr;
unsigned int *phys_pg_stack_high_limit;

unsigned int phys_pg_count = 0; /* nr of physical pages at our disposable */
unsigned int phys_pg_reqed = 0; /* nr of physical pages requested */

/* -------------------------------------------------------------------------
    Kernel heap management
   ------------------------------------------------------------------------- */

/* we use a simple first fit approach (AST Minix 3 book, p382 2nd ed) */
kheap_block *kheap_free = NULL; /* head of sorted free list */
kheap_block *kheap_allocated = NULL; /* head of unsorted allocated list */

/* vmm_malloc
   Allocate a block of physical memory for kernel use. This function should
   only be used to allocate blocks of memory for internal structures. Bear in
   mind that kernel space is mapped into all of physical memory - therefore if
   a requested block is greater than a page in size, we'll have to find a
   string of contiguous physical pages to hold the block. To allocate memory
   for userland processes, use the physical page request functions and map
   into the appropriate usr address space as required.
   => addr = pointer to pointer into which the requested block's address will
             be written. the address will be a kernel logical one.
      size = size of block required.
   <= 0 for success, or result code
*/
kresult vmm_malloc(void **addr, unsigned int size)
{
   unsigned int safe_size, *addr_word = (unsigned int *)addr;
   kheap_block *block, *extra;
	
	lock_gate(&(vmm_lock), LOCK_WRITE);
	
   /* adjust size to include our block header plus enough memory to tack
      a header onto any left over memory - we round up to a set size
	   (default 64 bytes) to reduce fragmentation and aid quick realloc'ing */
   size += sizeof(kheap_block);
   safe_size = size + sizeof(kheap_block);
	safe_size = KHEAP_PAD(safe_size);
   
   /* scan through free list to find the first block that will fit the
      requested size */
   block = kheap_free;
   while(block)
   {
      if(block->size > safe_size)
         break;
      block = block->next;
   }

   /* if the block is set, then we've found something suitable in the free
      list. remove it from the free list */
   if(block)
   {
      if(block->previous)
         block->previous->next = block->next;
      
      if(block->next)
         block->next->previous = block->previous;
      
      if(kheap_free == block)
         kheap_free = block->next;
   }

   /* if block is unset then we haven't found a suitable block (or the free
      list is empty). so allocate enough pages for the request */
   if(!block)
   {
      int pg_count, type = MEM_HIGH_PG;
      kresult result = vmm_ensure_pgs(safe_size, type);
      if(result)
      {
         /* try using low memory */
         type = MEM_LOW_PG;
         result = vmm_ensure_pgs(safe_size, type);
         if(result)
			{
				VMM_DEBUG("[vmm:%i] failed to grab physical pages for kernel heap (req size %i bytes)\n",
						  CPU_ID, safe_size);
				unlock_gate(&(vmm_lock), LOCK_WRITE);
				return result; /* give up otherwise */
			}
      }
		
      /* by now, we've verified that we have a run of pages so grab them */
      for(pg_count = 0; pg_count < ((safe_size / MEM_PGSIZE) + 1); pg_count++)
      {
         result = vmm_req_phys_pg((void **)&block, type); /* get pages in reverse order */
         if(result)
			{
				VMM_DEBUG("[vmm:%i] failed to grab physical page for kernel heap\n", CPU_ID);
				unlock_gate(&(vmm_lock), LOCK_WRITE);
				return result; /* XXX shouldn't happen - memory leak */
			}
      }

#ifdef VMM_DEBUG
		VMM_DEBUG("[vmm:%i] asked for a block of pages for %i bytes: %p\n", CPU_ID, safe_size, block);
#endif
		
      /* don't forget to convert from physical to kernel's logical */
      block = KERNEL_PHYS2LOG(block);
      block->size = (unsigned int)MEM_PGALIGN(safe_size) + MEM_PGSIZE;
#ifdef VMM_DEBUG
      VMM_DEBUG("[vmm:%i] grabbed %i bytes from mem %p for heap\n", CPU_ID, block->size, block); 
#endif
   }

   /* trim off excess memory and add this to the free list */
   extra = (kheap_block *)((unsigned int)block + size);
   extra->magic = KHEAP_FREE;
   extra->size = block->size - size;
   vmm_heap_add_to_free(extra);

   /* write pointer to the start of data and the block's header details */
   *addr_word = (unsigned int)block + sizeof(kheap_block);
   block->magic = KHEAP_INUSE;
   block->size = size; /* the true size of the requested block inc header */

   /* add to head of allocated link list */
   block->next = kheap_allocated;
   if(kheap_allocated) kheap_allocated->previous = block;
   block->previous = NULL;
   kheap_allocated = block;

	unlock_gate(&(vmm_lock), LOCK_WRITE);
   return success;
}

/* vmm_malloc_read_size
	Return the allocated size of a given block in bytes or 0 for bad block */
unsigned int vmm_malloc_read_size(void *addr)
{
	kheap_block *block = (kheap_block *)((unsigned int)addr - sizeof(kheap_block));
	return block->size;
}

/* vmm_malloc_write_size
   Update the allocated size of a given block in bytes */
void vmm_malloc_write_size(void *addr, unsigned int size)
{
	kheap_block *block = (kheap_block *)((unsigned int)addr - sizeof(kheap_block));
	block->size = size;
}

/* vmm_heap_add_to_free
   Add the given kernel heap block to the free list. Keep the list in order of
   base address, lowest first (at the head) and ascending. Then, scan the free
   list and merge adjoining blocks together.
   => pointer to heap block
*/
void vmm_heap_add_to_free(kheap_block *block)
{
	lock_gate(&(vmm_lock), LOCK_WRITE);
	
   kheap_block *block_loop = kheap_free;

   while(block_loop)
   {
      /* insert block in front of block_loop */
      if(block < block_loop)
      {
         block->next = block_loop;
         block->previous = block_loop->previous;
         if(!block->previous) kheap_free = block; /* connect to head if first */

         if(block_loop->previous)
            block_loop->previous->next = block;

         block_loop->previous = block;
         break;
      }

      /* add to end of the free list */
      if(!block_loop->next)
      {
         block_loop->next = block;
         block->next = NULL;
         block->previous = block_loop;
         break;
      }

      block_loop = block_loop->next;
   }

   /* if list is empty, then start it with block */
   if(!kheap_free)
   {
      block->next = NULL;
      block->previous = NULL;
      kheap_free = block;
   }

   block->magic = KHEAP_FREE;

   /* merge adjoining blocks */
   block_loop = kheap_free;
   while(block_loop)
   {
      kheap_block *target = block_loop->next;
      if(!target) break; /* sanity check next ptr */

      if((unsigned int)target == ((unsigned int)block_loop + block_loop->size))
      {
         block_loop->next = target->next;
         if(target->next)
            target->next->previous = block_loop;
         block_loop->size += target->size;
      }
      block_loop = block_loop->next;
   }
	
	unlock_gate(&(vmm_lock), LOCK_WRITE);
}

/* vmm_free
   Release a previously allocated block of physical memory and add it to the
   free lists. the address of the block must be a kernel logical one.
   => addr = base of block to free up
   <= 0 for success, or result code
*/
kresult vmm_free(void *addr)
{
   kheap_block *block;

   /* get out now if the addr is insane */
   if((unsigned int)addr < KERNEL_VIRTUAL_BASE) /* XXX assumes high knl */
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_free: given nonsense address %x\n", CPU_ID, addr);
		debug_stacktrace();
      return e_bad_address;
   }

   block = (kheap_block *)((unsigned int)addr - sizeof(kheap_block));

   /* sanity check the block */
   if(block->magic != KHEAP_INUSE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_free: block %x has wrong magic %x\n",
              CPU_ID, block, block->magic);
		debug_stacktrace();
      return e_bad_magic;
   }

	lock_gate(&(vmm_lock), LOCK_WRITE);
	
   /* update magic */
   block->magic = KHEAP_FREE;

   /* remove from allocated linked list */
   if(block->previous)
      block->previous->next = block->next;

   if(block->next)
      block->next->previous = block->previous;

   if(kheap_allocated == block)
      kheap_allocated = block->next;

   /* add to head of free list */
   vmm_heap_add_to_free(block);

	unlock_gate(&(vmm_lock), LOCK_WRITE);
	
   return success;
}

/* vmm_realloc
   Resize an allocated block by a given number of bytes - which may require
   moving the block.
   => addr = pointer to allocated block, if NULL then treat as a malloc()
      change = number of bytes to grow block by (if positive) or shrink
					the block by (if negative)
   <= pointer to new block pointer if it had to be moved, or the original
      block pointer if no move was required, or NULL for failure. The original
      block will remain in-tact if the reallocation fails for whatever reason.
*/
void *vmm_realloc(void *addr, signed int change)
{
	void *new;
	unsigned int size;
	
	if(!addr)
	{
		KOOPS_DEBUG("[vmm:%i] OMGWTF tried to alter size of a dereferenced block by %i bytes\n",
				      CPU_ID, change);
		return NULL; /* failed */
	}
	
	/* read the size of the block in bytes - if the size is zero (because the block
		was not found), then treat as a malloc */
	size = vmm_malloc_read_size(addr);
	
	if(!size)
	{
		/* we can't decrease or unchange a block that didn't exist... */
		if(change < 1)
		{
			KOOPS_DEBUG("[vmm:%i] OMGWTF tried to shrink a non-existent block by %i bytes\n",
							CPU_ID, 0 - change);
			return NULL; /* failed */			
		}
		
		if(vmm_malloc(&new, change) == success)
			return new; /* we succeeded, send back the pointer */
		else
			return NULL; /* failed to allocate */
	}
	
	/* a change of zero bytes will always be successful once we've ascertained that the
	   block pointer is valid */
	if(change == 0) return addr;
	
	/* we can't shrink a block to and beyond zero */
	if(change < 1)
	{
		if((0 - change) >= size)
		{
			KOOPS_DEBUG("[vmm:%i] OMGWTF tried to shrink a block of size %i by %i bytes\n",
							CPU_ID, size, 0 - change);
			return NULL; /* failed */
		}
	}

	/* sanity checks passed, try to grow/shrink the block within the padding of the block */
	if((size + change) <= KHEAP_PAD_SAFE(size))
	{
		/* update the size stats and return the same pointer */
		vmm_malloc_write_size(addr, size + change);
		return addr;
	}
	
	/* we can't extend within our block padding so it's time to malloc-copy-free */
	if(vmm_malloc((void **)&new, size + change) == success)
	{
		/* so far so good, copy the contents of the old block to the new one,
		   free the old block and return the new pointer */
		if(change > 0)
			vmm_memcpy(new, addr, size);
		else
			vmm_memcpy(new, addr, size + change);
		vmm_free(addr);
		return new;
	}
	
	/* fall through to a malloc failure */
	return NULL;
}


/* -------------------------------------------------------------------------
    Physical page management
   ------------------------------------------------------------------------- */

/* vmm_req_phys_pg
   Request an available physical page frame. If a sub-DMA marker page is
   requested and no such physical page is available, this function will give up
   and return 1. If no preference is given, this function will attempt to grab
   a physical page from above the DMA marker first, and look below the DMA
   marker if it has no success.
   => addr = pointer to pointer into which new stack frame base address will be
             written.
      pref = 0 to request a page from below the DMA marker, otherwise 1 for no
             preference.
   <= 0 for success or error code
*/
kresult vmm_req_phys_pg(void **addr, int pref)
{	
	lock_gate(&(vmm_lock), LOCK_WRITE);
	
   /* is a DMA-able physical page requested? */
   if(pref == MEM_LOW_PG)
   {
      /* if ptr is above the base, then the stack's empty */
      if(phys_pg_stack_low_ptr > phys_pg_stack_low_base)
		{
			unlock_gate(&(vmm_lock), LOCK_WRITE);
         return e_no_phys_pgs;
		}

      /* otherwise, hand out a page frame */
      goto get_low_page;
   }

   /* try to get a 'normal' phys page frame first, then try low */
   if(phys_pg_stack_high_ptr > phys_pg_stack_high_base)
   {
      if(phys_pg_stack_low_ptr > phys_pg_stack_low_base)
         return e_no_phys_pgs;
      else
         goto get_low_page;
   }
   /* fall through to getting a high page */

   *addr = (unsigned int *)*phys_pg_stack_high_ptr;
   phys_pg_stack_high_ptr++;
   goto get_page_success;

   /* are these gotos ugly? */
get_low_page:
   *addr = (unsigned int *)*phys_pg_stack_low_ptr;
   phys_pg_stack_low_ptr++;
   /* fall through to success */

get_page_success:
   phys_pg_reqed++; /* update accounting totals */
	/* we don't clean the page at this stage - it has
	   to be mapped in first */
	
	/* would be nice to clean this page */
	vmm_memset(KERNEL_PHYS2LOG(*addr), 0, MEM_PGSIZE);
	
	unlock_gate(&(vmm_lock), LOCK_WRITE);
   return success;
}

/* vmm_return_phys_pg
   Give up a physical page frame that is no longer needed and can be later
   reused.
   => addr = physical page frame base address
   <= 0 for success or error code
*/
kresult vmm_return_phys_pg(void *addr)
{	
   /* if the address is not mmu page align then things are up the swanny */
   if(MEM_PGALIGN(addr) != addr)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_return_phys_pg: physical stack frame "
                  "%x not page aligned!\n", CPU_ID, addr);
		debug_stacktrace();
      return e_not_pg_aligned;
   }

	lock_gate(&(vmm_lock), LOCK_WRITE);
	
   /* decide which stack we're going to return this page frame onto */
   if((unsigned int)addr < MEM_DMA_REGION_MARK)
   {
      /* check stack bounds before we go any further */
      if(phys_pg_stack_low_ptr <= phys_pg_stack_low_limit)
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_return_phys_pg: low physical stack frame "
                   "has overflowed!\n", CPU_ID);
			debug_stacktrace();
			unlock_gate(&(vmm_lock), LOCK_WRITE);
         return e_phys_stk_overflow;
      }

      /* push stack frame onto lower stack */
      phys_pg_stack_low_ptr--;
      *phys_pg_stack_low_ptr = (unsigned int)addr;
   }
   else
   {
      /* check stack bounds before we go any further */
      if(phys_pg_stack_high_ptr <= phys_pg_stack_high_limit)
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_return_phys_pg: high physical stack "
                     "has overflowed!\n", CPU_ID);
			debug_stacktrace();
			unlock_gate(&(vmm_lock), LOCK_WRITE);
         return e_phys_stk_overflow;
      }

      /* push stack frame onto upper stack */
      phys_pg_stack_high_ptr--;
      *phys_pg_stack_high_ptr = (unsigned int)addr;
   }

   phys_pg_reqed--; /* update accounting totals */
	
	unlock_gate(&(vmm_lock), LOCK_WRITE);
	
   return success;
}

/* vmm_enough_pgs
   Check to see if there are enough physical pages to hold the given amount
   of memory.
   => size = amount of memory to check for
   <= 0 for success (sufficient mem), or result code
*/
kresult vmm_enough_pgs(unsigned int size)
{
   if(!size) return success; /* there's always room for zero bytes ;) */

	lock_gate(&(vmm_lock), LOCK_READ);
	
   /* convert size into whole number of pages, rounding up */
   if((phys_pg_count - phys_pg_reqed) < ((size / MEM_PGSIZE) + 1))
	{
		unlock_gate(&(vmm_lock), LOCK_READ);
      return e_not_enough_pgs;
	}

	unlock_gate(&(vmm_lock), LOCK_READ);
   return success;
}

/* vmm_ensure_pgs
   Check that there is a contiguous run of physical pages available in the
   given area. Note: it will only check the selected type of mem.
   => size = number of bytes to check for
      type = 0 for DMA-able memory, 2 for non-DMA-able
*/
kresult vmm_ensure_pgs(unsigned int size, int type)
{
   unsigned int pg_loop;
	unsigned int *pg_ptr, *pg_base;
	
	lock_gate(&(vmm_lock), LOCK_READ);

   /* are we checking DMA-able physical memory? */
   if(type == MEM_LOW_PG)
   {
      pg_ptr = phys_pg_stack_low_ptr;
      pg_base = phys_pg_stack_low_base;
   }
   else /* or higher mem? */
   {
      pg_ptr = phys_pg_stack_high_ptr;
      pg_base = phys_pg_stack_high_base;
   }

   /* find number of pages fitting into size, rounding down for the benefit
      of the following checks. make sure physical pages are avaliable and
      that there's enough to bother checking before continuing. */
   pg_loop = (size / MEM_PGSIZE); 
   
	if(pg_ptr > pg_base)
	{
		unlock_gate(&(vmm_lock), LOCK_READ);
		return e_no_phys_pgs;
	}
	
   if((pg_base - pg_ptr) < pg_loop)
	{
		unlock_gate(&(vmm_lock), LOCK_READ);
		return e_no_phys_pgs;
	}

   /* we check to see if there is a run of contiguous stack frame pointers
      that descend in value as the loop moves up towards the stack base */
   while(pg_loop)
   {
      unsigned int *pg_next = pg_ptr;
		pg_next++;

      if(*pg_ptr != (*pg_next + MEM_PGSIZE))
		{
			unlock_gate(&(vmm_lock), LOCK_READ);
			return e_not_contiguous;
		}
      pg_loop--;
      pg_ptr++;
   }

	unlock_gate(&(vmm_lock), LOCK_READ);
   return success; /* managed to find run of pages */
}

/* -------------------------------------------------------------------------
    VMM initialisation
   ------------------------------------------------------------------------- */

/* vmm_initialise
   Start up vmm, which will mostly be calls to the underlying hardware
   dependent code. Bear in mind that any low level init should have been done
   by the hardware-dependent code's start.s
   => mbd = ptr to multiboot data about the system around us
   <= 0 for success or error code
*/
kresult vmm_initialise(multiboot_info_t *mbd)
{
   mb_memory_map_t *region;
   void *heap_init;
   unsigned int pg_stack_size, *pg_stack_top;

	/* initialise the smp lock */
	vmm_memset(&(vmm_lock), 0, sizeof(rw_gate));
	
   BOOT_DEBUG("[vmm:%i] kernel: logical start %x end %x size %i bytes\n",
              CPU_ID, KERNEL_START, KERNEL_END, KERNEL_SIZE);
   BOOT_DEBUG("[vmm:%i] kernel: physical start %x end %x aligned end %x\n",
              CPU_ID, KERNEL_PHYSICAL_BASE, KERNEL_PHYSICAL_END,
				  KERNEL_PHYSICAL_END_ALIGNED);

   /* check bit six to see if we can access mmap info */
   if(!(mbd->flags & (1<<6)))
   {
      VMM_DEBUG("*** missing mem map data from multiboot. (%x)\n", mbd->flags);
      return e_missing_mb_data;
   }

   /* set up physical page frame stacks, initially empty.
      note: stack ptrs always point to the top available word. */
   phys_pg_stack_low_ptr    = phys_pg_stack_low_base;
   phys_pg_stack_low_limit  = phys_pg_stack_low_base;
   phys_pg_stack_high_ptr   = phys_pg_stack_high_base;
   phys_pg_stack_high_limit = phys_pg_stack_high_base;

   /* do a quick count up of physical memory so we know how large our stacks
      need to be and thus, we can keep the physical page frames holding the
      stacks out of the stacks - we must make sure we do not hand out page
      frames holding the stacks and the kernel to the rest of the system to use
    */
   region = (mb_memory_map_t *)mbd->mmap_addr;
   while((unsigned int)region < mbd->mmap_addr + mbd->mmap_length)
   {
		if(region->type == 1) /* if region is present RAM */
			phys_pg_count += (region->length_low / MEM_PGSIZE);

      /* get next region */
      region = (mb_memory_map_t *)((unsigned int)region +
                                   region->size +
                                   sizeof(unsigned int));
   }

   pg_stack_size = phys_pg_count * sizeof(unsigned int);
   pg_stack_top = (unsigned int *)((unsigned int)phys_pg_stack_low_base -
                                   pg_stack_size);

   BOOT_DEBUG("[vmm:%i] found %i pages, %iMB total, phys stack size %i bytes\n",
              CPU_ID, phys_pg_count, (phys_pg_count * MEM_PGSIZE) / (1024 * 1024),
              pg_stack_size);
	
	/* check to make sure we have enough memory to function */
	if((phys_pg_count * MEM_PGSIZE) < (KERNEL_CRITICAL_END - KERNEL_CRITICAL_BASE))
	{
		BOOT_DEBUG("*** Not enough memory present, must have at least %i bytes available.\n",
		   		  KERNEL_CRITICAL_END - KERNEL_CRITICAL_BASE);
		return e_failure;
	}
	
   VMM_DEBUG("[vmm:%i] page stack: lo base %p hi base: %p top: %p aligned top: %p\n",
				 CPU_ID, phys_pg_stack_low_base, phys_pg_stack_high_base,
			    pg_stack_top, MEM_PGALIGN(pg_stack_top)); 

   /* the stack may not end on a page boundary, so round down - this is so
	   that we can make sure the pages holding the stacks don't end up on
	   the list of available physical page frames */
   pg_stack_top = MEM_PGALIGN(pg_stack_top);

   /* run through the memory areas found by the bootloader and build up
      physical page stacks */
   region = (mb_memory_map_t *)mbd->mmap_addr;
   while((unsigned int)region < mbd->mmap_addr + mbd->mmap_length)
   {
      unsigned int pg_loop;
      unsigned int pg_count_lo = 0;
      unsigned int pg_count_hi = 0;
      unsigned int pg_skip = 0;
      unsigned int max_addr = region->base_addr_low + region->length_low;

      VMM_DEBUG("[vmm:%i] mem region: start %x length %i type %x\n",
               CPU_ID, region->base_addr_low, region->length_low, region->type);

		/* is this region present? */
		if(region->type == 1)
		{
			pg_loop = region->base_addr_low;
			while((pg_loop + MEM_PGSIZE) <= max_addr)
			{
				/* skip over kernel in physical mem, otherwise things get messy */
				if((pg_loop >= (unsigned int)KERNEL_PHYSICAL_BASE) &&
					(pg_loop < (unsigned int)KERNEL_PHYSICAL_END_ALIGNED))
				{
					pg_skip++;
					pg_loop += MEM_PGSIZE;
					continue;
				}

				/* skip over pages that will hold these page frame stacks */
				if((pg_loop < (unsigned int)phys_pg_stack_low_base) &&
					(pg_loop >= (unsigned int)pg_stack_top))
				{
					pg_skip++;
					pg_loop += MEM_PGSIZE;
					continue;
				}

				/* skip over pages holding payload binaries XXX inefficient */
				if(payload_exist_here(pg_loop))
				{
					pg_skip++;
					pg_loop += MEM_PGSIZE;
					continue;
				}

				/* decide which stack to place the page frame in */
				if(pg_loop < MEM_DMA_REGION_MARK)
				{
					*phys_pg_stack_low_ptr = pg_loop;
					phys_pg_stack_low_ptr--;
					pg_count_lo++;
				}
				else
				{
					*phys_pg_stack_high_ptr = pg_loop;
					phys_pg_stack_high_ptr--;
					pg_count_hi++;
				}
				
				if(phys_pg_stack_low_ptr < phys_pg_stack_high_base)
				{
					KOOPS_DEBUG("*** lomem page stack crashed into himem stack!\n"
					            "    ptr %p after %i pages (%x) - halting.\n",
							      phys_pg_stack_low_ptr, pg_count_lo, pg_loop);
					while(1);
				}
				
				pg_loop += MEM_PGSIZE;
			}

			VMM_DEBUG("[vmm:%i] added phys pages: %i low, %i high (%i reserved)\n",
					  CPU_ID, pg_count_lo, pg_count_hi, pg_skip);

		}
			
      /* get to the next memory region */
      region = (mb_memory_map_t *)((unsigned int)region +
                                   region->size +
                                   sizeof(unsigned int));
   }

   /* set stack limits so we know when we're out of memory */
   phys_pg_stack_low_ptr++; /* adjust ptr to top word */
   phys_pg_stack_high_ptr++; /* adjust ptr to top word */
   phys_pg_stack_low_limit = phys_pg_stack_low_ptr;
   phys_pg_stack_high_limit = phys_pg_stack_high_ptr;
		
   /* now we've got a grip on physical memory, map it all into our virtual
      space using pagination */
   pg_init(); /* non-portable code */
	
   /* prime the kernel heap while we still have contiguous space */
   vmm_malloc(&heap_init, KHEAP_INITSIZE);
   vmm_free(heap_init);

   return 0;
}

/* vmm_memset
	Brute-force overwrite a block of memory accessible to the kernel.
   *Quite* dangerous, assumes caller knows what it's doing.
   => addr  = base address to start writing to
      value = byte-wide value to write into each byte
      count = number of bytes to write
*/
void vmm_memset(void *addr, unsigned char value, unsigned int count)
{
	unsigned char *ptr = (unsigned char *)addr;
	unsigned int i;
	
	for(i = 0; i < count; i++)
		ptr[i] = value;
}

/* vmm_memcpy
   Brute-force copy a block of memory immediately accessible
   to the kernel.
   *Quite* dangerous, assumes caller knows what it's doing.
   => target  = base address to write to
      source = base address to read from 
    	count = number of bytes to write
 */
void vmm_memcpy(void *target, void *source, unsigned int count)
{
	unsigned char *ptr1 = (unsigned char *)target;
	unsigned char *ptr2 = (unsigned char *)source;
	unsigned int i;
	for(i = 0; i < count; i++)
		ptr1[i] = ptr2[i];
}

/* vmm_memcpyuser
   Copy data between two different process spaces, or between
   kernel space and userspace. Copying into kernel space will
   only work if tproc is NULL; it must be implicitly dereferenced
   to indicate that this is expected behaviour and not shenangians.
   Similarly, copying from kernel space will only work if sproc
   is NULL.
   => target = target usermode virtual address
      tproc = target process structure (or NULL for kernel)
      source = source usermode virtual address
	   sproc = source process structure (or NULL for kernel)
      count = number of bytes to write
*/
kresult vmm_memcpyuser(void *target, process *tproc,
						     void *source, process *sproc, unsigned int count)
{
	/* the goal is to resolve the addresses into kernel 
      virtual addresses */
	kresult err = e_failure;
	unsigned int ktarget, ksource;
	
	if(((unsigned int)target + count) >= KERNEL_SPACE_BASE)
	{
		/* copying into kernel, tproc must be NULL */
		if(tproc) goto vmm_memcpyuser_wtf;
		ktarget = (unsigned int)target;
	}
	else
	{
		err = pg_user2kernel(&ktarget, (unsigned int)target, tproc);
		if(err) goto vmm_memcpyuser_wtf;
	}

	if(((unsigned int)source + count) >= KERNEL_SPACE_BASE)
	{
		/* copying from kernel, sproc must be NULL */
		if(sproc) goto vmm_memcpyuser_wtf;
		ksource = (unsigned int)source;
	}
	else
	{
		err = pg_user2kernel(&ksource, (unsigned int)source, sproc);
		if(err) goto vmm_memcpyuser_wtf;
	}
	
	/* perform the copy with sane virtual addresses */
	vmm_memcpy((void *)ktarget, (void *)ksource, count);
	return success;
	
vmm_memcpyuser_wtf:
	KOOPS_DEBUG("[vmm:%i] OMGWTF: vmm_memcpyuser has bad params!\n"
			      "                target = %p (proc %p)\n"
			      "                source = %p (proc %p)\n"
			      "                copying: %i bytes\n",
			      CPU_ID, target, tproc, source, sproc, count);
	debug_stacktrace();
	return err;
}

/* -------------------------------------------------------------------------
   Process memory area management
   Using SGLib: http://sglib.sourceforge.net/doc/index.html
 ------------------------------------------------------------------------- */

/* this should return a negative number if a given address x falls below
 the base of y, zero if it falls within the base and size of y, or positive to
 indicate it's greater than base+size of y */
#define vmm_cmp_vma(x, y) ( (x->area->base) < (y->area->base) ? -1 : ( (x->area->base) >= ((y->area->base) + (y->area->size)) ? 1 : 0 ) )

/* pointers to vmas are stored in a per-process balanced binary tree */
SGLIB_DEFINE_RBTREE_PROTOTYPES(vmm_tree, left, right, colour, vmm_cmp_vma);
SGLIB_DEFINE_RBTREE_FUNCTIONS(vmm_tree, left, right, colour, vmm_cmp_vma);

/* vmm_link_vma
   Link an existing area to a process
   => proc = process to link the vma with
      vma = the vma to link
   <= success or a failure code
*/
kresult vmm_link_vma(process *proc, vmm_area *vma)
{
	kresult err;
	vmm_tree *new, *existing;
	unsigned int loop;
	
	/* allocate and zero memory for the new tree node */
	err = vmm_malloc((void **)&new, sizeof(vmm_tree));
	if(err) return err;
	vmm_memset(new, 0, sizeof(vmm_tree));
	
	/* set the area pointer */
	new->area = vma;
	
	lock_gate(&(proc->lock), LOCK_WRITE);
	
	/* and try to add to the tree */
	if(sglib_vmm_tree_add_if_not_member(&(proc->mem), new, &existing))
	{
		lock_gate(&(vma->lock), LOCK_WRITE);
		
		/* non-zero return for success, so incremement the refcount */
		vma->refcount++;
		err = success;
		
		VMM_DEBUG("[vmm:%i] linked vma %p to process %i (%p) via tree node %p\n",
				  CPU_ID, vma, proc->pid, proc, new);
		
		/* add the vma to the list of users */
		if(vma->refcount > vma->users_max)
		{
			process **new_list;
			
			/* we need to grow the list size */
			unsigned int new_size = vma->users_max * 2;
			
			if(vmm_malloc((void **)&new_list, new_size * sizeof(process *)))
			{
				vmm_free(vma);
				new->area = NULL; /* FIXME: and unlink from the tree ?? */
				unlock_gate(&(vma->lock), LOCK_WRITE);
				unlock_gate(&(proc->lock), LOCK_WRITE);
				return e_failure; /* bail out if the malloc failed! */
			}
			
			vmm_memset(new_list, 0, new_size); /* clean the new list */
			
			/* copy over the previous list */
			vmm_memcpy(new_list, vma->users, vma->users_max * sizeof(process *));
			vmm_free(vma->users); /* free the old list */
			
			/* update the list's accounting */
			vma->users = new_list;
			vma->users_max = new_size;
		}
		
		/* find an empty slot and insert the new user's pointer */
		for(loop = 0; loop < vma->users_max; loop++)
			if((vma->users[loop]) == NULL)
			{
				/* found a free slot */
				vma->users[loop] = proc;
				break;
			}
		
		unlock_gate(&(vma->lock), LOCK_WRITE);
	}
	else
	{
		/* the vma already exists or collides with an area */
		err = e_vma_exists;
		vmm_free(new);
		
		VMM_DEBUG("[vmm:%i] couldn't link vma %p to process %i (%p) - collision with vma %p\n", 
				  CPU_ID, vma, proc->pid, proc, existing->area);
	}
	
	unlock_gate(&(proc->lock), LOCK_WRITE);
	
	return err;
}

/* vmm_unlink_vma
   Unlink an existing area from a process and destroy the vma if its
   refcount reaches zero - ie: it's no longer in use.
   => owner = process to unlink the vma from
      victim = the vma to unlink
   <= success or a failure code
 */
kresult vmm_unlink_vma(process *owner, vmm_tree *victim)
{
	lock_gate(&(owner->lock), LOCK_WRITE);
	
	unsigned int loop;
	vmm_area *vma = victim->area;
	
	lock_gate(&(vma->lock), LOCK_WRITE);
	
	/* try to remove from the tree */
	sglib_vmm_tree_delete(&(owner->mem), victim);
	unlock_gate(&(owner->lock), LOCK_WRITE);
	
	/* delete from the vma's users list */
	for(loop = 0; loop < vma->refcount; loop++)
	{
		if(vma->users[loop] == owner)
		{
			vma->users[loop] = NULL;
			vma->users_ptr = loop; /* next free slot is this one */
			break;
		}
	}
	
	/* reduce the refcount and free if zero */
	vma->refcount--;
	if(!(vma->refcount))
	{
		unlock_gate(&(vma->lock), LOCK_WRITE | LOCK_SELFDESTRUCT);
		vmm_free(vma);
	}
	else
		unlock_gate(&(vma->lock), LOCK_WRITE);
		
	VMM_DEBUG("[vmm:%i] unlinked vma %p from tree node %p in process %i (%p)\n",
			  CPU_ID, vma, victim, owner->pid, owner);
	
	return vmm_free(victim);
}

/* vmm_add_vma
   Create a new memory area and link it to a process
   => proc = process to link the new vma to
      base = base virtual address for this vma
      size = vma size in bytes
      flags = vma status and access flags
      cookie = a private reference set by the userspace page manager
   <= success or a failure code
*/
kresult vmm_add_vma(process *proc, unsigned int base, unsigned int size,
						  unsigned char flags, unsigned int cookie)
{
	vmm_area *new;
	kresult err = vmm_malloc((void **)&new, sizeof(vmm_area));
	if(err) return err;
	vmm_memset(new, 0, sizeof(vmm_area)); /* zero the area */
	
	/* fill in the details - ref_count will be updated by the link_vma() call */
	new->flags    = flags;
	new->refcount = 0;
	new->base     = base;
	new->size     = size;
	new->token    = cookie;
	
	/* set up an empty amortised list of vma users */
	new->users_max = 4;
	err = vmm_malloc((void **)&(new->users), sizeof(process *) * new->users_max);
	if(err)
	{
		vmm_free(new);
		return err;
	}
	vmm_memset(new->users, 0, sizeof(process *) * new->users_max); /* zero the list */
	
	VMM_DEBUG("[vmm:%i] created vma %p for proc %i (%p): base %x size %i flags %x cookie %x\n",
			  CPU_ID, new, proc->pid, proc, base, size, flags, cookie);
	
	if(vmm_link_vma(proc, new))
		return e_failure;
	else
		return success;
}

/* vmm_duplicate_vmas
   Duplicate the memory map of a process for a child by
   recreateing a new tree and linking the VMAs in the parent to the
	child's tree.
   => new = child process
      source = parent process to clone from
   <= success or a failure code
*/
kresult vmm_duplicate_vmas(process *new, process *source)
{
	kresult err;
	struct vmm_tree *node;
	struct sglib_vmm_tree_iterator state;
	
	if(!source) return e_failure;

	VMM_DEBUG("[vmm:%i] duplicating process map for proc %i (%p) from %i (%p)\n",
			  CPU_ID, new->pid, new, source->pid, source);
	
	lock_gate(&(source->lock), LOCK_READ);
	
	/* walk the parent's tree and copy it */
	for(node = sglib_vmm_tree_it_init(&state, source->mem);
		 node != NULL;
		 node = sglib_vmm_tree_it_next(&state))
	{
		err = vmm_link_vma(new, node->area);
		if(err != success) break;
	}
	
	unlock_gate(&(source->lock), LOCK_READ);
	
	return err;
}

/* vmm_destroy_vmas
	Tear down the given process's memory tree
   <= success or a failure code */
kresult vmm_destroy_vmas(process *victim)
{
	kresult err;
	struct vmm_tree *node;
	struct sglib_vmm_tree_iterator state;	
	
	lock_gate(&(victim->lock), LOCK_WRITE);
	
	/* walk the victim's tree, unlink and free the nodes */
	for(node = sglib_vmm_tree_it_init(&state, victim->mem);
		 node != NULL;
		 node = sglib_vmm_tree_it_next(&state))
	{
		err = vmm_unlink_vma(victim, node);
		if(err != success) break;
	}
	
	victim->mem = NULL;
	
	unlock_gate(&(victim->lock), LOCK_WRITE);
	return err;
}

/* vmm_find_vma
	Locate a vma tree node using the given address in the given proc
   <= pointer to tree node or NULL for not found
*/
vmm_tree *vmm_find_vma(process *proc, unsigned int addr)
{
	vmm_tree node;
	vmm_area area;
	vmm_tree *result;
	
	/* give up now if we're given rubbish pointers */
	if(!proc) return NULL;

	/* mock up a vma and node to search for */
	area.base = addr;
	area.size = 1;
	node.area = &area;
	
	lock_gate(&(proc->lock), LOCK_READ);
	result = sglib_vmm_tree_find_member(proc->mem, &node);
	unlock_gate(&(proc->lock), LOCK_READ);
	
	return result;
}

/* vmm_fault
	Make a decision on what to do with a faulting user process by using its
   vma tree and the flags associated with a located vma
   => proc = process at fault
      addr = virtual address where fault occurred
      flags = type of access attempted using the vma_area flags 
   <= decision code
*/
vmm_decision vmm_fault(process *proc, unsigned int addr, unsigned char flags)
{
	lock_gate(&(proc->lock), LOCK_READ);
	
	vmm_area *vma;
	vmm_tree *found = vmm_find_vma(proc, addr);

	if(!found) return badaccess; /* no vma means no possible access */
	
	vma = found->area;

	lock_gate(&(vma->lock), LOCK_READ);
	unlock_gate(&(proc->lock), LOCK_READ);
	
	VMM_DEBUG("[vmm:%i] fault at %x lies within vma %p (base %x size %i) in process %i\n",
			  CPU_ID, addr, vma, vma->base, vma->size, proc->pid);
	
	/* fail this access if it's a write to a non-writeable area */
	if((flags & VMA_WRITEABLE) && !(vma->flags & VMA_WRITEABLE))
	{
		unlock_gate(&(vma->lock), LOCK_READ);
		return badaccess;
	}

	/* defer to the userspace page manager if it is managing this vma */
	if(!(vma->flags & VMA_MEMSOURCE))
	{
		unlock_gate(&(vma->lock), LOCK_READ);
		return external;
	}
	
	/* if it's a linked vma then now's time to copy the page so this
	   process can have its own private copy */
	if(vma->refcount > 1)
	{
		unsigned int loop, loopmax, thisphys, phys;
		process *search;
		
		/* if there's nothing to copy, then have a new private blank page */
		if(!(flags & VMA_HASPHYS))
		{
			unlock_gate(&(vma->lock), LOCK_READ);
			return newpage;
		}
		
		/* to avoid leaking a page of phys mem, only clone if there are two
		   or more processes (including this one) sharing one physical page */
		if(pg_user2phys(&thisphys, proc->pgdir, addr))
		{
			KOOPS_DEBUG("[vmm:%i] OMGWTF page claimed to have physical memory - but doesn't\n", CPU_ID);
			unlock_gate(&(vma->lock), LOCK_READ);
			return badaccess;
		}
		
		loopmax = vma->refcount;
		for(loop = 0; loop < loopmax; loop++)
		{
			search = vma->users[loop];
			if(search)
				/* this process doesn't count in the search */
				if(search != proc)
					if(pg_user2phys(&phys, search->pgdir, addr) == success)
						if(phys == thisphys)
						{
							unlock_gate(&(vma->lock), LOCK_READ);
							return clonepage;
						}
		}
		
		/* it's writeable, it's present, we're the only ones using it.. */
		unlock_gate(&(vma->lock), LOCK_READ);
		return makewriteable;
	}

	/* if it's single-linked and has its own physical memory and writes are
	   allowed then it's a page left over from a copy-on-write, so just
	   mark it writeable */
	if(vma->refcount == 1)
	{	
		/* but only if there's physical memory to write onto */
		if(flags & VMA_HASPHYS)
		{
			unlock_gate(&(vma->lock), LOCK_READ);
			return makewriteable;
		}
	
		/* but if there's no physical memory and it's single-linked and
		   writes are allowed then allocate some memory for this page */
		unlock_gate(&(vma->lock), LOCK_READ);
		return newpage;
	}

	/* if this access satisfies no other cases then give up and fail it */
	unlock_gate(&(vma->lock), LOCK_READ);
	return badaccess;
}
