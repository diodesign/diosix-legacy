/* kernel/core/locks.c
 * diosix portable lock management code
 * Author : Chris Williams
 * Date   : Thu,12 Apr 2012.07:28:51

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* in order to provide a sane way of managing locks in volatile
   memory areas - ie: non-cached, non-buffered - that are SMP-safe,
   locks must be allocated from pools of whole pages. Each MMU page
   will be configured by the arch-specific paging code to be volatile.
   Each 4K page can hold 256 x 16-byte lock gates, which must be
   allocated dynamically in an efficient way. And quickly too.
   
   Using the VMM's pool system could be possible, but it calls into
   the locking system to secure itself. instead we'll chain pages
   of physical memory with a bitmap array to mark when they're in use. */

rw_gate *lock_lock;

/* statically allocate the first pool structure */
rw_gate_pool gate_pool;

#ifdef LOCK_TIME_CHECK
/* locking system's optional timeout counter */
volatile unsigned int lock_time_check_lock = 0;
#endif

/* lock_bitmap_test
   Test the value of a bit within a char-array bitmap.
   Hint to the compiler that we would like this function inlined..
   => array = pointer to base of unsigned char array to inspect
      bit = bit number to inspect
   <= return value of the bit requested
*/
static __inline__ unsigned char lock_bitmap_test(unsigned char *array, unsigned char bit)
{
   /* assumes array pointer is sane */
   
   /* calculate how many bytes into the array we need to start from
      and the offset into the Nth byte to find the bit */
   unsigned int bytes = (bit >> 3);
   unsigned char remainder = bit - (bytes << 3);
   
   return (array[bytes] >> remainder) & 1;
}

/* lock_bitmap_set
   Set a bit within a char-array bitmap.
   Hint to the compiler that we would like this function inlined..
   => array = pointer to base of unsigned char array to access
      bit = bit number to set
*/
static __inline__ void lock_bitmap_set(unsigned char *array, unsigned char bit)
{
   /* assumes array pointer is sane */
   
   /* calculate how many bytes into the array we need to start from
    and the offset into the Nth byte to find the bit */
   unsigned int bytes = (bit >> 3);
   unsigned char remainder = bit - (bytes << 3);
   
   array[bytes] |= (1 << remainder);
}

/* lock_bitmap_clear
   Clear a bit within a char-array bitmap.
   Hint to the compiler that we would like this function inlined..
   => array = pointer to base of unsigned char array to access
      bit = bit number to set
*/
static __inline__ void lock_bitmap_clear(unsigned char *array, unsigned char bit)
{
   /* assumes array pointer is sane */
   
   /* calculate how many bytes into the array we need to start from
    and the offset into the Nth byte to find the bit */
   unsigned int bytes = (bit >> 3);
   unsigned char remainder = bit - (bytes << 3);
   
   array[bytes] &= ~(1 << remainder);
}

/* lock_gate_alloc
   Allocate a readers-writer gate structure
   => ptr = pointer to word in which to store address of allocated gate
      description = NULL-terminated human-readable label 
   <= 0 for success, or an error code (and ptr will be set to NULL)
*/
kresult lock_rw_gate_alloc(rw_gate **ptr, char *description)
{
   kresult err;
   rw_gate_pool *search = &gate_pool;
   unsigned char slot_search_start, slot_search;
   unsigned char slot_found = 0;
   rw_gate *new_gate;

   /* sanity check */
   if(!ptr || !description) return e_bad_params;
   if(vmm_nullbufferlen(description) >= LOCK_DESCRIPT_LENGTH)
      return e_bad_params;
   
   /* lock the locking code */
   lock_gate(lock_lock, LOCK_WRITE);
   
   /* try to find an existing pool with free slots */
   while(search && search->nr_free == 0)
      search = search->next;
   
   /* if search is still NULL then grab a new page for the pool */
   if(!search)
   {
      void *new_page;
      err = vmm_req_phys_pg(&new_page, 1);
      if(err)
      {
         unlock_gate(lock_lock, LOCK_WRITE);
         return err; /* bail out if we're out of pages! */
      }
      
      /* allocate a new structure to describe this pool or give up */
      err = vmm_malloc((void **)&search, sizeof(rw_gate_pool));
      if(err)
      {
         unlock_gate(lock_lock, LOCK_WRITE);
         return err; /* bail out if we're out of memory! */
      }
      
      /* initialise the pool structure and add to the head of
         the linked list so it can be found quickly */
      vmm_memset(search, 0, sizeof(rw_gate_pool));
      search->nr_free = (unsigned char)LOCK_POOL_BITMAP_LENGTH_BITS;
      search->physical_base = new_page;
      search->virtual_base = KERNEL_PHYS2LOG(new_page);
      
      vmm_memset(KERNEL_PHYS2LOG(new_page), 0, MEM_PGSIZE);
            
      /* add us to the start of the list */
      if(gate_pool.next)
      {
         search->next = gate_pool.next;
         search->next->previous = search;
      }

      gate_pool.next = search;
      search->previous = &gate_pool;
   }
   
   /* search is now valid or we wouldn't be here, so locate a free slot by
      scanning through the bitmap, starting at the last known free slot */
   slot_search = slot_search_start = search->last_free;
   do
   {
      if(lock_bitmap_test(search->bitmap, slot_search) == 0)
         slot_found = 1;
      
      slot_search++;
      if(slot_search >= LOCK_POOL_BITMAP_LENGTH_BITS)
         slot_search = 0;
   }
   while(slot_search_start != slot_search && !slot_found);
   
   /* set the bit and grab the address of the slot to use for the lock */
   if(slot_found)
   {
      lock_bitmap_set(search->bitmap, slot_search);
      
      /* initialise the gate structure */
      new_gate = (rw_gate *)(unsigned int)search->virtual_base + (sizeof(rw_gate) * slot_search);
      vmm_memset(new_gate, 0, (sizeof(rw_gate)));
      *ptr = new_gate;
      
#ifdef DEBUG_LOCK_RWGATE_PROFILE
      vmm_memcpy(&(new_gate->description), description, vmm_nullbufferlen(description) + sizeof('\0'));
#endif
                 
      /* update accounting, increment the next free slot but beware of
         overflow */
      search->nr_free--;
      search->last_free++;
      if(search->last_free >= LOCK_POOL_BITMAP_LENGTH_BITS)
         slot_search = 0;
      
      LOCK_DEBUG("[lock:%i] created new readers-writer lock '%s' %p (spinlock %p)\n",
                 description, new_gate, new_gate->spinlock);
      
      unlock_gate(lock_lock, LOCK_WRITE);
      return success;
   }
   
   /* something weird has happened if we've fallen this far, so
      fall through to failure */
   unlock_gate(lock_lock, LOCK_WRITE);
   *ptr = NULL;
   return e_failure;
}

/* lock_gate_free
   Free a readers-writer gate structure
   => ptr = pointer to address of allocated gate
   <= 0 for success, or an error code
*/
kresult lock_rw_gate_free(rw_gate *gate)
{
   void *page_to_find = MEM_PGALIGN(gate);
   unsigned char slot = ((unsigned int)gate - (unsigned int)page_to_find) / sizeof(rw_gate);
   
   /* sanity check */
   if(!gate) return e_failure;
   
   /* locate the gate */
   rw_gate_pool *search = &gate_pool;

   LOCK_DEBUG("[lock:%i] freeing readers-writer gate %p\n", CPU_ID, gate); 
   
   /* lock the locking code */
   lock_gate(lock_lock, LOCK_WRITE);
   
   while(search)
   {
      if(search->virtual_base == page_to_find) break;
      search = search->next;
   }
   
   /* serious problems if a lock's page has gone missing
      or the slot bit isn't set.. */
   if(!search || !lock_bitmap_test(search->bitmap, slot))
   {
      unlock_gate(lock_lock, LOCK_WRITE);
      return e_not_found;
   }
   
   /* clear the slot and free the page if need be */
   lock_bitmap_clear(search->bitmap, slot);
   search->nr_free++;
   search->last_free = slot;
   
   if(search->nr_free == LOCK_POOL_BITMAP_LENGTH_BITS)
   {
      /* free the page */
      vmm_return_phys_pg(search->physical_base);
      
      /* delink from the list */
      if(search->next)
         search->next->previous = search->previous;
      
      if(search->previous)
         search->previous->next = search->next;
      
      vmm_free(search);
   }
   
   unlock_gate(lock_lock, LOCK_WRITE);
   return success;
}

/* locks_initialise
   Initialise the portable part of the locking system. This will
   require setting up locks for the VMM before any of its management
   functions can be called using the given physical page
   => initial_phys_pg = base address of first physical page to use
   <= 0 for success, or an error code
*/
kresult locks_initialise(void *initial_phys_pg)
{
   /* sanity check */
   if(!initial_phys_pg) return e_bad_params;
   
   /* zero out the initial pool structure - this VMM call is safe to use.
      this also forces gate_pool.prev to be NULL, indicating it's
      the first item and therefore shouldn't be free()d */
   vmm_memset(&gate_pool, 0, sizeof(rw_gate_pool));
   
   /* plug in the base address of this initial page */
   gate_pool.physical_base = initial_phys_pg;
   gate_pool.virtual_base = KERNEL_PHYS2LOG(initial_phys_pg);
   
   /* create the first lock by hand - the lock for the locking code */
   lock_lock = (rw_gate *)gate_pool.virtual_base;
   gate_pool.bitmap[0] = 1; /* too hardcoded? */
   gate_pool.last_free = 1;
   gate_pool.nr_free = LOCK_POOL_BITMAP_LENGTH_BITS - 1;
   vmm_memset(lock_lock, 0, sizeof(rw_gate));
   
   LOCK_DEBUG("[lock:%i] locks initialised: first gate pool at %p, first lock at %p\n",
              CPU_ID, &gate_pool, gate_pool.virtual_base);
   
   return success;
}

/* lock_gate
   Allow multiple threads to read during a critical section, but only
   allow one writer. To avoid deadlocks, it keeps track of the exclusive
   owner - so that a thread can enter a gate as a reader and later
   upgrade to a writer, or start off a writer and call a function that
   requires a read or write lock. This function will block and spin if
   write access is requested and other threads are reading/writing, or
   read access is requested and another thread is writing.
   => gate = lock structure to modify
      flags = set either LOCK_READ or LOCK_WRITE, also set LOCK_SELFDESTRUCT
              to mark a gate as defunct, causing other threads to fail if
              they try to access it
   <= success or a failure code
*/
kresult lock_gate(rw_gate *gate, unsigned int flags)
{
#ifndef UNIPROC
   kresult err = success;
   unsigned int caller;

#ifdef LOCK_TIME_CHECK
   unsigned long long ticks = x86_read_cyclecount();
#endif

   /* sanity checks */
   if(!gate) return e_failure;
   if(!cpu_table) return success; /* only one processor running */
   
#ifdef LOCK_DEBUG
   if(cpu_table)
   {
      LOCK_DEBUG("[lock:%i] -> lock_gate(%p, %x) by thread %p\n", CPU_ID, gate, flags, cpu_table[CPU_ID].current);
   }
   else
   {
      LOCK_DEBUG("[lock:%i] -> lock_gate(%p, %x) during boot\n", CPU_ID, gate, flags);
   }
#endif
   
   /* cpu_table[CPU_ID].current cannot be lower than the kernel virtual base 
      so it won't collide with the processor's CPU_ID, which is used to
      identify the owner if no thread is running */
   if(cpu_table[CPU_ID].current)
      caller = (unsigned int)cpu_table[CPU_ID].current;
   else
      caller = (CPU_ID) + 1; /* zero means no owner, CPU_IDs start at zero... */

   while(1)
   {
      lock_spin(&(gate->spinlock));
      
      /* we're in - is the gate claimed? */
      if(gate->owner)
      {
         /* it's in use - but is it another thread? */
         if(gate->owner != caller)
         {
            /* another thread has it :( perform checks */
            
            /* this lock is defunct? */
            if(gate->flags & LOCK_SELFDESTRUCT)
            {
               err = e_failure;
               goto exit_lock_gate;
            }
            
            /* if we're not trying to write and the owner isn't
               writing and a writer isn't waiting, then it's safe to progress */
            if(!(flags & LOCK_WRITE) && !(gate->flags & LOCK_WRITE) && !(gate->flags & LOCK_WRITEWAITING))
               goto exit_lock_gate;

            /* if we're trying to write then set a write-wait flag.
               when this flag is set, stop allowing new reading threads.
               this should prevent writer starvation */
            if(flags & LOCK_WRITE)
               gate->flags |= LOCK_WRITEWAITING;
         }
         else
         {
            /* if the gate's owned by this thread, then carry on */
            gate->refcount++; /* keep track of the number of times we're entering */
            goto exit_lock_gate;
         }
      }
      else
      {
         /* no one owns this gate, so make our mark */
         gate->owner = caller;
         gate->flags = flags;
         gate->refcount = 1; /* first in */
         goto exit_lock_gate;
      }
      
      unlock_spin(&(gate->spinlock));
      
      /* small window of opportunity for the other thread to
         release the gate :-/ */
      /* hint to newer processors that this is a spin-wait loop or
         NOP for older processors */
      __asm__ __volatile__("pause");
      
#ifdef LOCK_TIME_CHECK
      if((x86_read_cyclecount() - ticks) > LOCK_TIMEOUT)
      {
         /* prevent other cores from trashing the output debug while we dump this info */
         lock_spin(&lock_time_check_lock);
         
         KOOPS_DEBUG("[lock:%i] OMGWTF waited too long for gate %p to become available (flags %x)\n"
                     "         lock is owned by %p", CPU_ID, gate, flags, gate->owner);
         if(gate->owner > KERNEL_SPACE_BASE)
         {
            thread *t = (thread *)(gate->owner);
            KOOPS_DEBUG(" (thread %i process %i on cpu %i)", t->tid, t->proc->pid, t->cpu);
         }
         KOOPS_DEBUG("\n");
         debug_stacktrace();
         
         unlock_spin(&lock_time_check_lock);
         
         debug_panic("deadlock in kernel: we can't go on together with suspicious minds");
      }
#endif
   }

exit_lock_gate:
   /* release the gate so others can inspect it */
   unlock_spin(&(gate->spinlock));
   
   LOCK_DEBUG("[lock:%i] locked %p with %x\n", CPU_ID, gate, flags);
   
   return err;
#endif
}

/* unlock_gate
   Unlock a gate if it is ours to unlock. 
   => gate = lock structure to modify
      flags = set either LOCK_READ or LOCK_WRITE, also set LOCK_SELFDESTRUCT
              to mark a gate as defunct, causing other threads to fail if
              they try to access it
   <= success or a failure code
 */
kresult unlock_gate(rw_gate *gate, unsigned int flags)
{   
#ifndef UNIPROC
   unsigned int caller;
   
   /* sanity checks */
   if(!gate) return e_failure;
   if(!cpu_table) return success; /* only one processor running */   
   
#ifdef LOCK_DEBUG
   if(cpu_table)
   {
      LOCK_DEBUG("[lock:%i] unlock_gate(%p, %x) by thread %p\n", CPU_ID, gate, flags, cpu_table[CPU_ID].current);
   }
   else
   {
      LOCK_DEBUG("[lock:%i] unlock_gate(%p, %x)\n", CPU_ID, gate, flags);
   }
#endif
   
   if(cpu_table[CPU_ID].current)
      caller = (unsigned int)cpu_table[CPU_ID].current;
   else
      caller = (CPU_ID) + 1;
   
   lock_spin(&(gate->spinlock));
   
   /* if this is our gate then unset details */
   if(gate->owner == caller)
   {
      /* decrement thread entry count and release the gate if the
         owner has completely left, keeping the self-destruct lock
         in place if required */
      gate->refcount--;
      if(gate->refcount == 0)
      {
         gate->owner = 0;
         gate->flags = flags & LOCK_SELFDESTRUCT;
      }
   }

   /* release the gate so others can inspect it */
   unlock_spin(&(gate->spinlock));

   LOCK_DEBUG("[lock:%i] <- unlocked %p with %x on cpu %i\n", CPU_ID, gate, flags, 0);
   
   return success;
#endif
}
