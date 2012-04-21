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
rw_gate_pool pool_head;
 
/* lock_gate_alloc
   Allocate a readers-writer gate structure from a pool of SMP-safe pages.
   => ptr = pointer to word in which to store address of allocated gate
   <= 0 for success, or an error code
*/
kresult lock_rw_gate_alloc(rw_gate **ptr)
{
   kresult err;
   rw_gate_pool *search = &pool_head;

   /* sanity check */
   if(!ptr) return e_bad_params;
   
   /* lock the locking code */
   lock_gate(lock_lock, LOCK_READ);
   
   /* try to find an existing pool */
   while(search)
   {
   }
   
   /* if search is still NULL then grab a new page for the pool */
   if(!search)
   {
      void *new_page;
      err = vmm_req_phys_pg(&new_page, 1);
      if(err)
      {
         unlock_gate(lock_lock, LOCK_READ);
         return err; /* bail out if we're out of pages! */
      }
      
      /* allocate a new structure to describe this pool or give up */
      err = vmm_malloc((void **)&search, sizeof(rw_gate_pool));
      if(err)
      {
         unlock_gate(lock_lock, LOCK_READ);
         return err; /* bail out if we're out of memory! */
      }
      
      /* initialise the pool structure and add to the head of
         the linked list so it can be found quickly */
      vmm_memset(search, 0, sizeof(rw_gate_pool));
      
      /* mark the page as non-cacheable, non-buffering */
      pg_set_page_cache_type(new_page, 
      
      lock_gate(lock_lock, LOCK_WRITE);
      
   }
}

/* locks_initialise
   Initialise the portable part of the locking system. This will
   require setting up locks for the VMM before any of its management
   functions can be called using the given physical page, which is assumed to
   have been configured with the correct non-cached/unbuffered MMU flags
   => initial_phys_pg = base address of first physical page to use
   <= 0 for success, or an error code
*/
kresult locks_initialise(void *initial_phys_pg)
{
   /* sanity check */
   if(!initial_phys_pg) return e_bad_params;
   
   /* zero out the initial pool structure - this VMM call is safe to use */
   vmm_memset(&pool_head, 0, sizeof(rw_gate_pool));
   
   /* plug in the base address of this initial page */
   pool_head.physical_base = initial_phys_pg;
   pool_head.virtual_base = KERNEL_PHYS2LOG(initial_phys_pg);
   
   /* create the first lock by hand - the lock for the locking code */
   lock_lock = (rw_gate *)pool_head.virtual_base;
   pool_head.bitmap[0] = 1; /* too hardcoded? */
   pool_head.last_free = 1;
   vmm_memset(lock_lock, 0, sizeof(rw_gate));
   
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

   /* sanity checks */
   if(!gate) return e_failure;
   if(!cpu_table) return success; /* only one processor running */   
   
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
