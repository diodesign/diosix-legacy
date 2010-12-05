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
   unsigned int required_capacity, *addr_word = (unsigned int *)addr;
   kheap_block *block;
      
   lock_gate(&(vmm_lock), LOCK_WRITE);
   
   /* round up size to a fixed multple, including our block header. we round up to a set size
      (default 64 bytes) to reduce fragmentation and aid quick realloc'ing */
   required_capacity = KHEAP_ROUND_UP(size);

   VMM_DEBUG("[vmm:%i] request to allocate %i bytes (rounded up to %i)\n", CPU_ID,
             size, required_capacity);
   
   /* scan through free list to find the first block that will fit the
      requested size */
   block = kheap_free;
   while(block)
   {
      if(block->capacity >= required_capacity)
         break;
      block = block->next;
   }

   /* if block is set, then we've found something suitable in the free
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
   else
   /* if block is unset then we haven't found a suitable block (or the free
      list is empty). so allocate enough pages for the request */
   {
      /* try using high memory first */
      int pg_count, type = MEM_HIGH_PG;
      kresult result = vmm_ensure_pgs(required_capacity, type);
      if(result)
      {
         /* try using low memory */
         type = MEM_LOW_PG;
         result = vmm_ensure_pgs(required_capacity, type);
         if(result)
         {
            VMM_DEBUG("[vmm:%i] failed to grab physical pages for kernel heap (req size %i bytes)\n",
                    CPU_ID, required_capacity);
            unlock_gate(&(vmm_lock), LOCK_WRITE);
            return result; /* give up otherwise */
         }
      }
      
      /* by now, we've verified that we have a run of pages so grab them */
      for(pg_count = 0; pg_count < ((required_capacity / MEM_PGSIZE) + 1); pg_count++)
      {
         result = vmm_req_phys_pg((void **)&block, type); /* get pages in reverse order */
         if(result)
         {
            VMM_DEBUG("[vmm:%i] failed to grab physical page for kernel heap\n", CPU_ID);
            unlock_gate(&(vmm_lock), LOCK_WRITE);
            return result; /* XXX shouldn't happen - memory leak */
         }
      }

      VMM_DEBUG("[vmm:%i] grabbed %i pages for %i bytes (block %p)\n", CPU_ID, pg_count, required_capacity, block);
      
      /* don't forget to convert from physical to kernel's logical */
      block = KERNEL_PHYS2LOG(block);
      
      block->capacity = (unsigned int)MEM_PGALIGN(required_capacity) + MEM_PGSIZE;
      VMM_DEBUG("[vmm:%i] created block %p (max %i) in heap\n", CPU_ID, block, block->capacity); 
   }

   block->magic = KHEAP_INUSE;
   block->inuse = size;

   /* trim off excess memory and add this to the free list if there's enough to make
      at least a minimum-sized block */
   vmm_trim(block);
   
   /* write pointer to the start of data and the block's header details */
   *addr_word = (unsigned int)block + sizeof(kheap_block);

   /* add to head of allocated link list */
   block->next = kheap_allocated;
   if(kheap_allocated) kheap_allocated->previous = block;
   block->previous = NULL;
   kheap_allocated = block;

   unlock_gate(&(vmm_lock), LOCK_WRITE);

   VMM_DEBUG("[vmm:%i] allocated block %p addr %p capacity %i inuse %i magic %x next %p prev %p\n",
             CPU_ID, block, *addr_word, block->capacity, block->inuse, block->magic,
             block->next, block->previous);
   
   return success;
}

/* vmm_trim
   If the capacity of a heap block exceeds the allocated bytes by at least one multiple of the
   minimum-block size, then trim off the end of the block and release it into the free pool
   => block = heap block to operate on
   <= 0 for success (which does not indicate any memory was freed) or an error code
*/
kresult vmm_trim(kheap_block *block)
{
   /* sanity checks */
   if(block->magic != KHEAP_INUSE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_trim: Bad magic block %p to trim (magic %x)\n",
                  CPU_ID, block, block->magic);
      return e_bad_magic;
   }
   
   if(KHEAP_ROUND_UP(block->inuse) > block->capacity)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_trim: allocated bytes %i (rounded-up %i) in block %p exceeds capacity %i\n",
                  CPU_ID, block->inuse, KHEAP_ROUND_UP(block->inuse), block, block->capacity);
      return e_failure;
   }
   
   unsigned int difference = block->capacity - KHEAP_ROUND_UP(block->inuse);
   
   VMM_DEBUG("[vmm:%i] trimming block %p capacity %i inuse %i rounded up to %i diff %i\n",
             CPU_ID, block, block->capacity, block->inuse, KHEAP_ROUND_UP(block->inuse), difference);
   
   if(difference >= KHEAP_BLOCK_MULTIPLE)
   {
      kheap_block *extra = (kheap_block *)((unsigned int)block + KHEAP_ROUND_UP(block->inuse));
      
      extra->magic = KHEAP_INUSE; /* keep vmm_heap_add_to_free() happy */
      extra->capacity = difference;
      extra->inuse = 0;
      
      block->capacity -= extra->capacity; /* old block just got smaller */
      vmm_heap_add_to_free(extra);
      
      VMM_DEBUG("[vmm:%i] trimmed %i bytes off the end and freed. new capacity %i\n",
                CPU_ID, extra->capacity, block->capacity);
   }
   
   return success;
}

/* vmm_heap_add_to_free
   Add the given kernel heap block to the free list. Keep the list in order of
   base address, lowest first (at the head) and ascending. Then, scan the free
   list and merge adjoining blocks together.
   => pointer to heap block
*/
void vmm_heap_add_to_free(kheap_block *block)
{
   /* sanity check */
   if(block->magic != KHEAP_INUSE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! Tried to free non-allocated block at %p (magic %x)\n",
                  CPU_ID, block, block->magic);
      return;
   }
   
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

   /* reset metadata */
   block->magic = KHEAP_FREE;
   block->inuse = 0;

   /* merge adjoining blocks */
   block_loop = kheap_free;
   while(block_loop)
   {
      kheap_block *target = block_loop->next;
      if(!target) break; /* sanity check next ptr */

      if((unsigned int)target == ((unsigned int)block_loop + block_loop->capacity))
      {
         block_loop->next = target->next;
         if(target->next)
            target->next->previous = block_loop;
         block_loop->capacity += target->capacity;
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
      if(block->magic == KHEAP_FREE)
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_free: double free detetced on block %x\n",
                     CPU_ID, block);
      }
      else
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_free: block %x has wrong magic %x\n",
                     CPU_ID, block, block->magic);
      }
      debug_stacktrace();
      return e_bad_magic;
   }

   lock_gate(&(vmm_lock), LOCK_WRITE);

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
   
   VMM_DEBUG("[vmm:%i] freed heap block %p (addr %p)\n", CPU_ID, block, addr);
   
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
   kheap_block *block;
   
   /* do a straight vmm_malloc() call if addr is NULL */
   if(!addr)
   {      
      if(change < 1)
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_realloc: tried to alloc a block of size %i bytes\n",
                     CPU_ID, change);
         return NULL; /* failed, reject nonsense allocations */
      }
      
      if(vmm_malloc(&new, change) == success)
         return new; /* we succeeded, send back the pointer */
      else
         return NULL; /* failed to allocate */     
   }
   
   block = (kheap_block *)((unsigned int)addr - sizeof(kheap_block));
   
   /* sanity checks */
   if(!block)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_realloc: tried to alter size of dereferenced block by %i bytes\n",
                  CPU_ID, change);
      return NULL; /* failed */
   }
   
   if(block->magic != KHEAP_INUSE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_realloc: bad magic (%x) reallocing block %p by %i bytes\n",
                  CPU_ID, block->magic, change);
      debug_stacktrace();
      return NULL; /* failed */
   }
   
   /* a change of zero bytes will always be successful once we've ascertained that the
      block pointer is valid */
   if(change == 0) return addr;
   
   if(change < 0)
   {
      /* we can't shrink a block to and beyond zero */
      if((0 - change) >= block->inuse)
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF tried to shrink block %p of inuse size %i by %i bytes\n",
                     CPU_ID, block, block->inuse, 0 - change);
         return NULL; /* failed */
      }
   }

   /* sanity checks passed, try to grow/shrink the block within the padding of the block */
   if(KHEAP_ROUND_UP(block->inuse + change) <= block->capacity)
   {
      /* update the size stats and return the same pointer */
      block->inuse += change;
      
      VMM_DEBUG("[vmm:%i] realloc'd block %p to %i bytes within capacity %i\n",
                CPU_ID, block, block->inuse, block->capacity);
      
      vmm_trim(block); /* attempt to free up leftover memory */
      return addr;
   }
   
   /* we can't extend within our block padding so it's time to malloc-copy-free */
   if(vmm_malloc((void **)&new, block->inuse + change) == success)
   {
      /* so far so good, copy the contents of the old block to the new one,
         free the old block and return the new pointer */
      if(change > 0)
         vmm_memcpy(new, addr, block->inuse);
      else
         vmm_memcpy(new, addr, block->inuse + change);
      vmm_free(addr);
      return new;
   }
   
   /* fall through to a remalloc failure */
   return NULL;
}

/* -------------------------------------------------------------------------
    Memory pool management
   ------------------------------------------------------------------------- */

/* vmm_create_pool
   Create a memory pool, which is made up of pre-allocated equal sized blocks
   that can be allocated and freed very quickly.
   => block_size = size of each block in the pool in bytes
      init_count = number of free blocks to initialise the pool with
   <= pointer to kpool struct, or NULL for failure
*/
kpool *vmm_create_pool(unsigned int block_size, unsigned int init_count)
{
   kpool *new;
   unsigned int initsize;
   
   /* some sanity checking - non-zero params only */
   if(!block_size || block_size > KPOOL_MAX_BLOCKSIZE) return NULL;
   if(!init_count) return NULL;
   if(init_count > KPOOL_MAX_INITCOUNT) init_count = KPOOL_MAX_INITCOUNT;
   
   /* calculate initial size of the pool */
   initsize = KPOOL_BLOCK_TOTALSIZE(block_size) * init_count;
   
   /* allocate a new pool structure and zero it */
   if(vmm_malloc((void **)&new, sizeof(kpool))) return NULL;
   vmm_memset((void *)new, 0, sizeof(kpool));
   
   /* allocate the pool's heap block and zero it */
   if(vmm_malloc((void **)&(new->pool), initsize))
   {
      vmm_free(new);
      return NULL;
   }
   vmm_memset((void *)new->pool, 0, initsize);
   
   /* fill in its details */
   new->block_size   = block_size;
   new->total_blocks = init_count;
   if(vmm_create_free_blocks_in_pool(new, 0, init_count))
   {
      /* something's gone very wrong... */
      vmm_free(new->pool);
      vmm_free(new);
      return NULL;
   }
   
   VMM_DEBUG("[vmm:%i] created new pool %p (block size %i initial blocks %i heap %p)\n", CPU_ID,
             new, new->block_size, new->total_blocks, new->pool);
   
   return new;
}

/* vmm_destroy_pool
   Destroy a previously created pool, freeing its pool
   => pool = pointer to pool structrue to teardown
   <= 0 for success, or an error code
*/
kresult vmm_destroy_pool(kpool *pool)
{
   /* sanity checks */
   if(!pool) return e_bad_params;
   
   /* mark the lock as invalid */
   if(lock_gate(&(pool->lock), LOCK_WRITE | LOCK_SELFDESTRUCT))
      return e_failure;
   
   /* pretty easy stuff */
   if(pool->pool)
      vmm_free(pool->pool);
   vmm_free(pool);
   
   unlock_gate(&(pool->lock), LOCK_WRITE | LOCK_SELFDESTRUCT);
   
   VMM_DEBUG("[vmm:%i] destroyed pool %p\n", CPU_ID, pool);
   
   return success;
}

/* vmm_fixup_moved_pool
   If a pool's heap moves then all its pointers will need fixing up
   so that they point into the new heap block.
   => pool = pool structure to operate on
      prev = previous base address of the heap
      new = new base address of the heap
   <= 0 for success, or an error code
*/
kresult vmm_fixup_moved_pool(kpool *pool, void *prev, void *new)
{
   kpool_block *block;
   unsigned int *ptr;
   signed int offset = (unsigned int)new - (unsigned int)prev;
   
   /* sanity checks */
   if(!pool || !prev || !new) return e_bad_params;
   
   /* assumes we have a write lock on the pool */
   /* fix up the header's pointers */
   if(pool->head)
      pool->head = (kpool_block *)((unsigned int)(pool->head) + offset);
   if(pool->tail)
      pool->tail = (kpool_block *)((unsigned int)(pool->tail) + offset);
   if(pool->free)
      pool->free = (kpool_block *)((unsigned int)(pool->free) + offset);

   /* fix in-use list */
   block = pool->head;
   while(block)
   {
      ptr = (unsigned int *)&(block->next);
      if(*ptr) *ptr = *ptr + offset;
      
      ptr = (unsigned int *)&(block->previous);
      if(*ptr) *ptr = *ptr + offset;
      
      /* find next block */
      block = block->next;
   }
   
   /* fix free list */
   block = pool->free;
   while(block)
   {
      ptr = (unsigned int *)&(block->next);
      if(*ptr) *ptr = *ptr + offset;
      
      ptr = (unsigned int *)&(block->previous);
      if(*ptr) *ptr = *ptr + offset;
      
      /* find next block */
      block = block->next;
   }
   
   VMM_DEBUG("[vmm:%i] fixed up pool %p (moved %i bytes)\n", CPU_ID,
             pool, offset);
   
   return success;
}

/* vmm_alloc_pool
   Get a block from a pool. If there are no free blocks available, 
   the pool will be automatically expanded to create free blocks.
   The allocated block is always added to the end of the in-use list,
   allowing the pool's alloc'd blocks to be used quickly as a queue. 
   => ptr = pointer to a pointer in which the address of the block will
            be stored. The address is the first byte in the block.
      pool = pointer to the pool structure from which to allocate
   <= 0 for success, or an error code
*/
kresult vmm_alloc_pool(void **ptr, kpool *pool)
{
   kpool_block *new;
   
   /* sanity checks */
   if(!ptr || !pool) return e_bad_params;
   
   lock_gate(&(pool->lock), LOCK_WRITE);

   /* are there any free blocks? if not, we need more memory... */
   if(!pool->free_blocks)
   {
      void *prev_heap_addr = pool->pool;
      void *new_heap_addr;
      
      unsigned int start_of_free;
      unsigned int pool_blocks = pool->total_blocks;
      unsigned int pool_size = KPOOL_BLOCK_TOTALSIZE(pool->block_size) * pool_blocks;

      /* grow the pool by doubling it in size: increae it by pool_size bytes */
      new_heap_addr = vmm_realloc(pool->pool, pool_size);
      
      /* give up if we can't grow the pool */
      if(!new_heap_addr)
      {
         unlock_gate(&(pool->lock), LOCK_WRITE);
         return e_failure;
      }

      /* fix up the pool's pointers if the pool heap moved */
      if(prev_heap_addr != new_heap_addr)
      {
         pool->pool = new_heap_addr; /* save the new address */
         vmm_fixup_moved_pool(pool, prev_heap_addr, new_heap_addr);
      }
      
      /* zero the new area and mark it as free */
      start_of_free = (unsigned int)(pool->pool) + pool_size;
      vmm_memset((void *)start_of_free, 0, pool_size);
      pool->total_blocks += pool_blocks;
      vmm_create_free_blocks_in_pool(pool, pool_blocks, pool_blocks * 2);
      
      VMM_DEBUG("[vmm:%i] grew pool %p by %i bytes (heap now at %p)\n", 
                CPU_ID, pool, pool_size, pool->pool);
   }
   
   /* grab the next free block */
   new = pool->free;
   if(new->magic != KPOOL_FREE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_alloc_pool: block %p in pool %p is "
                  "in the free list but not marked as free (magic %x)\n",
                  CPU_ID, new, pool, new->magic);
      unlock_gate(&(pool->lock), LOCK_WRITE);
      return e_failure;
   }
   
   /* mark it as in-use */
   new->magic = KPOOL_INUSE;
   
   /* remove it from the free list */
   pool->free = new->next;
   if(pool->free) pool->free->previous = NULL;
   
   /* add it to the end of the in-use list */
   if(pool->tail)
   {
      new->previous = pool->tail;
      new->next = NULL;
      pool->tail->next = new;
      pool->tail = new;
   }
   else
   {
      /* we've got no tail */
      pool->tail = new;
      pool->head = new;
      new->next = new->previous = NULL;
   }
   
   /* update pool statistics */
   pool->inuse_blocks++;
   pool->free_blocks--;
   
   /* write out the pointer */
   *ptr = (void *)((unsigned int)new + sizeof(kpool_block));
   
   unlock_gate(&(pool->lock), LOCK_WRITE);
   
   VMM_DEBUG("[vmm:%i] allocated block %p (addr %x) from pool %p\n", 
             CPU_ID, new, *ptr, pool);
   
   return success;
}

/* vmm_free_pool
   Free a previously allocated block within a pool, allowing it to
   be reused later.
   => ptr = address of block to free
      pool = pointer to pool structrue the block belongs to
   <= 0 for success, or an error code
*/
kresult vmm_free_pool(void *ptr, kpool *pool)
{
   kpool_block *block;
   
   /* sanity checks */
   if(!ptr || !pool) return e_bad_params;
   
   lock_gate(&(pool->lock), LOCK_WRITE);
   
   block = (kpool_block *)((unsigned int)ptr - sizeof(kpool_block));
   if(block->magic != KPOOL_INUSE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF vmm_free_pool: tried to free non-inuse block %x "
                  "in pool %p (magic %x)\n",
                  CPU_ID, block, pool, block->magic);
      debug_stacktrace();
      unlock_gate(&(pool->lock), LOCK_WRITE);
      return e_bad_params;
   }
   
   /* remove it from the in-use list */
   if(block->previous)
      block->previous->next = block->next;
   if(block->next)
      block->next->previous = block->previous;
   if(pool->head == block)
   {
      pool->head = block->next;
      if(pool->head) pool->head->previous = NULL;
   }
   if(pool->tail == block)
   {
      pool->tail = block->previous;
      if(pool->tail) pool->tail->next = NULL;
   }
   
   /* add to the start of the free list */
   if(pool->free)
      pool->free->previous = block;
   block->next = pool->free;
   pool->free = block;
   block->previous = NULL;
   
   /* change the magic in the block */
   block->magic = KPOOL_FREE;
   
   /* update pool statistics */
   pool->inuse_blocks--;
   pool->free_blocks++;
 
   unlock_gate(&(pool->lock), LOCK_WRITE);
   
   VMM_DEBUG("[vmm:%i] freed block %p (addr %x) in pool %p (head %p tail %p)\n",
             CPU_ID, block, ptr, pool, pool->head, pool->tail);
   
   return success;
}

/* vmm_create_free_blocks_in_pool
   Mark a range of blocks in the pool as free and ready to use
   and add to the pool's free list.
   => pool = pointer to pool structure to work on
      start = index of first block to mark as free, starting from zero
      end = (index + 1) of the last block to mark as free
 <= 0 for success, or an error code
*/
kresult vmm_create_free_blocks_in_pool(kpool *pool, unsigned int start, unsigned int end)
{
   unsigned int loop, addr, total_block_size;
   kpool_block *block;
   
   /* sanity checks */
   if(!pool) return e_bad_params;
   if(end > pool->total_blocks || start >= end) return e_bad_params;
   if(start > pool->total_blocks) return e_bad_params;
   
   /* size of block including kpool_block header */
   total_block_size = KPOOL_BLOCK_TOTALSIZE(pool->block_size);
   
   /* start address of the pool */
   addr = ((unsigned int)pool->pool) + (start * total_block_size);
   
   /* loop through blocks */
   for(loop = start; loop < end; loop++)
   {
      block = (kpool_block *)addr;
      
      /* fill in block's details - assumes all
         the struct's field have already been zero'd */
      block->magic = KPOOL_FREE;
      block->next = pool->free;

      /* update the pool - assumes pool is locked for update 
         by the calling function! */
      pool->free = block;
      pool->free_blocks++;
      
      addr += total_block_size;
   }
   
   VMM_DEBUG("[vmm:%i] initialised free blocks %i to %i in pool %p\n",
             CPU_ID, start, end - 1, pool);
   
   return success;
}

/* vmm_next_in_pool
   Return the address of the first byte of the next in-use block in the
   pool. Useful for building a queue system.
   => ptr = address of in-use block to use to derive the next in-use block
            or NULL to get the first block
      pool = pointer to pool structure
   <= address of first byte of next in-use block or NULL for failure or
      end of in-use list
*/
void *vmm_next_in_pool(void *ptr, kpool *pool)
{
   kpool_block *block;
   void *result = NULL;
   
   /* sanity checks */
   if(!pool) return NULL;
   
   lock_gate(&(pool->lock), LOCK_READ);
   
   /* give out the first block if asked for */
   if(!ptr)
   {
      /* don't forget to fix up head pointer before returning it */
      if(pool->head)
         result = (void *)((unsigned int)pool->head + sizeof(kpool_block));
      goto vmm_next_in_pool_exit;
   }
   
   block = (kpool_block *)((unsigned int)ptr - sizeof(kpool_block));
   if(block->magic != KPOOL_INUSE)
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! vmm_next_in_pool: 'inuse' block %p (addr %x) in pool %p has bad magic (%x)\n",
                  CPU_ID, block, ptr, pool, block->magic);
      debug_stacktrace();
      goto vmm_next_in_pool_exit; /* return NULL */
   }
   
   if(block->next)
      result = (void *)((unsigned int)block->next + sizeof(kpool_block));

vmm_next_in_pool_exit:
   unlock_gate(&(pool->lock), LOCK_READ);
   return result; /* result still set to NULL unless otherwise modified */
}

/* vmm_count_pool_inuse
   Count how many inuse blocks there are in a pool.
   => pool = pool structrue to investigate
   <= number of inuse blocks, or zero for a failure - ambiguous :(
*/
unsigned int vmm_count_pool_inuse(kpool *pool)
{
   /* sanity check */
   if(!pool) return 0;
   
   return pool->inuse_blocks;
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
      {
         unlock_gate(&(vmm_lock), LOCK_WRITE);
         return e_no_phys_pgs;
      }
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
   
   return 0;
}

/* vmm_nullbufferlen
   Return the length of a NULL-terminated buffer in a number of bytes
   => buffer = pointer to NULL-terminated buffer
   <= returns size in bytes or possibly 0 if an error occured
*/
unsigned int vmm_nullbufferlen(char *buffer)
{
   char *search = buffer;
   
   /* sanity check - perhaps a better way of raising an error should be found */
   if(!search) return 0;
   
   /* search for the NULL byte */
   while(*search)
      search++;
   
   return (unsigned int)search - (unsigned int)buffer;
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
   
   /* sanity checks */
   if(!target || !source || !count) return;
   
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
   unsigned int ktarget, ksource;
   
   if((unsigned int)target + MEM_CLIP(target, count) >= KERNEL_SPACE_BASE)
   {
      /* copying into kernel, tproc must be NULL */
      if(tproc) goto vmm_memcpyuser_wtf;
      ktarget = (unsigned int)target;
   }
   else
   {
      if(pg_user2kernel(&ktarget, (unsigned int)target, tproc))
         return e_bad_target_address;
   }

   if((unsigned int)source + MEM_CLIP(source, count) >= KERNEL_SPACE_BASE)
   {
      /* copying from kernel, sproc must be NULL */
      if(sproc) goto vmm_memcpyuser_wtf;
      ksource = (unsigned int)source;
   }
   else
   {
      if(pg_user2kernel(&ksource, (unsigned int)source, sproc))
         return e_bad_source_address;
   }
   
   /* perform the copy with sane virtual addresses */
   vmm_memcpy((void *)ktarget, (void *)ksource, count);
   return success;
   
   /* flag up a broken attempt to use this function */
vmm_memcpyuser_wtf:
   KOOPS_DEBUG("[vmm:%i] OMGWTF: vmm_memcpyuser has bad params!\n"
               "                target = %p (proc %p)\n"
               "                source = %p (proc %p)\n"
               "                copying: %i bytes\n",
               CPU_ID, target, tproc, source, sproc, count);
   debug_stacktrace();
   return e_bad_params;
}

/* -------------------------------------------------------------------------
   Process memory area management
   Using SGLib: http://sglib.sourceforge.net/doc/index.html
 ------------------------------------------------------------------------- */

/* this should return a negative number if a given address x falls below
   the base of y, zero if it falls within the base and size of y, or positive to
   indicate it's greater than base+size of y.

   xmin = x->base;  xmax = x->base + x->area->size;
   ymin = y->base;  ymax = y->base + y->area->size;
 
 Possible combinations of areas...                           vma_cmp() result
 ..................ymin------------------ymax............... 
 ..xmin###xmax.............................................. allowed     (-1)
 ..xmin##################xmax............................... not allowed (0)
 ........................xmin####xmax....................... not allowed (0)
 ................................xmin###########xmax........ not allowed (0)
 ...............................................xmin####xmax allowed     (1)
 ..xmin#########################################xmax........ not allowed (0)
*/
#define vma_min(x) (x->base)
#define vma_max(x) (x->base + x->area->size)
#define vma_cmp(x, y) ( (vma_max(x) - 1) < vma_min(y) ? -1 : ( vma_min(x) >= vma_max(y) ? 1 : 0 ) )

/* pointers to vmas are stored in a per-process balanced (rb) binary tree */
SGLIB_DEFINE_RBTREE_PROTOTYPES(vmm_tree, left, right, colour, vma_cmp);
SGLIB_DEFINE_RBTREE_FUNCTIONS(vmm_tree, left, right, colour, vma_cmp);

/* vmm_link_vma
   Link an existing area to a process
   => proc = process to link the vma with
      baseaddr = base address of the vma in the process's virtual space. 
                 this will be rounded down to the nearest page boundary.
      vma = the vma to link
   <= success or a failure code
*/
kresult vmm_link_vma(process *proc, unsigned int baseaddr, vmm_area *vma)
{
   kresult err;
   vmm_tree *new, *existing;
   
   baseaddr = baseaddr & ~MEM_PGMASK; /* round down */

   /* sanity checks - no null pointers, or kernel or zero page mappings */
   if(!proc || !baseaddr || !vma || baseaddr >= KERNEL_SPACE_BASE) return e_bad_params;
   
   /* allocate and zero memory for the new tree node */
   err = vmm_malloc((void **)&new, sizeof(vmm_tree));
   if(err) return err;
   vmm_memset(new, 0, sizeof(vmm_tree));
   
   /* set the area pointer and base address */
   new->area = vma;
   new->base = baseaddr;
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   /* and try to add to the tree */
   if(sglib_vmm_tree_add_if_not_member(&(proc->mem), new, &existing))
   {
      vmm_area_mapping *new_mapping;
      
      lock_gate(&(vma->lock), LOCK_WRITE);

      /* allocate a new block to store this mapping in */
      err = vmm_alloc_pool((void **)&new_mapping, vma->mappings);
      if(!err)
      {         
         /* save the process pointer and base address */
         new_mapping->proc = proc;
         new_mapping->base = baseaddr;
         
         /* non-zero return for success */
         err = success;
         
         VMM_DEBUG("[vmm:%i] linked vma %p (addr %x) to process %i (%p) via tree node %p\n",
                 CPU_ID, vma, baseaddr, proc->pid, proc, new);
      }
      
      unlock_gate(&(vma->lock), LOCK_WRITE);
   }
   else
   {
      /* the vma already exists or collides with an area */
      err = e_vma_exists;
      vmm_free(new);
      
      VMM_DEBUG("[vmm:%i] couldn't link vma %p to process %i (%p) - collision with vma %p (base %x size %x)\n", 
              CPU_ID, vma, proc->pid, proc, existing->area, existing->base, existing->area->size);
   }
   
   unlock_gate(&(proc->lock), LOCK_WRITE);
   
   return err;
}

/* vmm_unlink_vma
   Unlink an existing area from a process and destroy the vma if 
   it's no longer in use by any processes.
   => owner = process to unlink the vma from
      victim = the vma to unlink
   <= success or a failure code
*/
kresult vmm_unlink_vma(process *owner, vmm_tree *victim)
{
   lock_gate(&(owner->lock), LOCK_WRITE);
   
   vmm_area_mapping *mapping;
   unsigned int page_loop, release_flag = 1;
   vmm_area *vma = victim->area;
   
   lock_gate(&(vma->lock), LOCK_WRITE);
   
   /* try to remove from the tree */
   sglib_vmm_tree_delete(&(owner->mem), victim);
   unlock_gate(&(owner->lock), LOCK_WRITE);
   
   /* remove any page mappings associated with this vma in this process.
      release the pages if they are shared and we're the last linked process -
      but only if the pages aren't in a direct physical mapping */
   if(vma->flags & VMA_FIXED)
      release_flag = 0;
   else
      if((vma->flags & VMA_SHARED) && (vmm_count_pool_inuse(vma->mappings) > 1))
         release_flag = 0;
   
   for(page_loop = 0; page_loop < vma->size; page_loop += MEM_PGSIZE)
      pg_remove_4K_mapping(owner->pgdir, victim->base + page_loop, release_flag);
   
   /* delete from the vma's users pool */
   mapping = vmm_find_vma_mapping(vma, owner);
   if(mapping)
      vmm_free_pool(mapping, vma->mappings);
   else
   {
      KOOPS_DEBUG("[vmm:%i] OMGWTF! tried removing process %p (pid %i) from vma %p where no mapping existed\n",
                  CPU_ID, owner, owner->pid, vma);
   }
   
   /* free if there are no longer any processes mapping in this vma */
   if(!(vmm_count_pool_inuse(vma->mappings)))
   {
      unlock_gate(&(vma->lock), LOCK_WRITE | LOCK_SELFDESTRUCT);
      vmm_destroy_pool(vma->mappings);
      vmm_free(vma);
   }
   else
      unlock_gate(&(vma->lock), LOCK_WRITE);
      
   VMM_DEBUG("[vmm:%i] unlinked vma %p from tree node %p in process %i (%p)\n",
           CPU_ID, vma, victim, owner->pid, owner);
   
   return vmm_free(victim);
}

#define VMM_RESIZE_VMA_RETURN(a) { err = (a); goto vmm_resize_vma_exit; }

/* vmm_resize_vma
   Change the size of a vma within a process. A vma can only be resized if there is a sole owner.
   => owner = pointer to the owning process
      node = pointer to a vma's node within the process's virtual memory tree
      change = signed integer of number of bytes to expand by (or shrink by if negative).
               the new size of the vma will be rounded up to the nearest page boundary if increasing 
               or rounded down if shrinking.
   <= 0 for success or an error code
*/
kresult vmm_resize_vma(process *owner, vmm_tree *node, signed int change)
{
   kresult err = success;
   vmm_area *vma;
   
   /* sanity check parameters */
   if(!node || !owner) return e_bad_params;
   if(change == 0) return success; /* a resize of zero always succeeds */
   
   vma = node->area;
   
   /* give up if the vma is in use by more than one process - we must have 
      exclusive access to it or everything starts getting messy */
   lock_gate(&(owner->lock), LOCK_WRITE);
   lock_gate(&(vma->lock), LOCK_WRITE);
   
   if(vmm_count_pool_inuse(vma->mappings) > 1)
      VMM_RESIZE_VMA_RETURN(e_no_rights); /* permission denied to alter the vma */
   
   if(change > 0)
   {
      vmm_tree *check;
      /* positive change - round up to the nearest page bundary */
      change = MEM_PG_ROUND_UP(change - 1);
      
      /* stop the process mapping over the kernel */
      if(node->base + MEM_CLIP(node->base, change) >= KERNEL_SPACE_BASE)
         VMM_RESIZE_VMA_RETURN(e_too_big);
      
      /* check there's no collision with the area above in memory */
      check = vmm_find_vma(owner, node->base + node->area->size, change);
      if(check) VMM_RESIZE_VMA_RETURN(e_vma_exists);
      
      /* do the actual size change */
      vma->size += change;
      VMM_DEBUG("[vmm:%i] increased vma %p (base %x) in process %i by %i bytes\n",
                CPU_ID, vma, node->base, owner->pid, change);
   }
   else
   {
      unsigned int page_loop;
      unsigned int release_flag = 1;
      /* negative change - check we don't shrink to zero or beyond.
         round down to the nearest page boundary */
      unsigned int bytes = (0 - change) & ~MEM_PGMASK;
      if(bytes >= vma->size) VMM_RESIZE_VMA_RETURN(e_bad_params);
      
      vma->size -= bytes; /* do the actual size change */
      VMM_DEBUG("[vmm:%i] reduced vma %p (base %x) in process %i by %i bytes\n",
                CPU_ID, vma, node->base, owner->pid, bytes);

      /* don't release any mapped physical pages if there are multiple
         processes sharing this vma or if it's a direct physical mapping */
      if(vma->flags & VMA_FIXED)
         release_flag = 0;
      else
         if((vma->flags & VMA_SHARED) && (vmm_count_pool_inuse(vma->mappings) > 1))
            release_flag = 0;
      
      /* now we need to unmap any pages that might have been present or
         else the process can continue to access any previously mapped in
         pages.. and other processes will want to use the physical pages. */
      for(page_loop = 0; page_loop < bytes; page_loop += MEM_PGSIZE)
         pg_remove_4K_mapping(owner->pgdir, node->base + vma->size + page_loop, release_flag);
   }

vmm_resize_vma_exit:
   unlock_gate(&(vma->lock), LOCK_WRITE);
   unlock_gate(&(owner->lock), LOCK_WRITE);
   return err;
}

/* vmm_alter_vma
   Alter a vma's access flags.
   => owner = pointer to the owning process
      node = pointer to a vma's node within the process's virtual memory tree
      flags = new flags word for the vma
   <= 0 for success or an error code
*/
kresult vmm_alter_vma(process *owner, vmm_tree *node, unsigned int flags)
{
   kresult err = success;
   vmm_area *vma;
   
   /* sanity check parameters */
   if(!node || !owner) return e_bad_params;
   
   vma = node->area;
   
   /* give up if the vma is in use by more than one process - we must have 
    exclusive access to it or everything starts getting messy */
   lock_gate(&(owner->lock), LOCK_WRITE);
   lock_gate(&(vma->lock), LOCK_WRITE);
   
   if(vmm_count_pool_inuse(vma->mappings) == 1)
      vma->flags = flags;
   else
      err = e_no_rights; /* permission denied */
   
   unlock_gate(&(vma->lock), LOCK_WRITE);
   unlock_gate(&(owner->lock), LOCK_WRITE);
   return err;
}

/* vmm_find_vma_mapping
   Return a block in a vma's mappings pool that holds a pointer
   to the given process.
   => vma = vma to inspect
      tofind = process to find within the vma's mapping pool
   <= pointer to the block, or NULL for none
*/
vmm_area_mapping *vmm_find_vma_mapping(vmm_area *vma, process *tofind)
{
   vmm_area_mapping *search = NULL;
   
   /* sanity check */
   if(!vma || !tofind) return NULL;
   
   /* assumes we have at least a read lock on the vma.
      loop through the pool blocks looking for a mapping
      that matches the owner. if vmm_next_in_pool() returns
      NULL then we've run out of pool to check. */
   for(;;)
   {
      search = vmm_next_in_pool(search, vma->mappings);
      
      if(search)
      {
         if(search->proc == tofind)
            return search;
      }
      else return NULL;
   }
}

/* vmm_add_vma
   Create a new memory area and link it to a process
   => proc = process to link the new vma to
      base = base virtual address for this vma, rounded down to nearest page size multiple
      size = vma size in bytes. this will be rounded up to nearest page size multiple
      flags = vma status and access flags
      cookie = a private reference set by the userspace page manager
   <= success or a failure code
*/
kresult vmm_add_vma(process *proc, unsigned int base, unsigned int size,
                    unsigned char flags, unsigned int cookie)
{
   vmm_area *new;

   /* sanity check */
   if(!base || !size) return e_bad_params;
   
   /* round up the size and round down the base to 4K page multiples */
   size = MEM_PG_ROUND_UP(size - 1);
   base = base & ~MEM_PGMASK;
   
   /* block any attempt to map over the kernel */
   if(base + MEM_CLIP(base, size) >= KERNEL_SPACE_BASE) return e_bad_params;

   kresult err = vmm_malloc((void **)&new, sizeof(vmm_area));
   if(err) return err;
   vmm_memset(new, 0, sizeof(vmm_area)); /* zero the area */
   
   /* fill in the details */
   new->flags    = flags;
   new->size     = size;
   new->token    = cookie;
   
   new->mappings = vmm_create_pool(sizeof(vmm_area_mapping), 2);
   
   VMM_DEBUG("[vmm:%i] created vma %p for proc %i (%p): base %x size %i flags %x cookie %x\n",
           CPU_ID, new, proc->pid, proc, base, size, flags, cookie);
   
   err = vmm_link_vma(proc, base, new);
   if(err)
   {
      /* tear down this failed vma */
      vmm_destroy_pool(new->mappings);
      vmm_free(new);
      
      return err;
   }
   
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
      err = vmm_link_vma(new, node->base, node->area);
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
   Locate a vma tree node using the given address in the given proc. Any overlapping
   vmas will be returned or NULL for no vma.
   => proc = process to inspect
      addr = base address to check
      size = range of ranges (in bytes) to check from the base address
   <= pointer to tree node or NULL for not found
*/
vmm_tree *vmm_find_vma(process *proc, unsigned int addr, unsigned int size)
{
   vmm_tree node;
   vmm_area area;
   vmm_tree *result;
   
   /* give up now if we're given rubbish pointers */
   if(!proc || !addr) return NULL;

   /* mock up a vma and node to search for */
   area.size = size;
   node.base = addr;
   node.area = &area;
   
   lock_gate(&(proc->lock), LOCK_READ);
   result = sglib_vmm_tree_find_member(proc->mem, &node);
   unlock_gate(&(proc->lock), LOCK_READ);
   
   return result;
}

/* standardise return from vmm_fault() */
#define VMM_FAULT_RETURN(r) { unlock_gate(&(vma->lock), LOCK_READ); return (r); }

/* vmm_fault
   Make a decision on what to do with a faulting user process by using its
   vma tree and the flags associated with a located vma
   => proc = process at fault
      addr = virtual address where fault occurred
      flags = type of access attempted using the vma_area flags 
      rw_flag = pointer to an unsigned char set to 1 for a writeable area or preserved for read-only
   <= decision code
*/
vmm_decision vmm_fault(process *proc, unsigned int addr, unsigned char flags, unsigned char *rw_flag)
{
   lock_gate(&(proc->lock), LOCK_READ);

   vmm_area *vma;
   vmm_tree *found = vmm_find_vma(proc, addr, sizeof(char));
   
   if(!found) return badaccess; /* no vma means no possible access */
   
   vma = found->area;

   lock_gate(&(vma->lock), LOCK_READ);
   unlock_gate(&(proc->lock), LOCK_READ);
   
   VMM_DEBUG("[vmm:%i] fault at %x lies within vma %p (base %x size %i) in process %i\n",
           CPU_ID, addr, vma, found->base, vma->size, proc->pid);
   
   /* let the page system know whether or not it's a writeable area */
   if(vma->flags & VMA_WRITEABLE) *rw_flag = 1;
   
   /* fail this access if it's a write to a non-writeable area */
   if((flags & VMA_WRITEABLE) && !(vma->flags & VMA_WRITEABLE))
      VMM_FAULT_RETURN(badaccess);

   /* is this a shared page? if so, skip the copy-on-write system */
   if(vma->flags & VMA_SHARED) VMM_FAULT_RETURN(newsharedpage);
   
   /* defer to the userspace page manager if it is managing this vma */
   if(!(vma->flags & VMA_MEMSOURCE)) VMM_FAULT_RETURN(external);
   
   /* if it's a linked vma then now's time to copy the page so this
      process can have its own private copy */
   if(vmm_count_pool_inuse(vma->mappings) > 1)
   {
      unsigned int thisphys, phys;
      vmm_area_mapping *search;
    
      /* if it's a shared vma then skip the copy-on-write system */
      if(vma->flags & VMA_SHARED) VMM_FAULT_RETURN(newsharedpage);
      
      /* if there's nothing to copy, then have a new private blank page */
      if(!(flags & VMA_HASPHYS)) VMM_FAULT_RETURN(newpage);

      /* to avoid leaking a page of phys mem, only clone if there are two
         or more processes (including this one) sharing one physical page */
      if(pg_user2phys(&thisphys, proc->pgdir, addr))
      {
         KOOPS_DEBUG("[vmm:%i] OMGWTF page claimed to have physical memory - but doesn't\n", CPU_ID);
         VMM_FAULT_RETURN(badaccess);
      }
      
      search = NULL;
      for(;;)
      {
         search = vmm_next_in_pool(search, vma->mappings);
         
         if(search)
         {
            /* this process doesn't count in the search */
            if(search->proc != proc)
               if(pg_user2phys(&phys, search->proc->pgdir, addr) == success)
                  if(phys == thisphys)
                     VMM_FAULT_RETURN(clonepage);
         }
         else break;
      }
      
      /* it's writeable, it's present, we're the only ones using it.. */
      VMM_FAULT_RETURN(makewriteable);
   }

   /* if it's single-linked and has its own physical memory and writes are
      allowed then it's a page left over from a copy-on-write, so just
      mark it writeable */
   if(vmm_count_pool_inuse(vma->mappings) == 1)
   {   
      /* but only if there's physical memory to write onto */
      if(flags & VMA_HASPHYS) VMM_FAULT_RETURN(makewriteable);
   
      /* but if there's no physical memory and it's single-linked 
         then allocate some memory for this page */
      VMM_FAULT_RETURN(newpage);
   }

   /* if this access satisfies no other cases then give up and fail it */
   VMM_FAULT_RETURN(badaccess);
}
