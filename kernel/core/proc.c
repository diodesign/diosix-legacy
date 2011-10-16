/* kernel/core/proc.c
 * diosix portable process management code
 * Author : Chris Williams
 * Date   : Fri,06 Apr 2007.20:39:10

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* hash table of pointers to process entries */
process **proc_table = NULL;

/* table of processes with defined roles within the operating systems */
process *proc_roles[DIOSIX_ROLES_NR];
role_snoozer *role_wait_list[DIOSIX_ROLES_NR];

unsigned int next_pid = FIRST_PID;
unsigned int proc_count = 0;

/* provide locking around critical sections */
rw_gate proc_lock;


/* --------------------------------- roles ------------------------------- */
/* proc_role_remove
   Remove a role from a process
   => target = process to alter
      role = role to remove from the process
   <= 0 for success, or an error code
*/
kresult proc_role_remove(process *target, unsigned int role)
{
   /* sanity check */
   if(!target || !target->role) return e_bad_params;
   if(target->role != role) return e_not_found;
   
   /* assumes a lock is held on the process */
   lock_gate(&(proc_lock), LOCK_WRITE);
   
   proc_roles[target->role - 1] = NULL;
   target->role = DIOSIX_ROLE_NONE;
   
   PROC_DEBUG("[proc:%i] removed role %i from process %p (pid %i)\n",
              CPU_ID, role, target, target->pid);
   
   unlock_gate(&(proc_lock), LOCK_WRITE);
   return success;
}

/* proc_role_add
   Add a role to a process. Only processes with PROC_FLAG_CANPLAYAROLE
   set can register a role with the kernel.
   => target = process to alter
      role = role to add to the process
   <= 0 for success, or an error code
*/
kresult proc_role_add(process *target, unsigned int role)
{
   /* sanity check */
   if(!target || target->role) return e_bad_params;
   if(!(target->flags & PROC_FLAG_CANPLAYAROLE)) return e_no_rights;
   if(!role || role > DIOSIX_ROLES_NR) return e_bad_params;
   
   /* assumes a lock is held on the process */
   lock_gate(&(proc_lock), LOCK_WRITE);
   
   proc_roles[role - 1] = target;
   target->role = role;

   PROC_DEBUG("[proc:%i] given process %p (pid %i) role %i\n",
              CPU_ID, target, target->pid, role);
   
   unlock_gate(&(proc_lock), LOCK_WRITE);
   return success;
}

/* proc_role_lookup
   Get the process with the given role
   => role = role to search for
   <= pointer to process structure or NULL if none registered
*/
process *proc_role_lookup(unsigned int role)
{
   process *proc;
   
   if(!role || role > DIOSIX_ROLES_NR) return NULL;
   
   lock_gate(&(proc_lock), LOCK_READ);
   proc = proc_roles[role - 1];
   unlock_gate(&(proc_lock), LOCK_READ);   
   
   PROC_DEBUG("[proc:%i] looked up process %p for role %i\n",
              CPU_ID, proc, role);
   
   return proc;
}

/* proc_wait_for_role
   Put a thread to sleep until a role is registered by a process
   => snoozer = thread to send to sleep
      role = role to sleep against
   <= 0 for success, or an error code
*/
kresult proc_wait_for_role(thread *snoozer, unsigned char role)
{
   role_snoozer *new = NULL, *head;
   
   /* sanity check */
   if(!snoozer || !role || role > DIOSIX_ROLES_NR) return e_bad_params;

   /* allocate a head for the linked list */
   if(vmm_malloc((void **)&new, sizeof(role_snoozer)) != success)
      return e_failure;
   
   new->snoozer = snoozer;
   new->previous = NULL;
   
   lock_gate(&(proc_lock), LOCK_WRITE);
   
   /* attach it to the start of the linked list */
   head = role_wait_list[role - 1];
   if(head) head->previous = new;
   new->next = head;
   role_wait_list[role - 1] = new;
   
   unlock_gate(&(proc_lock), LOCK_WRITE);
   
   /* tell the scheduler to send the thread to sleep */
   sched_remove(snoozer, sleeping);
   
   PROC_DEBUG("[proc:%i] put thread %p tid %i pid %i into sleep-wait on role %i [entry %p thread %p]\n", CPU_ID,
              snoozer, snoozer->tid, snoozer->proc->pid, role, new, new->snoozer);
   return success;
}

/* proc_role_wakeup
   Wake up any threads that were sleep-waiting for a named process
   to block waiting for a message
   => role = role id to wake up
*/
void proc_role_wakeup(unsigned char role)
{
   /* sanity check */
   if(role > DIOSIX_ROLES_NR || !role) return;
   
   lock_gate(&(proc_lock), LOCK_WRITE);
   role_snoozer *entry = role_wait_list[role - 1];
   
   while(entry)
   {
      /* wake up the sleeping thread */
      role_snoozer *next = entry->next;
      PROC_DEBUG("[proc:%i] waking up thread %p (tid %i pid %i) [entry %p] on role %i\n", CPU_ID,
                 entry->snoozer, entry->snoozer->tid, entry->snoozer->proc->pid, entry, role);
      sched_add(entry->snoozer->cpu, entry->snoozer);
      
      /* unlink it from the chain and free the memory describing it */
      if(entry->previous) entry->previous->next = next;
      if(next) next->previous = entry->previous;
      if(role_wait_list[role - 1] == entry) role_wait_list[role - 1] = next;
      vmm_free(entry);
      entry = next;
   }
   
   unlock_gate(&(proc_lock), LOCK_WRITE);
}

/* --------------------------------- rights ------------------------------- */
/* proc_layer_up
   Increase a process's privilege layer so that it becomes less privileged
   => proc = pointer to process structure to update
   <= 0 for success or an error code
*/
kresult proc_layer_up(process *proc)
{
   kresult result = success;
   
   /* check pointer for sanity */
   if(!proc) return e_failure;
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   /* enforce maximum processlayer */
   if(proc->layer >= LAYER_MAX)
   {
      result = e_max_layer;
      goto proc_layer_up_return;
   }
   
   /* ok, let's do this */
   proc->layer++;
      
proc_layer_up_return:
   unlock_gate(&(proc->lock), LOCK_WRITE);
   return result;
}

/* proc_clear_rights
   Clear rights bits from a process. A bit set in the mask byte will clear the
   corresponding bit in the process's flag byte, thus removing that right from
   the process.
   => proc = pointer process's structure
      bits = word to apply to the process's flag byte
   <= 0 for success or an error code
*/
kresult proc_clear_rights(process *proc, unsigned int bits)
{   
   /* check pointer for sanity */
   if(!proc) return e_failure;
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   /* ok, let's do this - first, mask off non-rights bits */
   bits &= PROC_RIGHTS_MASK;
   
   /* now clear bits in the flag byte where there are set bits in the given word */
   proc->flags &= (~bits);
   
   unlock_gate(&(proc->lock), LOCK_WRITE);
   return success;
}

/* --------------------------------- process mngmnt ------------------------------- */
/* proc_find_proc
   <= return a pointer to a process that matches the given pid, or NULL for failure */
process *proc_find_proc(unsigned int pid)
{
   process *search;
   unsigned int hash = pid % PROC_HASH_BUCKETS;

   lock_gate(&proc_lock, LOCK_READ);
   
   search = proc_table[hash];
   while(search)
   {
      if(search->pid == pid)
      {
         unlock_gate(&proc_lock, LOCK_READ);
         return search; /* foundya */
      }
         
      search = search->hash_next;
   }

   unlock_gate(&proc_lock, LOCK_READ);
   return NULL; /* not found! */
}

/* proc_send_group_signal
   Send a signal to all processes in a given process group via msg_send_signal().
   => pgid = process group id to target, or 0 to use the sending process's pgid
      sender = pointer to thread structure of sender, or NULL for kernel
      signum = signal code to send
      sigcode = additional reason code
   <= 0 for success, or an error reason code
*/
kresult proc_send_group_signal(unsigned int pgid, thread *sender, unsigned int signum, unsigned int sigcode)
{
   process *search;
   unsigned int hashloop;
   kresult err;
   
   if(!pgid) pgid = sender->proc->proc_group_id;
   
   lock_gate(&proc_lock, LOCK_READ);
   
   /* go through all the processes looking for a pgid match */
   for(hashloop = 0; hashloop < PROC_HASH_BUCKETS; hashloop++)
   {
      search = proc_table[hashloop];
      while(search)
      {
         if(search->proc_group_id == pgid)
         {
            err = msg_send_signal(search, sender, signum, sigcode);
            
            /* give up if a failure occurs */
            if(err)
            {
               unlock_gate(&proc_lock, LOCK_READ);
               return err;
            }
         }
         search = search->hash_next;
      }
   }
   
   unlock_gate(&proc_lock, LOCK_READ);
   return err; /* should be success */
}

/* proc_is_valid_pgid
   Confirm there is at least one other process with a matching
   process group id and session id
   => pgid = process group id to search for
      sid = required matching session id, or 0 for match any
      exclude = if set, exclude this process from the search
   <= 0 for successful find, or an error code
*/
kresult proc_is_valid_pgid(unsigned int pgid, unsigned int sid, process *exclude)
{
   process *search;
   unsigned int hashloop;
   
   lock_gate(&proc_lock, LOCK_READ);
   
   /* go through all the processes */
   for(hashloop = 0; hashloop < PROC_HASH_BUCKETS; hashloop++)
   {
      search = proc_table[hashloop];
      while(search)
      {
         if((search->proc_group_id == pgid) &&
            ((search->session_id == sid) || !sid))
         {
            if(search != exclude)
            {
               unlock_gate(&proc_lock, LOCK_READ);
               return success; /* I got you */
            }
         }
         
         search = search->hash_next;
      }
   }
   
   unlock_gate(&proc_lock, LOCK_READ);
   return e_not_found; /* not found! */
}

/* proc_is_child
   Confirm a process is a descendant of another process
   => parent = pointer to parent process
      child = pointer to process to check is a child of parent
   <= success if child is a descdenant of parent
      e_not_found if the child couldn't be found
      e_failure if parameters are invalid
*/
kresult proc_is_child(process *parent, process *child)
{
   unsigned int loop;
   
   /* sanity check.. */
   if(!parent || !child) return e_failure;
   
   if(lock_gate(&(parent->lock), LOCK_READ))
      return e_failure;
   
   /* check the child on the parent's child list */
   for(loop = 0; loop < parent->child_list_size; loop++)
      if(parent->children[loop] == child)
      {
         unlock_gate(&(parent->lock), LOCK_READ);
         return success;
      }

   unlock_gate(&(parent->lock), LOCK_READ);
   return e_not_found;
}

/* proc_attach_child
   Add a child process to a parent's table
   => parent and child = pointers to relevant process structures
   <= success or e_failure if something went wrong
*/
kresult proc_attach_child(process *parent, process *child)
{
   unsigned int child_loop;
   
   if(lock_gate(&(parent->lock), LOCK_WRITE))
      return e_failure;
   if(lock_gate(&(child->lock), LOCK_READ))
   {
      unlock_gate(&(parent->lock), LOCK_WRITE);
      return e_failure;
   }
   
   /* add to the list of children - is the list big enough? */
   if(parent->child_count >= parent->child_list_size)
   {
      process **new_list;
      /* we need to grow the list size */
      unsigned int new_size = parent->child_list_size * 2;
      
      if(!new_size) new_size = 64; /* initial size */
      if(vmm_malloc((void **)&new_list, new_size * sizeof(process *)))
      {
         unlock_gate(&(child->lock), LOCK_READ);
         unlock_gate(&(parent->lock), LOCK_WRITE);
         return e_failure; /* bail out of malloc fails! */
      }
      
      vmm_memset(new_list, 0, new_size); /* clean the new list */
      
      if(parent->children)
      {
         /* copy over the previous list */
         vmm_memcpy(new_list, parent->children, 
                    parent->child_list_size * sizeof(process *));
         vmm_free(parent->children); /* free the old list */
      }
      
      /* update the list's accounting */
      parent->children = new_list;
      parent->child_list_size = new_size;
   }
   
   /* find an empty slot and insert the new child's pointer */
   for(child_loop = 0; child_loop < parent->child_list_size; child_loop++)
      if(parent->children[child_loop] == NULL)
      {
         /* found a free slot, write in the new child's pointer */
         parent->children[child_loop] = child;
         parent->child_count++;
         child->parentpid = parent->pid;
         break;
      }
   
   unlock_gate(&(child->lock), LOCK_READ);
   unlock_gate(&(parent->lock), LOCK_WRITE);
   return success;
}

/* proc_remove_child
   Take a child off a process's list */
kresult proc_remove_child(process *parent, process *child)
{
   unsigned int child_loop;
   
   if(lock_gate(&(parent->lock), LOCK_WRITE))
      return e_failure;
   if(lock_gate(&(child->lock), LOCK_READ))
   {
      unlock_gate(&(parent->lock), LOCK_WRITE);
      return e_failure;
   }
   
   for(child_loop = 0; child_loop < parent->child_list_size; child_loop++)
      if(parent->children[child_loop] == child)
      {
         /* found a free slot, write in the new child's pointer */
         parent->children[child_loop] = NULL;
         parent->child_count--;
         child->parentpid = 0;
         break;
      }
   
   unlock_gate(&(child->lock), LOCK_READ);
   unlock_gate(&(parent->lock), LOCK_WRITE);
   return success;
}

/* proc_new
   Clone a process - or create an entirely blank process
   => current = pointer to current parent process to base the
                new child on, or NULL for none.
      caller = thread that invoked this function or NULL if
               the kernel called to generate a new process.
               if NULL then start with a blank memory map
               (save for the kernel's mappings).
   <= pointer to new process structure, or NULL for failure
*/
process *proc_new(process *current, thread *caller)
{
   unsigned char pid_free = 0;
   process *new = NULL;
   unsigned int hash;
   thread *newthread = NULL;
   kresult err;
   
   /* grab memory to hold the process structure and zero-fill it */
   err = vmm_malloc((void **)&new, sizeof(process));
   if(err) return NULL; /* fail if we can't even alloc a process */

   vmm_memset(new, 0, sizeof(process));
   
   lock_gate(&proc_lock, LOCK_WRITE);
   
   /* give up now if we have too many processes */
   if(proc_count >= PROC_MAX_NR)
   {
      unlock_gate(&proc_lock, LOCK_WRITE);
      vmm_free(new);
      return NULL;
   }

   /* search for an available PID */
   while(!pid_free)
   {
      if(proc_find_proc(next_pid) == NULL)
         pid_free = 1;
      else
      {
         next_pid++;
         if(next_pid >= PROC_MAX_NR)
            next_pid = FIRST_PID;
      }
   }
      
   /* assign our new PID */
   new->pid = next_pid;
   next_pid++;
   if(next_pid >= PROC_MAX_NR)
      next_pid = FIRST_PID;
   
   unlock_gate(&proc_lock, LOCK_WRITE);

   /* call the port-specific process creation code and duplicate the vmas of the parent
      for this new process. If caller = NULL then the kernel is constructing the system's
      first processes and will handle their virtual memory mappings, hence why we override
      current with NULL if caller is also NULL */
   if(caller) pg_new_process(new, current); else pg_new_process(new, NULL);
   if(caller) vmm_duplicate_vmas(new, current); /* and clone the vmas */
   
   /* inherit status/details from the parent process */
   if(current)
   {
      thread *dupthread;
   
      if(lock_gate(&(current->lock), LOCK_WRITE))
      {
         vmm_free(new);
         return NULL;         
      }
      
      new->parentpid     = current->pid;
      new->flags         = current->flags;
      new->cpu           = current->cpu;
      new->layer         = current->layer;
      new->priority_low  = current->priority_low;
      new->priority_high = current->priority_high;
      
      /* if the kernel's calling then keep tid at 1 */
      if(caller) new->next_tid = current->next_tid;
      else new->next_tid = FIRST_TID;
      
      /* preserve POSIX-conformant user and group ids */
      new->proc_group_id = current->proc_group_id;
      new->session_id    = current->session_id;
      vmm_memcpy(&(new->uid), &(current->uid), sizeof(posix_id_set));
      vmm_memcpy(&(new->gid), &(current->gid), sizeof(posix_id_set));
            
      if(proc_attach_child(current, new))
      {
         unlock_gate(&(current->lock), LOCK_WRITE);
         vmm_free(new);
         return NULL;
      }

      /* create a hash table of threads in the process */
      if(thread_new_hash(new))
      {
         proc_remove_child(current, new);
         vmm_free(new); /* tidy up */
         unlock_gate(&(current->lock), LOCK_WRITE);
         return NULL;
      }
      
      /* duplicate the running thread */
      /* if caller is NULL then it's the kernel calling during
         boot and userspace hasn't been started yet */
      if(caller) dupthread = thread_duplicate(new, caller);
      else dupthread = thread_new(new);

      if(!dupthread)
      {
         proc_remove_child(current, new);
         vmm_free(new); /* tidy up */
         unlock_gate(&(current->lock), LOCK_WRITE);
         return NULL;
      }
      
      new->thread_count = 1;
      unlock_gate(&(current->lock), LOCK_WRITE);
   }
   else
   {
      /* assume this is the system's first first process, so
         set some defaults */
      new->priority_low = SCHED_PRIORITY_MIN;
      new->priority_high = SCHED_PRIORITY_MAX;
      
      /* initialise the process's hash table of threads and 
       create a new thread for execution */
      new->next_tid = FIRST_TID;
      newthread = thread_new(new);
   }
   
   /* add the new process to the pid hash table */
   lock_gate(&proc_lock, LOCK_WRITE);
   
   hash = new->pid % PROC_HASH_BUCKETS;
   if(proc_table[hash])
   {
      proc_table[hash]->hash_prev = new;
      new->hash_next = proc_table[hash];
   }
   else
   {
      new->hash_next = NULL;
   }
   proc_table[hash] = new;
   new->hash_prev = NULL;
   proc_count++;
   unlock_gate(&proc_lock, LOCK_WRITE);
   
   PROC_DEBUG("[proc:%i] created new process %i (%p)\n", CPU_ID, new->pid, new);

   return new;
}

/* proc_kill
   Request to kill the given process
   => victimpid = PID of process to destroy (USER-SUPPLIED)
      slayer    = pointer to process making the request
   <= 0 for success or an error code */
kresult proc_kill(unsigned int victimpid, process *slayer)
{
   process *victim, *parent;
   unsigned int loop;
   
   /* sanity checks */
   if((victimpid > PROC_MAX_NR) || !slayer) return e_failure;
   victim = proc_find_proc(victimpid);
   if(!victim) return e_failure;
   
   lock_gate(&(slayer->lock), LOCK_READ);
   
   /* rights checks */
   if(victim->layer > slayer->layer) goto do_proc_kill;
   if(proc_is_child(slayer, victim) != success) goto do_proc_kill;
   
   /* failed to possess the correct rights to kill this victim */
   unlock_gate(&(slayer->lock), LOCK_READ);
   return e_no_rights;
   
do_proc_kill:
   unlock_gate(&(slayer->lock), LOCK_READ);
   
   /* we have a green light - but we're not trying to kill the
      system executive, right? */
   if(victim == proc_role_lookup(DIOSIX_ROLE_SYSTEM_EXECUTIVE))
      debug_panic("system executive just died D:");
   
   /* reverse the process creation routines in proc_new() */
   parent = proc_find_proc(victim->parentpid);
   
   /* stop it from running but remember, there's no point
      unblocking the process we're about to slay */
   if(lock_gate(&(victim->lock), LOCK_WRITE | LOCK_SELFDESTRUCT))
      return e_failure;
   sched_lock_proc(victim);
   
   /* victim process is now effectively dead to the system */
   
   /* unlink it from the process table */
   lock_gate(&proc_lock, LOCK_WRITE);
   if(victim->hash_next)
      victim->hash_next->hash_prev = victim->hash_prev;
   if(victim->hash_prev)
      victim->hash_prev->hash_next = victim->hash_next;
   else
      /* we were the hash table entry head, so fixup table */
      proc_table[victim->pid % PROC_HASH_BUCKETS] = victim->hash_next;
   unlock_gate(&proc_lock, LOCK_WRITE);
   
   /* destroy the threads */
   thread_kill(victim, NULL);
   
   /* won't someone think of the children? */
   if(victim->children)
   {
      /* attach the to-be-orphaned children to the system executive process */
      for(loop = 0; loop < victim->child_list_size; loop++)
      {
         if(victim->children[loop])
         {
            victim->children[loop]->prevparentpid = victim->children[loop]->parentpid;
            proc_attach_child(proc_role_lookup(DIOSIX_ROLE_SYSTEM_EXECUTIVE), victim->children[loop]);
         }
      }
      
      /* and free the table */
      vmm_free(victim->children);
   }
   
   /* remove the message pools */
   if(victim->system_signals) vmm_destroy_pool(victim->system_signals);
   if(victim->user_signals) vmm_destroy_pool(victim->user_signals);
   if(victim->msg_queue)
   {
      /* wake up each waiting process to tell them it's game over */
      queued_sender *sender = NULL;
      for(;;)
      {
         sender = vmm_next_in_pool(sender, victim->msg_queue);
         if(sender)
         {
            process *sender_proc = proc_find_proc(sender->pid);
            if(sender_proc)
            {
               thread *sender_thread = thread_find_thread(sender_proc, sender->tid);
               if(sender_thread)
               {
                  syscall_post_msg_send(sender_thread, e_no_receiver);
                  sched_add(sender_thread->cpu, sender_thread);
               }
            }
         }
         else break; /* no more senders queued */
      }
      
      vmm_destroy_pool(victim->msg_queue);
   }
   
   /* teardown the process's virtual memory structures */
   vmm_destroy_vmas(victim);
   pg_destroy_process(victim);
   
   /* teardown the process's physical memory structures */
   proc_remove_phys_mem_allocation(victim, NULL);
   
   /* remove any role entry the process held */
   if(victim->role) proc_role_remove(victim, victim->role);
   
   /* strip away any registered irq handlers */
   while(victim->interrupts)
   {
      irq_driver_entry *entry = victim->interrupts;
      irq_deregister_driver(entry->irq_num, entry->flags & IRQ_DRIVER_TYPEMASK,
                            entry->proc, entry->func);
   }
   
   /* give up the space held by the process structure */
   vmm_free(victim);
   
   /* don't forget to dispatch a signal to the parent and
      don't fret if the parent shuns its moment of mourning */
   msg_send_signal(parent, NULL, SIGCHLD, 0);
   
   PROC_DEBUG("[proc:%i] killed process %i (%p)\n", CPU_ID, victim->pid, victim);
   
   return success;
}

/* proc_add_phys_mem_allocation
   Register a block of contiguous physical memory with the given process
   => proc = process to add the phys mem allocation to
      addr = base address of the block
      pages = number of pages in the block
   <= 0 for success, or an error code
*/
kresult proc_add_phys_mem_allocation(process *proc, void *addr, unsigned short pages)
{
   kresult err;
   process_phys_mem_block *block;
   
   /* sanity check */
   if(!proc || !addr || !pages) return e_bad_params;
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   err = vmm_malloc((void **)&block, sizeof(process_phys_mem_block));
   if(err)
   {
      unlock_gate(&(proc->lock), LOCK_WRITE);
      return err;
   }
   
   /* fill in the block and add it to the process's linked list of
      physical memory allocations */
   block->base = addr;
   block->pages = pages;
   
   if(proc->phys_mem_head)
   {
      proc->phys_mem_head->previous = block;
      block->next = proc->phys_mem_head;
   }
   else
   {
      block->next = NULL;
   }
   proc->phys_mem_head = block;
   block->previous = NULL;
   
   unlock_gate(&(proc->lock), LOCK_WRITE);
   
   return success;
}

/* proc_remove_phys_mem_allocation
   Remove a previously allocated block of physical memory from a given process
   and also return the block's pages to the system
   => proc = process to release the memory from
      addr = base address of the block, or NULL to remove all the process's
             physical memory allocations
   <= 0 for success, or an error code
*/
kresult proc_remove_phys_mem_allocation(process *proc, void *addr)
{
   process_phys_mem_block *block;
   
   /* sanity check */
   if(!proc) return e_bad_params;
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   block = proc->phys_mem_head;
   
   /* are we tearing down everything in one go? */
   if(!addr)
   {
      while(block)
      {
         process_phys_mem_block *victim = block;
         
         vmm_return_phys_pages(block->base, block->pages);
         block = block->next;
         vmm_free(victim);
      }

      unlock_gate(&(proc->lock), LOCK_WRITE);
      return success;
   }
   
   while(block)
   {
      if(block->base == addr)
      {
         vmm_return_phys_pages(block->base, block->pages);
         
         /* unlink it from the process's chain if we're not
            tearing down everything */
         if(block->next)
            block->next->previous = block->previous;
         if(block->previous)
            block->previous->next = block->next;
         else
            proc->phys_mem_head = block->next;

         vmm_free(block);
         unlock_gate(&(proc->lock), LOCK_WRITE);
         return success;
      }
      
      block = block->next;
   }
      
   unlock_gate(&(proc->lock), LOCK_WRITE);
   return e_not_found;
}

kresult proc_initialise(void)
{
   payload_descr payload;
   mb_module_t *module = NULL;
   kresult err;
   process *first_proc = NULL;
   unsigned int loop, module_loop;

   /* initialise critical section lock */
   vmm_memset(&proc_lock, 0, sizeof(rw_gate));
   
   /* initialise the roles table */
   vmm_memset(&proc_roles, 0, DIOSIX_ROLES_NR * sizeof(process *));
   
   /* initialise the role snoozer table */
   vmm_memset(&role_wait_list, 0, DIOSIX_ROLES_NR * sizeof(role_snoozer *));
   
   /* initialise proc hash table */
   err = vmm_malloc((void **)&proc_table, sizeof(process *) * PROC_HASH_BUCKETS);
   if(err) return err; /* fail if we can't even alloc a process hash table */
   
   for(loop = 0; loop < PROC_HASH_BUCKETS; loop++)
      proc_table[loop] = NULL;
   
   BOOT_DEBUG("[proc:%i] initialised process hash table %p... %i buckets %i max procs\n",
              CPU_ID, proc_table, PROC_HASH_BUCKETS, PROC_MAX_NR);
   
   /* get the lowlevel layer initialised before we start the operating system */
   lowlevel_proc_preinit();
   
   /* turn loaded modules into processes ready to run */
   for(module_loop = 0; module_loop < payload_modulemax; module_loop++)
   {      
      process *new = NULL;
      unsigned int virtual, virtual_top, physical;
      payload_type type;
      
      module = payload_readmodule(module_loop);
      
      type = payload_parsemodule(module, &payload);
      
      /* give up if malformed binaries are in the payload */
      if(type == payload_bad)
      {
         BOOT_DEBUG("[proc:%i] failed to parse payload module at %x !\n", CPU_ID, module);
         return e_failure;
      }

      /* create a bare-bones process for an executable in the payload */
      if(type == payload_exe)
      {
         new = proc_new(first_proc, NULL);
         if(!new) return e_failure; /* bail if we can't start up our modules */
         
         /* take a copy of the first process's structure pointer.
            it will be the parent for all others loaded here */
         if(!first_proc) first_proc = new;
         
         /* build page tables for this module */
         BOOT_DEBUG("[proc:%i] preparing system process '%s'...\n", CPU_ID, payload.name);
         
         if(payload.areas[PAYLOAD_CODE].flags & (PAYLOAD_READ | PAYLOAD_EXECUTE))
         {
            unsigned int code_virtual_flags = VMA_READABLE | VMA_FIXED | VMA_EXECUTABLE | VMA_TEXT | VMA_MEMSOURCE;
            unsigned int code_physical_flags = PG_PRESENT | PG_PRIVLVL;

            virtual = (unsigned int)payload.areas[PAYLOAD_CODE].virtual;
            virtual_top = virtual + payload.areas[PAYLOAD_CODE].memsize;
            
            /* round down the base addresses to the nearest page boundary */
            virtual &= ~MEM_PGMASK;
            physical = (unsigned int)KERNEL_LOG2PHYS(payload.areas[PAYLOAD_CODE].physical) & ~MEM_PGMASK;

            while(virtual <= virtual_top)
            {
               pg_add_4K_mapping(new->pgdir, virtual, physical, code_physical_flags);
               virtual += MEM_PGSIZE;
               physical += MEM_PGSIZE;
            }
            
            err = vmm_add_vma(new, (unsigned int)payload.areas[PAYLOAD_CODE].virtual,
                              payload.areas[PAYLOAD_CODE].memsize, code_virtual_flags, 0);
            if(err)
            {
               KOOPS_DEBUG("[proc:%i] WTF? failed to create executable area for process (%p %x %x %x)\n",
                           CPU_ID, new, (unsigned int)payload.areas[PAYLOAD_CODE].virtual,
                           payload.areas[PAYLOAD_CODE].memsize, code_virtual_flags);
               return err;
            }
         }
         if(payload.areas[PAYLOAD_DATA].flags & (PAYLOAD_READ))
         {
            unsigned int pflags = PG_PRESENT | PG_PRIVLVL;
            unsigned int vflags = VMA_FIXED | VMA_DATA | VMA_MEMSOURCE;
            
            if(payload.areas[PAYLOAD_DATA].flags & PAYLOAD_WRITE)
            {
               pflags = pflags | PG_RW;
               vflags = vflags | VMA_WRITEABLE;
            }
            
            virtual = (unsigned int)payload.areas[PAYLOAD_DATA].virtual;
            virtual_top = virtual + payload.areas[PAYLOAD_DATA].memsize;
            
            /* round down the base addresses to the nearest page boundary */
            virtual &= ~MEM_PGMASK;
            physical = (unsigned int)KERNEL_LOG2PHYS(payload.areas[PAYLOAD_DATA].physical) & ~MEM_PGMASK;

            while(virtual <= virtual_top)
            {
               pg_add_4K_mapping(new->pgdir, virtual, physical, pflags);
               virtual += MEM_PGSIZE;
               physical += MEM_PGSIZE;
            }
            
            err = vmm_add_vma(new, (unsigned int)payload.areas[PAYLOAD_DATA].virtual,
                             payload.areas[PAYLOAD_DATA].memsize, vflags, 0);
            if(err)
            {
               KOOPS_DEBUG("[proc:%i] WTF? failed to create data area for process (%p %x %x %x)\n",
                           CPU_ID, new, (unsigned int)payload.areas[PAYLOAD_DATA].virtual,
                           payload.areas[PAYLOAD_DATA].memsize, vflags);
               return err;
            }
         }
         
         if(!new->thread_count)
         {
            BOOT_DEBUG("[proc:%i] OMGWTF system process %i (%p) thread creation failed!\n",
                       CPU_ID, new->pid, new);
            if(first_proc == new) debug_panic("failed to create first userland thread");
         }
         
         /* kernel payload binaries start in the executive layer */
         new->layer = LAYER_EXECUTIVE;
         new->flags |= FLAGS_EXECUTIVE;
      
#ifdef ARCH_HASIOPORTS
         /* create a blank io port access bitmap for the process */
         lowlevel_ioports_new(new);
#endif

         /* set the entry program counter and get ready to run it */
         new->entry = (unsigned int)payload.entry;
         sched_move_to_end(sched_pick_queue(CPU_ID), thread_find_any_thread(new));
      }
   }

   return success;
}
