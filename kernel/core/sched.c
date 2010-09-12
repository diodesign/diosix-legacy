/* kernel/core/sched.c
 * diosix portable process and thread scheduling code
 * Author : Chris Williams
 * Date   : Wed,21 Oct 2009.03:18:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

unsigned int tick = SCHED_CARETAKER;

/* provide locking around critical sections */
rw_gate sched_lock;

/* act as a 'clock arm' selecting cpu queues one by one */
unsigned char sched_next_queue = 0;
unsigned int sched_total_queued = 0; /* number of threads queued */

/* sched_inc_queued_threads
 Increment the number of queued threads total in a thread-safe manner */
void sched_inc_queued_threads(void)
{   
   lock_gate(&(sched_lock), LOCK_WRITE);
   sched_total_queued++;
   unlock_gate(&(sched_lock), LOCK_WRITE);
   
   SCHED_DEBUG("[sched:%i] total queued threads now: %i (up one)\n", CPU_ID, sched_total_queued);
}

/* sched_dec_queued_threads
   Decrement the number of queued threads total in a thread-safe manner */
void sched_dec_queued_threads(void)
{
   lock_gate(&(sched_lock), LOCK_WRITE);
   sched_total_queued--;
   unlock_gate(&(sched_lock), LOCK_WRITE);
   
   SCHED_DEBUG("[sched:%i] total queued threads now: %i (down one)\n", CPU_ID, sched_total_queued);
}

/* sched_lock_thread
 Stop a thread from running, remove it from the queue and lock
 it out until it is unlocked. This will momentarily block until
 the scheduler is satisfied the thread is no longer running
 across the system. It is inappropriate for a thread to
 lock itself as this will leave the system in an unstable state
 on exit from this function.
 => victim = thread to pause
 <= success or a failure code
 */
kresult sched_lock_thread(thread *victim)
{
   unsigned char id = CPU_ID;
   
   if(!victim) return e_failure;
   
   lock_gate(&(cpu_table[id].lock), LOCK_READ);
   
   if(victim == cpu_table[id].current)
   {
      unlock_gate(&(cpu_table[id].lock), LOCK_READ);
      return e_failure;
   }
   
   unlock_gate(&(cpu_table[id].lock), LOCK_READ);
   
   /* if the process is running then interrupt the other cpu */
   lock_gate(&(victim->lock), LOCK_READ);
   
   if(victim->state == inrunqueue)
      sched_remove(victim, held);
   
   unlock_gate(&(victim->lock), LOCK_READ);
   
   return success;
}

/* sched_lock_proc
   Stop a process from running, remove it from the queue and lock
   it out until it is unlocked. This will momentarily block until
   the scheduler is satisfied the process is no longer running
   across the system. It is inappropriate for a process to
   lock itself as this will leave the system in an unstable state
   on exit from this function.
   => proc = process to pause
   <= success or a failure code
*/
kresult sched_lock_proc(process *proc)
{
   thread *t;
   unsigned char id = CPU_ID;
   unsigned int loop, tfound;
   
   if(!proc) return e_failure;
   
   lock_gate(&(cpu_table[id].lock), LOCK_READ);
   lock_gate(&(cpu_table[id].current->lock), LOCK_READ);
   
   if(proc == cpu_table[id].current->proc)
   {
      unlock_gate(&(cpu_table[id].current->lock), LOCK_READ);
      unlock_gate(&(cpu_table[id].lock), LOCK_READ);
      return e_failure;
   }

   unlock_gate(&(cpu_table[id].current->lock), LOCK_READ);
   unlock_gate(&(cpu_table[id].lock), LOCK_READ);
   
   SCHED_DEBUG("[sched:%i] locking process %i (%p)\n", CPU_ID, proc->pid, proc);
   
   /* make sure we're the only one's updating this process */
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   /* warn the scheduler not to run threads in this process */
   proc->flags |= PROC_FLAG_RUNLOCKED;
   
   /* loop through the threads making sure none are running */
   for(loop = 0; loop < THREAD_HASH_BUCKETS; loop++)
   {
      t = proc->threads[loop];
      while(t)
      {
         
         sched_lock_thread(t);
         tfound++;
         
         /* try the next thread */
         lock_gate(&(t->lock), LOCK_READ);
         t = t->hash_next;
         unlock_gate(&(t->lock), LOCK_READ);
      }
      
      /* avoid scanning the whole hash space */
      if(tfound >= proc->thread_count) break;
   }
   
   unlock_gate(&(proc->lock), LOCK_WRITE);
   
   return success;
}

/* sched_unlock_thread
 Release a thread from a held state and allow it to
 run again. It is not appropriate for the currently running process to
 somehow wake itself up.
 => proc = process to release
 <= success or a failure code
 */
kresult sched_unlock_thread(thread *towake)
{
   unsigned char id = CPU_ID;
   
   if(!towake) return e_failure;
   
   lock_gate(&(cpu_table[id].lock), LOCK_READ);
   
   if(towake == cpu_table[id].current)
   {
      unlock_gate(&(cpu_table[id].lock), LOCK_READ);
      return e_failure;
   }
   
   unlock_gate(&(cpu_table[id].lock), LOCK_READ);
   
   lock_gate(&(towake->lock), LOCK_READ);
   
   /* if the thread is held then put it back on the run queue, but 
      don't requeue if the whole process is blocked */
   if(towake->state == held && !(towake->proc->flags & PROC_FLAG_RUNLOCKED))
      sched_add(towake->proc->cpu, towake->priority, towake);
   else
   {
      unlock_gate(&(towake->lock), LOCK_READ);
      return e_failure;
   }

   unlock_gate(&(towake->lock), LOCK_READ);
   
   return success;
}

/* sched_unlock_proc
   Release a previously process from a held state and allow its threads to
   run again. It is not appropriate for the currently running process to
   somehow wake itself up.
   => proc = process to release
   <= success or a failure code
*/
kresult sched_unlock_proc(process *proc)
{
   thread *t;
   unsigned int loop, tfound;
   unsigned char id = CPU_ID;
   
   if(!proc) return e_failure;
   
   lock_gate(&(cpu_table[id].lock), LOCK_READ);
   lock_gate(&(cpu_table[id].current->lock), LOCK_READ);
   
   if(proc == cpu_table[id].current->proc)
   {
      unlock_gate(&(cpu_table[id].current->lock), LOCK_READ);
      unlock_gate(&(cpu_table[id].lock), LOCK_READ);
      return e_failure;
   }
   
   unlock_gate(&(cpu_table[id].current->lock), LOCK_READ);
   unlock_gate(&(cpu_table[id].lock), LOCK_READ);
   
   SCHED_DEBUG("[sched:%i] unlocking process %i (%p)\n", CPU_ID, proc->pid, proc);
   
   lock_gate(&(proc->lock), LOCK_WRITE);
   
   /* clear the blocked flag */
   proc->flags &= ~PROC_FLAG_RUNLOCKED;
   
   /* loop through the threads to run any that are held */
   for(loop = 0; loop < THREAD_HASH_BUCKETS; loop++)
   {
      t = proc->threads[loop];
      while(t)
      {
         sched_unlock_thread(t);
         tfound++;
         
         /* try the next thread */
         lock_gate(&(t->lock), LOCK_READ);
         t = t->hash_next;
         unlock_gate(&(t->lock), LOCK_READ);
      }
      
      /* avoid scanning the whole hash space */
      if(tfound >= proc->thread_count) break;
   }
   
   unlock_gate(&(proc->lock), LOCK_WRITE);
   
   return success;
}

/* sched_caretaker
   Run maintenance over the queues to ensure even load across
   cpus by adjusting the cpu id for waiting threads. if a
   thread is to wake up on a loaded cpu, then move it to a
   quieter one. don't move running threads around for fear of
   cache performance issues */
void sched_caretaker(void)
{
   SCHED_DEBUG("[sched:%i] caretaker tick\n", CPU_ID);
}

/* sched_tick
   Called 100 times a second (SCHED_FREQUENCY). Pick a new
   thread, if necessary.
*/
void sched_tick(int_registers_block *regs)
{
   mp_core *cpu = &cpu_table[CPU_ID];

   /* give up if no thread running */
   if(!(cpu->current)) return;
   
   /* check to see if it's time for maintanence */
   /* make sure only the boot cpu runs this? */
   if(CPU_ID == mp_boot_cpu)
   {
      if(tick)
         tick--;
      else
      {
         sched_caretaker();
         tick = SCHED_CARETAKER;
      }
   }
   
   lock_gate(&(cpu->lock), LOCK_READ);
   /* bail out if we're not running anything */
   if(!(cpu->current))
   {
      unlock_gate(&(cpu->lock), LOCK_READ);
      return;
   }
   unlock_gate(&(cpu->lock), LOCK_READ);
   
   lock_gate(&(cpu->current->lock), LOCK_WRITE);
   
   /* decrement timeslice */
   if(cpu->current->timeslice)
      cpu->current->timeslice--;
   
   /* reschedule if thread is out of time */
   if(cpu->current->timeslice == 0)
   {
      SCHED_DEBUG("[sched:%i] timeslice for thread %i of process %i expired\n",
              CPU_ID, cpu->current->tid, cpu->current->proc->pid);
      
      unlock_gate(&(cpu->current->lock), LOCK_WRITE);
      
      sched_move_to_end(CPU_ID, cpu->current->priority, cpu->current);
      sched_pick(regs);
   }
   else
      unlock_gate(&(cpu->current->lock), LOCK_WRITE);
}

/* sched_pick
   Check the run queues for new higher prority threads to run
   => pointer to the interrupted thread's kernel stack
*/
void sched_pick(int_registers_block *regs)
{
   thread *now, *next;

   mp_core *cpu = &cpu_table[CPU_ID];
   
#ifdef SCHED_DEBUG
   {
      thread *t = cpu->queue_head;
      if(!t)
      {
         SCHED_DEBUG("[sched:%i] queue empty!\n", CPU_ID);
      }
      else
      {
         SCHED_DEBUG("[sched:%i] picking from cpu %i queue: start[t%i p%i] ",
                     CPU_ID, CPU_ID, t->tid, t->proc->pid);
         while(t)
         {
            SCHED_DEBUG("(t%i p%i) ", t->tid, t->proc->pid);
            t = t->queue_next;
         }
         if(cpu->queue_tail)
         {
            SCHED_DEBUG("end[t%i p%i]\n", cpu->queue_tail->tid, cpu->queue_tail->proc->pid);
         }
         else 
         {
            SCHED_DEBUG("end[NULL]\n");
         }
      }
   }
#endif   

   /* this is the state of play */
   lock_gate(&(cpu->lock), LOCK_READ);
   now = cpu->current;
   unlock_gate(&(cpu->lock), LOCK_READ);
   
   /* if the queue head is empty, then spin until it's there */
   do
   {
      lock_gate(&(cpu->lock), LOCK_READ);
      next = (volatile thread *)(cpu->queue_head);
      unlock_gate(&(cpu->lock), LOCK_READ);
   }
   while (!next);
   
   if(next == now) return; /* easy quick switch back to where we were */
   
   /* update thread states */
   lock_gate(&(now->lock), LOCK_WRITE);
   lock_gate(&(next->lock), LOCK_WRITE);
   
   if(now->state == running) now->state = inrunqueue;
   next->cpu = CPU_ID;
   next->state = running;
   
   unlock_gate(&(next->lock), LOCK_WRITE);
   unlock_gate(&(now->lock), LOCK_WRITE);

   /* this is the only place in core that we change cpu->current. the (un)lock_gate
      code relies on cpu->current not changing between lock-unlock pairs for a
      given kernel thread. so we have to use a basic low-level spin lock on the
      cpu's gate while we update this */   
   lock_spin(&(cpu->lock.spinlock));
   cpu->current = next;
   unlock_spin(&(cpu->lock.spinlock));

   lowlevel_thread_switch(now, next, regs);
   
   SCHED_DEBUG("[sched:%i] switched thread %i of process %i (%p) for thread %i of process %i (%p)\n",
           CPU_ID, now->tid, now->proc->pid, now, next->tid, next->proc->pid, next);
}

/* sched_move_to_end
   Put a thread at the end of a run queue
   => cpu = id of per-cpu run queue
      priority = priority queue to use
      toqueue = thread to queue up
*/
void sched_move_to_end(unsigned char cpu, unsigned char priority, thread *toqueue)
{
   if((cpu > mp_cpus) || !toqueue)
      return; /* bail if parameters are insane */
   
   lock_gate(&(toqueue->lock), LOCK_WRITE);
   
   /* remove from the run queue if present */
   if(toqueue->state == running || toqueue->state == inrunqueue)
      sched_remove(toqueue, held);
   
   lock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   
   /* add it to the end of the queue */
   if(cpu_table[cpu].queue_tail)
      cpu_table[cpu].queue_tail->queue_next = toqueue;

   toqueue->queue_prev = cpu_table[cpu].queue_tail;
   
   /* if the head is empty, then fix up */
   if(cpu_table[cpu].queue_head == NULL)
      cpu_table[cpu].queue_head = toqueue;
   
   cpu_table[cpu].queue_tail = toqueue;
   toqueue->queue_next = NULL;
   
   /* cap out-of-control priorities */
   if(priority >= SCHED_PRIORITY_LEVELS)
      priority = SCHED_PRIORITY_LEVELS - 1;
   
   /* update accounting */
   if(toqueue->state != running && toqueue->state != inrunqueue)
   {
      cpu_table[cpu].queued++;
      sched_inc_queued_threads();
   }

   toqueue->priority  = priority;
   toqueue->state     = inrunqueue;
   toqueue->timeslice = SCHED_TIMESLICE;
   
   unlock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   unlock_gate(&(toqueue->lock), LOCK_WRITE);
   
   SCHED_DEBUG("[sched:%i] moved thread %i (%p) of process %i to end of cpu %i queue, priority %i\n",
           CPU_ID, toqueue->tid, toqueue, toqueue->proc->pid, cpu, priority);
}

/* sched_add
   Add a thread to a run queue for a cpu
   => cpu = id of the requested per-cpu run queue
      priority = the priority level of the process
      torun = the thread to add
*/
void sched_add(unsigned char cpu, unsigned char priority, thread *torun)
{
   if((cpu > mp_cpus) || !torun)
      return; /* bail if parameters are insane */

   /* perform some load balancing */
   cpu = sched_pick_queue(cpu);
      
   lock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   lock_gate(&(torun->lock), LOCK_WRITE);
      
   /* add it to the start of the queue */
   if(cpu_table[cpu].queue_head)
   {
      cpu_table[cpu].queue_head->queue_prev = torun;
      torun->queue_next = cpu_table[cpu].queue_head;
   }
   else
   {
      torun->queue_next = NULL;
   }
   /* if the tail is empty, then fix up */
   if(!(cpu_table[cpu].queue_tail))
      cpu_table[cpu].queue_tail = torun;
   
   cpu_table[cpu].queue_head = torun;
   torun->queue_prev = NULL;
      
   /* cap out-of-control priorities */
   if(priority >= SCHED_PRIORITY_LEVELS)
      priority = SCHED_PRIORITY_LEVELS - 1;
   
   /* update accounting */
   if(torun->state != running && torun->state != inrunqueue)
   {
      cpu_table[cpu].queued++;
      sched_inc_queued_threads();
   }

   torun->priority  = priority;
   torun->state     = inrunqueue;
   torun->timeslice = SCHED_TIMESLICE;
   torun->cpu        = cpu;
   
   unlock_gate(&(torun->lock), LOCK_WRITE);
   unlock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
      
   SCHED_DEBUG("[sched:%i] added thread %i (%p) of process %i to cpu %i queue, priority %i\n",
           CPU_ID, torun->tid, torun, torun->proc->pid, cpu, priority);
}

/* sched_remove
   Remove a thread from a run queue for a cpu
   => victim = the thread to remove
      state = new thread state
*/
void sched_remove(thread *victim, thread_state state)
{
   unsigned int cpu = victim->cpu;
   
   lock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   lock_gate(&(victim->lock), LOCK_WRITE);
   
   /* remove it from the queue */
   if(victim->queue_next)
      victim->queue_next->queue_prev = victim->queue_prev;
   else
      /* we were the tail, so fix up */
      cpu_table[cpu].queue_tail = victim->queue_prev;
   if(victim->queue_prev)
      victim->queue_prev->queue_next = victim->queue_next;
   else
   /* we were the queue head, so fixup pointers */
      cpu_table[cpu].queue_head = victim->queue_next;
   
   /* update accounting */
   if(victim->state == running || victim->state == inrunqueue)
   {
      cpu_table[cpu].queued--;
      sched_dec_queued_threads();
   }

   victim->state = state;
   
   unlock_gate(&(victim->lock), LOCK_WRITE);   
   unlock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   
   SCHED_DEBUG("[sched:%i] removed thread %i (%p) of process %i from cpu %i queue, priority %i\n",
           CPU_ID, victim->tid, victim, victim->proc->pid, cpu, victim->priority);
}

/* sched_pick_queue
   Balance load across the per-cpu queues by picking a queue to schedule a thread
   to run on, biased in favour of the hinted cpu queue
   => hint = hinted cpu queue 
   <= cpu queue to use
*/
unsigned char sched_pick_queue(unsigned char hint)
{
   unsigned char picked;
   unsigned int max_fair_share, original_sched_next;
   
   /* use the boot cpu queue if it's the only cpu */
   if(mp_cpus == 1) return mp_boot_cpu;
   
   /* use the boot cpu queue if the hint is wild */
   if(hint >= mp_cpus) hint = mp_boot_cpu;
   
   /* protect the scheduler's critical section right here */
   lock_gate(&(sched_lock), LOCK_WRITE);

   /* the number of threads per cpu queue that's 'fair' to run
      is simply the total number of threads divided by cpu queues 
      available */
   max_fair_share = sched_total_queued / mp_cpus;
   if(!max_fair_share) max_fair_share = 1; /* ensure a sane value */

   /* ensure next_queue is sane */
   if(sched_next_queue >= mp_cpus) sched_next_queue = 0;
   
   /* make sure the next queue is not the hinted queue */
   if(sched_next_queue == hint)
   {
      sched_next_queue++;
      if(sched_next_queue >= mp_cpus) sched_next_queue = 0;
   }
   
   SCHED_DEBUG("[sched:%i] load balancing: max threads per cpu: %i next queue: %i (%i) hint: %i (%i) total queued: %i\n",
               CPU_ID, max_fair_share, sched_next_queue, cpu_table[sched_next_queue].queued,
               hint, cpu_table[hint].queued, sched_total_queued);
   
   /* the strategy is thus: use the hinted queue unless it has more than its
      fair share of threads or the next queue is empty. in which case, use the next queue if possible and advance it if
      successful. if the hinted queue is empty, then use that */
   original_sched_next = sched_next_queue;
   lock_gate(&(cpu_table[hint].lock), LOCK_READ);
   lock_gate(&(cpu_table[original_sched_next].lock), LOCK_READ);
   
   if(((cpu_table[sched_next_queue].queued == 0) ||
       (cpu_table[hint].queued > max_fair_share)) &&
      (cpu_table[hint].queued != 0))
   {
      picked = sched_next_queue;
      sched_next_queue++;
   }
   else
      picked = hint;

   SCHED_DEBUG("[sched:%i] load balancing: selected queue: %i\n",
               CPU_ID, picked);
   
   unlock_gate(&(cpu_table[original_sched_next].lock), LOCK_READ);
   unlock_gate(&(cpu_table[hint].lock), LOCK_READ);         

   unlock_gate(&(sched_lock), LOCK_WRITE);
   
   return picked;
}

/* sched_pre_initalise
   Perform initialisation prior to any sched_*() calls */
void sched_pre_initalise(void)
{
   /* initialise lock */
   vmm_memset(&sched_lock, 0, sizeof(rw_gate));   
}

/* sched_initialise
   Prepare the default scheduler for action */
void sched_initialise(void)
{
   BOOT_DEBUG("[sched:%i] starting operating system...\n", CPU_ID);

   /* start running process 1, thread 1 in user mode, which
      should spawn system managers and continue the boot process */
   while(1) lowlevel_kickstart();
}
