/* kernel/core/thread.c
 * diosix portable thread management code
 * Author : Chris Williams
 * Date   : Sun,25 Oct 2009.01:35:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* thread_find_thread
   <= return a pointer to a thread that matches the given tid owned by the
      given process, or NULL for failure */
thread *thread_find_thread(process *proc, unsigned int tid)
{
   thread *search, **table;
   unsigned int hash = tid % THREAD_HASH_BUCKETS;

   if(!tid || !proc)
   {
      KOOPS_DEBUG("[thread:%i] OMGWTF thread_find_thread failed on sanity check.\n"
                  "            process %p tid %i\n", CPU_ID, proc, tid);
      debug_stacktrace();
      return NULL;
   }

   if(lock_gate(&(proc->lock), LOCK_READ))
      return NULL;
   
   table = proc->threads;
   
   search = table[hash];
   while(search)
   {
      if(search->tid == tid)
      {
         unlock_gate(&(proc->lock), LOCK_READ);
         return search; /* foundya */
      }
      
      search = search->hash_next;
   }
   
   unlock_gate(&(proc->lock), LOCK_READ);
   return NULL; /* not found! */
}

/* thread_duplicate
   Make an exact copy of a thread in another process - for use when a
   process calls fork(). Assume the memory mappings will be taken care of elsewhere..
   => proc = process to create new thread for
      source = original thread to clone
   <= returns pointer to new thread or NULL for failure
*/
thread *thread_duplicate(process *proc, thread *source)
{
   unsigned int kstack, hash;
   thread *new, **threads;
   
   if(lock_gate(&(proc->lock), LOCK_WRITE))
      return NULL;
   if(lock_gate(&(source->lock), LOCK_READ))
   {
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return NULL;
   }
   
   /* grab memory and zero it now to store details of new thread */
   kresult err = vmm_malloc((void **)&new, sizeof(thread));
   if(err)
   {
      unlock_gate(&(source->lock), LOCK_READ);
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return NULL; /* fail if we can't alloc a new thread */
   }
   vmm_memset(new, 0, sizeof(thread));
   
   /* initialise thread hash table if required */
   if(!(proc->threads))
   {
      thread_new_hash(proc);
      if(!(proc->threads))
      {
         vmm_free(new);
         unlock_gate(&(source->lock), LOCK_READ);
         unlock_gate(&(proc->lock), LOCK_WRITE);
         return NULL;
      }
   }
   threads = proc->threads;
   
   /* fill in the blanks */
   new->proc             = proc;
   new->tid              = source->tid;
   new->flags            = source->flags;
   
   new->timeslice        = source->timeslice;
   new->priority         = source->priority;
   new->priority_granted = SCHED_PRIORITY_INVALID; /* new threads don't inherit granted pri */ 
   sched_priority_calc(new, priority_reset);
   
   new->stackbase        = source->stackbase;

   /* the new thread is asleep and due to be scheduled */
   new->state = sleeping;
   
   /* clone the source thread's kernel stack for this new thread */
   err = vmm_malloc((void **)&kstack, MEM_PGSIZE);
   if(err)
   {
      unlock_gate(&(source->lock), LOCK_READ);
      unlock_gate(&(proc->lock), LOCK_WRITE);
      vmm_free(new);
      return NULL; /* something went wrong */
   }
   
   /* stacks grow down, hence pushing base up */
   new->kstackblk = kstack;
   new->kstackbase = kstack + MEM_PGSIZE;
   
   /* copy thread state FIXME not very portable :( */
   vmm_memcpy(&(new->regs), &(source->regs), sizeof(int_registers_block));
   
   hash = new->tid % THREAD_HASH_BUCKETS;
   if(threads[hash])
   { 
      threads[hash]->hash_prev = new;
      new->hash_next = threads[hash];
   }
   else
   {
      new->hash_next = NULL;
   }
   threads[hash] = new;
   new->hash_prev = NULL;   
   
   unlock_gate(&(source->lock), LOCK_READ);
   unlock_gate(&(proc->lock), LOCK_WRITE);
   
   THREAD_DEBUG("[thread:%i] cloned thread %i of process %i for process %i (%p) (kstack %p)\n",
           CPU_ID, source->tid, source->proc->pid, proc->pid, new, new->kstackbase);
   
   return new;
}

/* thread_new_hash
   Create a new hash table for threads in the given process
   <= returns 0 for success, anything else is failure
*/
kresult thread_new_hash(process *proc)
{
   kresult err;
   thread **threads;
   
   if(lock_gate(&(proc->lock), LOCK_WRITE))
      return e_failure;
   
   if(proc->threads)
   {
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return e_failure;
   }
   
   err = vmm_malloc((void **)&threads, sizeof(thread *) * THREAD_HASH_BUCKETS);
   if(err)
   {
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return err;
   }
   
   vmm_memset(threads, 0, sizeof(thread *) * THREAD_HASH_BUCKETS);
   proc->threads = threads;
   
   unlock_gate(&(proc->lock), LOCK_WRITE);
   
   THREAD_DEBUG("[thread:%i] created thread hash table %p for process %i\n",
           CPU_ID, threads, proc->pid);

   return success;
}

/* thread_new
   Create a new thread inside a process
   => proc = pointer to process owning the thread
 <= pointer to new thread structure, or NULL for failure
*/
thread *thread_new(process *proc)
{
   kresult err;
   unsigned int tid_free = 0, hash, stackbase;
   thread *new;
   unsigned int kstack;
   
   if(!proc) return NULL; /* give up now if we get a bad pointer */
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   /* give up now if we have too many threads */
   if(proc->thread_count > THREAD_MAX_NR)
   {
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return NULL;
   }

   /* grab memory now to store details of new thread */
   err = vmm_malloc((void **)&new, sizeof(thread));
   if(err)
   {
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return NULL; /* fail if we can't alloc a new thread */
   }
   vmm_memset(new, 0, sizeof(thread));
   
   /* kernel stack initialisation - just 4K per thread for now */
   err = vmm_malloc((void **)&kstack, MEM_PGSIZE);
   if(err)
   {
      vmm_free(new);
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return NULL; /* FIXME should really do some clean up if this fails */
   }
   vmm_memset((void *)kstack, 0, MEM_PGSIZE);
   
   /* initialise thread hash table if required */
   if(!(proc->threads))
   {
      thread_new_hash(proc);
      if(!(proc->threads))
      {
         vmm_free((void *)kstack);
         vmm_free(new);
         unlock_gate(&(proc->lock), LOCK_WRITE);
         return NULL;
      }
   }

   /* search for an available thread id */
   while(!tid_free)
   {
      if(thread_find_thread(proc, proc->next_tid) == NULL)
         tid_free = 1;
      else
      {
         proc->next_tid++;
         if(proc->next_tid >= THREAD_MAX_NR)
            proc->next_tid = FIRST_TID;
      }
   }

   /* assign our new TID */
   new->tid = proc->next_tid;
   proc->next_tid++;
   if(proc->next_tid >= THREAD_MAX_NR)
      proc->next_tid = FIRST_TID;

   /* add it to the tid hash table of threads for this process */
   hash = new->tid % THREAD_HASH_BUCKETS;
   if(proc->threads[hash])
   {
      proc->threads[hash]->hash_prev = new;
      new->hash_next = proc->threads[hash];
   }
   else
   {
      new->hash_next = NULL;
   }
   proc->threads[hash] = new;
   new->hash_prev = NULL;

   /* fill in more details */
   new->proc = proc;
   proc->thread_count++;
   
   /* calculate the base priority points score */
   new->priority = proc->priority_low;
   new->priority_granted = SCHED_PRIORITY_INVALID;
   sched_priority_calc(new, priority_reset);

   /* create a vma for the thread's user stack - don't forget stacks grow down */
   stackbase = KERNEL_SPACE_BASE - (THREAD_MAX_STACK * MEM_PGSIZE * new->tid);
   err = vmm_add_vma(proc, stackbase - (THREAD_MAX_STACK * MEM_PGSIZE), (THREAD_MAX_STACK * MEM_PGSIZE),
                     VMA_READABLE | VMA_WRITEABLE | VMA_MEMSOURCE, 0);
   /* bail out and clean up if linking the stack failed */
   if(err)
   {
      proc->threads[hash] = new->hash_next;
      if(new->hash_next) new->hash_prev = NULL;
      vmm_free((void *)kstack);
      vmm_free(new);
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return NULL;
   }
   
   new->stackbase = stackbase;

   /* stacks grow down... */
   new->kstackblk = kstack;
   new->kstackbase = (kstack + MEM_PGSIZE);

   unlock_gate(&(proc->lock), LOCK_WRITE);

   THREAD_DEBUG("[thread:%i] created thread %p tid %i (ustack %p kstack %p) for process %i\n",
           CPU_ID, new, new->tid, new->stackbase, new->kstackbase, proc->pid);

   return new;
}

/* thread_kill
   Destroy a thread, freeing its kernel-held resources
   => owner = process holding the thread
      victim = thread to kill or NULL for all threads - ONLY call on process shutdown
   <= success or e_failure if something went wrong
*/
kresult thread_kill(process *owner, thread *victim)
{
   unsigned int physaddr;
   
   if(!owner) return e_failure;
      
   if(victim)
   {
      unsigned int stackbase;
      vmm_tree *stack_node;
      
      /* bail out if we're trying to kill a thread outside of the
         process */
      if(victim->proc != owner) return e_failure;
      
      /* if we can't lock then assume it's this thread that's dying */
      if(sched_lock_thread(victim)) sched_remove(victim, dead);
      
      /* synchronise with the rest of the system - we cannot tear down
         a thread if it's still running on another core so spin until 
         it's clear that the thread is no longer being run on the cpu
         it was last on */
      lock_gate(&(victim->lock), LOCK_READ);
      while(cpu_table[victim->cpu].current != victim);
      unlock_gate(&(victim->lock), LOCK_READ);

      /* stop this thread from running and lock it */
      if(lock_gate(&(victim->lock), LOCK_WRITE | LOCK_SELFDESTRUCT))
         return e_failure;
      
      /* destroy the thread's user stack vma */
      stackbase = KERNEL_SPACE_BASE - (THREAD_MAX_STACK * MEM_PGSIZE * victim->tid);
      stack_node = vmm_find_vma(victim->proc, stackbase - (THREAD_MAX_STACK * MEM_PGSIZE),
                                THREAD_MAX_STACK * MEM_PGSIZE);
      if(stack_node) vmm_unlink_vma(victim->proc, stack_node);

      /* unlink thread from the process's thread hash table */
      lock_gate(&(owner->lock), LOCK_WRITE);
                 
      if(victim->hash_next)
         victim->hash_next->hash_prev = victim->hash_prev;
      if(victim->hash_prev)
         victim->hash_prev->hash_next = victim->hash_next;
      else
      /* we were the hash table entry head, so fixup table */
         owner->threads[victim->tid % THREAD_HASH_BUCKETS] = victim->hash_next;

      owner->thread_count--;
      unlock_gate(&(owner->lock), LOCK_WRITE);
      
      /* free up resources */
      vmm_free((void *)(victim->kstackblk));
      vmm_free(victim);
      
      THREAD_DEBUG("[thread:%i] killed thread %i (%p) of process %i (%p)\n",
              CPU_ID, victim->tid, victim, owner->pid, owner);
   }
   else
   {
      /* destroy all threads one-by-one */
      unsigned int loop;
      
      lock_gate(&(owner->lock), LOCK_WRITE);
      
      for(loop = 0; loop < THREAD_HASH_BUCKETS; loop++)
      {
         victim = owner->threads[loop];
         
         while(victim)
         {
            thread_kill(owner, victim);
            victim = victim->hash_next;
         }
      }
      
      /* release the thread hash table */
      vmm_free(owner->threads);
      
      unlock_gate(&(owner->lock), LOCK_WRITE);
   }
   
   return success;
}
