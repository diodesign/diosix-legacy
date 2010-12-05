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

/* keep a record of the system executive's process structure */
process *proc_sys_executive = NULL;

unsigned int next_pid = FIRST_PID;
unsigned int proc_count = 0;

/* provide locking around critical sections */
rw_gate proc_lock;

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
                new child on, or NULL for none
      caller = thread that invoked this function or NULL if
               the kernel called to generate a new process
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
   
   /* call the port-specific process creation code, with current=NULL to indicate
      if we're calling from before userspace has been set up */
   pg_new_process(new, current);   
   vmm_duplicate_vmas(new, current); /* and clone the vmas */
   
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
      new->next_tid      = current->next_tid;
      
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
      
      /* duplicate the running thread */
      if(thread_new_hash(new))
      {
         proc_remove_child(current, new);
         vmm_free(new); /* tidy up */
         unlock_gate(&(current->lock), LOCK_WRITE);
         return NULL;
      }
      
      dupthread = thread_duplicate(new, caller);
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
   
   /* we have a green light, so reverse the process creation
      routines in proc_new() */
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
   lock_gate(&proc_lock, LOCK_WRITE);
   
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
            proc_attach_child(proc_sys_executive, victim->children[loop]);
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

kresult proc_initialise(void)
{
   payload_descr payload;
   mb_module_t *module = NULL;
   kresult err;
   unsigned int loop, module_loop;

   /* initialise critical section lock */
   vmm_memset(&proc_lock, 0, sizeof(rw_gate));
   
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
         new = proc_new(NULL, NULL);
         if(!new) return e_failure; /* bail if we can't start up our modules */
         
         /* build page tables for this module */
         BOOT_DEBUG("[proc:%i] preparing system process '%s'...\n", CPU_ID, payload.name);
         
         if(payload.areas[PAYLOAD_CODE].flags & (PAYLOAD_READ | PAYLOAD_EXECUTE))
         {         
            BOOT_DEBUG("       code: entry %p virt %p phys %p size %u memsize %u flags %u\n", 
                    payload.entry,
                    payload.areas[PAYLOAD_CODE].virtual, payload.areas[PAYLOAD_CODE].physical,
                    payload.areas[PAYLOAD_CODE].size, payload.areas[PAYLOAD_CODE].memsize,
                    payload.areas[PAYLOAD_CODE].flags);
            
            virtual = (unsigned int)payload.areas[PAYLOAD_CODE].virtual;
            virtual_top = virtual + payload.areas[PAYLOAD_CODE].memsize;
            physical = (unsigned int)KERNEL_LOG2PHYS(payload.areas[PAYLOAD_CODE].physical);
            while(virtual < virtual_top)
            {
               pg_add_4K_mapping(new->pgdir, virtual, physical, PG_PRESENT | PG_PRIVLVL);
               virtual += MEM_PGSIZE;
               physical += MEM_PGSIZE;
            }
            
            vmm_add_vma(new, (unsigned int)payload.areas[PAYLOAD_CODE].virtual,
                        payload.areas[PAYLOAD_CODE].memsize, VMA_READABLE | VMA_FIXED, 0);
         }
         if(payload.areas[PAYLOAD_DATA].flags & (PAYLOAD_READ))
         {
            unsigned int flags = PG_PRESENT | PG_PRIVLVL;
            
            if(payload.areas[PAYLOAD_DATA].flags & PAYLOAD_WRITE)
               flags = flags | PG_RW;
            
            BOOT_DEBUG("       data:          virt %p phys %p size %u memsize %u flags %u\n", 
                    payload.areas[PAYLOAD_DATA].virtual, payload.areas[PAYLOAD_DATA].physical,
                    payload.areas[PAYLOAD_DATA].size, payload.areas[PAYLOAD_DATA].memsize,
                    payload.areas[PAYLOAD_DATA].flags);
            
            virtual = (unsigned int)payload.areas[PAYLOAD_DATA].virtual;
            virtual_top = virtual + payload.areas[PAYLOAD_DATA].memsize;
            physical = (unsigned int)KERNEL_LOG2PHYS(payload.areas[PAYLOAD_DATA].physical);
            while(virtual < virtual_top)
            {
               pg_add_4K_mapping(new->pgdir, virtual, physical, flags);
               virtual += MEM_PGSIZE;
               physical += MEM_PGSIZE;
            }
            
            vmm_add_vma(new, (unsigned int)payload.areas[PAYLOAD_DATA].virtual,
                        payload.areas[PAYLOAD_DATA].memsize, VMA_WRITEABLE | VMA_FIXED, 0);
         }
         
         if(!(new->threads[FIRST_TID]))
         {
            BOOT_DEBUG("[proc:%i] OMGWTF system process %i (%p) thread creation failed!\n",
                       CPU_ID, new->pid, new);
            debug_panic("failed to create first userland thread");
         }
         
         /* kernel payload binaries start in the executive layer */
         new->layer = LAYER_EXECUTIVE;
         new->flags |= FLAGS_EXECUTIVE;
         
         /* create a blank io port access bitmap for the process */
         lowlevel_ioports_new(new);
         
         proc_sys_executive = new;
         
         /* set the entry program counter and get ready to run it */
         new->entry = (unsigned int)payload.entry;
         sched_add(CPU_ID, new->threads[FIRST_TID]);
      }
   }

   return success;
}
