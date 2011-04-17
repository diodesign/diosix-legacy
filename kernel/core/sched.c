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
kpool *sched_bedroom; /* queued pool of sleeping threads waiting for an alarm timeout */

/* provide spinlocking around critical sections */
volatile unsigned int sched_next_queue_slock = 0;
volatile unsigned int sched_total_queued_slock = 0;

/* act as a 'clock arm' selecting cpu queues one by one */
unsigned char sched_next_queue = 0;
unsigned int sched_total_queued = 0; /* number of threads queued */

/* sched_inc_queued_threads
 Increment the number of queued threads total in a thread-safe manner */
void sched_inc_queued_threads(void)
{
   lock_spin(&(sched_total_queued_slock));
   sched_total_queued++;
   unlock_spin(&(sched_total_queued_slock));
   
   SCHED_DEBUG("[sched:%i] total queued threads now: %i (up one)\n", CPU_ID, sched_total_queued);
}

/* sched_dec_queued_threads
   Decrement the number of queued threads total in a thread-safe manner */
void sched_dec_queued_threads(void)
{
   lock_spin(&(sched_total_queued_slock));
   sched_total_queued--;
   unlock_spin(&(sched_total_queued_slock));
   
   SCHED_DEBUG("[sched:%i] total queued threads now: %i (down one)\n", CPU_ID, sched_total_queued);
}

/* sched_determine_priority
   Return the priority level of the given thread - assumes lock is held on the thread's metadata */
unsigned char sched_determine_priority(thread *target)
{
   unsigned char priority;
   
   /* driver threads always get priority 0 (or 1 if punished) */
   if(target->flags & THREAD_FLAG_ISDRIVER) return target->priority;
   
   /* if the thread's been granted a better priority then use that */
   if(target->priority_granted != SCHED_PRIORITY_INVALID &&
      target->priority > target->priority_granted)
      priority = target->priority_granted;
   else
      priority = target->priority;
   
   /* sanitise the priority in case something's gone wrong */
   if(priority > SCHED_PRIORITY_MAX) priority = SCHED_PRIORITY_MAX;

   SCHED_DEBUG("[sched:%i] determined thread %i of process %i (%p) has priority %i (curr %i granted %i)\n",
               CPU_ID, target->tid, target->proc->pid, target, priority, target->priority, target->priority_granted);
   
   return priority;
}

/* sched_get_next_to_run
   Return a pointer to the thread that should be run next on the
   given core, or NULL for no threads to run */
thread *sched_get_next_to_run(unsigned char cpuid)
{
   mp_core *cpu = &cpu_table[cpuid];
   mp_thread_queue *cpu_queue;
   volatile thread *torun;

   lock_gate(&(cpu->lock), LOCK_READ);
   cpu_queue = &(cpu->queues[cpu->lowest_queue_filled]);
   torun = (volatile thread *)(cpu_queue->queue_head);
   unlock_gate(&(cpu->lock), LOCK_READ);
   
   return (thread *)torun;
}

/* sched_priority_calc
   Re-calculate the priority points for a thread: either reset a thread's
   points to their base score for its priority level or subtract a point or 
   add a point or sanity check the points score.
   => tocalc = pointer to thread to operate on
      request = priority_reset, priority_reward, priority_punish,
                priority_expiry_punish or priority_check
*/
void sched_priority_calc(thread *tocalc, sched_priority_request request)
{
   unsigned int priority, priority_low_limit, priority_high_limit;
   
   /* sanatise input */
   if(!tocalc)
   {
      KOOPS_DEBUG("[sched:%i] sched_priority_calc: called with nonsense thread pointer\n",
                  CPU_ID);
      return;
   }
   
   /* interrupt-handling driver threads are immune to these checks
      except punishment for exceeding their timeslice, which shouldn't
      happen to a well-behaving driver thread */
   if(tocalc->flags & THREAD_FLAG_ISDRIVER)
   {
      lock_gate(&(tocalc->lock), LOCK_WRITE);
      
      if(request == priority_expiry_punish)
         /* a punished driver thread sits in priority queue above the
            well-behaved interrupt driver threads */
         tocalc->priority = SCHED_PRIORITY_INTERRUPTS + 1;
      else
         tocalc->priority = SCHED_PRIORITY_INTERRUPTS;
      
      unlock_gate(&(tocalc->lock), LOCK_WRITE);
      return;
   }
   
   lock_gate(&(tocalc->lock), LOCK_WRITE);
   
   /* get the correct priority level to use */
   priority = sched_determine_priority(tocalc);
   
   /* determine the actual low and high limits */
   if(tocalc->proc->priority_low > SCHED_PRIORITY_MIN)
      priority_low_limit = tocalc->proc->priority_low;
   else
      priority_low_limit = SCHED_PRIORITY_MIN;
   
   if(tocalc->proc->priority_high < SCHED_PRIORITY_MAX)
      priority_high_limit = tocalc->proc->priority_high;
   else
      priority_high_limit = SCHED_PRIORITY_MAX;
   
   switch(request)
   {
      /* set the base priority points for a thread in its priority level */
      case priority_reset:
         tocalc->priority_points = SCHED_PRIORITY_BASE_POINTS(priority);
         break;
         
      /* add a scheduling point */
      case priority_reward:
         if(tocalc->priority_points < SCHED_PRIORITY_MAX_POINTS(priority))
            tocalc->priority_points++;
         
         /* if we exceed (2 * (2 ^ priority)) then enhance the priority level (by reducing it) */
         if(tocalc->priority_points == SCHED_PRIORITY_MAX_POINTS(priority))
         {
            if(tocalc->priority > priority_low_limit)
            {
               tocalc->priority--;
               tocalc->priority_points = SCHED_PRIORITY_BASE_POINTS(priority);
            }
         }
         break;
         
      /* subtract a scheduling point */   
      case priority_expiry_punish:
      case priority_punish:
         if(tocalc->priority_points > 0)
            tocalc->priority_points--;
         
         /* if we drop to zero then diminish the priority level (by increasing it) */
         if(tocalc->priority_points == 0)
         {
            if(tocalc->priority < priority_high_limit)
            {
               tocalc->priority++;
               tocalc->priority_points = SCHED_PRIORITY_BASE_POINTS(priority);
            }
         }
         break;
         
      /* check everything's sane */
      case priority_check:
         if(tocalc->priority_points > SCHED_PRIORITY_MAX_POINTS(priority))
            tocalc->priority_points = SCHED_PRIORITY_MAX_POINTS(priority);
         if(tocalc->proc->priority_low > tocalc->proc->priority_high)
            priority_low_limit = priority_high_limit = tocalc->proc->priority_low = tocalc->proc->priority_high;
         if(tocalc->priority > priority_high_limit)
            tocalc->priority = priority_high_limit;
         if(tocalc->priority < priority_low_limit)
            tocalc->priority = priority_low_limit;
         if(tocalc->priority_granted != SCHED_PRIORITY_INVALID)
         {
            if(tocalc->priority_granted > SCHED_PRIORITY_MAX)
               tocalc->priority_granted = SCHED_PRIORITY_MAX;
            if(tocalc->priority_granted < SCHED_PRIORITY_MIN)
               tocalc->priority_granted = SCHED_PRIORITY_MIN;
         }
         break;
         
      default:
         KOOPS_DEBUG("[sched:%i] sched_priority_calc: unknown request %i for thread %p\n",
                     CPU_ID, request, tocalc);
   }
   
   unlock_gate(&(tocalc->lock), LOCK_WRITE);
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
      sched_add(towake->proc->cpu, towake);
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
   cache performance issues. if debug is enabled then check
   the sanity of the queues */
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
   if(!cpu_table) return; /* give up now if the system isn't ready yet */
   
   mp_core *cpu = &cpu_table[CPU_ID];
   
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
      
      /* are there any sleeping threads? */
      if(vmm_count_pool_inuse(sched_bedroom))
      {
         snoozing_thread *snoozer = NULL;
         
         for(;;)
         {
            /* step through the sleeping threads and decrease their timer values */
            snoozer = vmm_next_in_pool(snoozer, sched_bedroom);
            if(snoozer)
            {
               snoozer->timer--;
               if(snoozer->timer == 0)
               {
                  switch(snoozer->action)
                  {
                     case wake:
                     {
                        /* wake up the thread */
                        sched_add(snoozer->sleeper->cpu, snoozer->sleeper);
                        
                        SCHED_DEBUG("[sched:%i] woke up snoozing thread %p (tid %i pid %i)\n",
                                    CPU_ID, snoozer->sleeper, snoozer->sleeper->tid, snoozer->sleeper->proc->pid);
                     }

                     case signal:
                     {
                        /* poke the owning process as a SIGALARM signal from the kernel */
                        msg_send_signal(snoozer->sleeper->proc, NULL, SIGALRM, 0);
                        
                        SCHED_DEBUG("[sched:%i] sent SIGALRM to pid %i\n",
                                    CPU_ID, snoozer->sleeper, snoozer->sleeper->proc->pid);
                     }
                  }
               
                  /* and bin the pool block */
                  vmm_free_pool(snoozer, sched_bedroom);
                  snoozer = NULL; /* pointer no longer valid */
                  break;
               }
            } else break; /* escape the loop if no more sleepers */
         }
      }
   }
   
   lock_gate(&(cpu->lock), LOCK_READ);
   
   /* bail out if we're not running anything */
   if(!(cpu->current))
   {
      SCHED_DEBUG("[sched:%i] nothing running on this core\n", CPU_ID);
      unlock_gate(&(cpu->lock), LOCK_READ);
      return;
   }
   unlock_gate(&(cpu->lock), LOCK_READ);
   
   SCHED_DEBUG("[sched:%i] tick for thread %i of process %i (state %i)\n",
               CPU_ID, cpu->current->tid, cpu->current->proc->pid, cpu->current->state);
   
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
      
      /* punish the thread for using up all its timeslice */
      sched_priority_calc(cpu->current, priority_expiry_punish);
      sched_move_to_end(CPU_ID, cpu->current);

      sched_pick(regs);
   }
   else
      unlock_gate(&(cpu->current->lock), LOCK_WRITE);
}

/* sched_remove_snoozer
   Remove a snoozing thread from the queued pool of sleepers
   => snoozer = sleeping thread
   <= 0 for success, or an error code
*/
kresult sched_remove_snoozer(thread *snoozer)
{
   kresult result = e_not_found;
   
   /* sanity check */
   if(!snoozer) return e_bad_params;
   
   snoozing_thread *search = NULL;
   
   for(;;)
   {
      /* step through the sleeping threads to find the given thread */
      search = vmm_next_in_pool(search, sched_bedroom);
      if(search)
      {
         if(search->sleeper == snoozer)
         {
            /* bin the pool block */
            vmm_free_pool(search, sched_bedroom);
            search = NULL; /* pointer no longer valid */
            result = success;
         }
      } else break; /* escape the loop if no more sleepers */
   }
   
   SCHED_DEBUG("[sched:%i] removed thread %p (tid %i pid %i) from bedroom (result %i)\n",
               CPU_ID, snoozer, snoozer->tid, snoozer->proc->pid, result);
   
   return result;
}

/* sched_add_snoozer
   Add a thread to the queued pool of threads waiting on the scheduler clock
   => snoozer = thread to work on
      timeout = number of scheduling ticks to count down from, or 0 to cancel
      action = wake: put calling thread to sleep and wake it when timeout reaches zero
               signal: send a SIGALARM to the thread's owner process when timeout reaches zero
   <= 0 for success, or an error code
*/
kresult sched_add_snoozer(thread *snoozer, unsigned int timeout, sched_snooze_action action)
{
   snoozing_thread *new;
   kresult err;
   
   /* sanity check */
   if(!snoozer) return e_bad_params;
   
   if(!timeout)
      /* cancel all the timers for this thread */
      return sched_remove_snoozer(snoozer);
   
   /* allocate the new block to store the thread's details */
   err = vmm_alloc_pool((void **)&new, sched_bedroom);
   if(err) return err;
   
   new->sleeper = snoozer;
   new->timer   = timeout;
   new->action  = action;
   
   SCHED_DEBUG("[sched:%i] added thread %p (tid %i pid %i) to bedroom: timeout %i ticks action %i\n",
               CPU_ID, snoozer, snoozer->tid, snoozer->proc->pid, timeout, action);
   
   if(action == wake) sched_remove(snoozer, sleeping);

   return success;
}

/* sched_rescan_queues
 Run through a CPU's queues and determine the highest-priority queue with threads waiting in it.
 => cpuid = CPU_ID of core to scan
 */
void sched_rescan_queues(unsigned char cpuid)
{
   /* assumes at least a read lock on the CPU's structure */
   mp_core *cpu = &cpu_table[cpuid];
   unsigned int loop;
   
   for(loop = 0; loop < SCHED_PRIORITY_LEVELS; loop++)
      if(cpu->queues[loop].queue_head)
      {
         cpu->lowest_queue_filled = loop;
         break;
      }
}

/* sched_pick
   Check the run queues for new higher prority threads to run
   and perform a task switch if one is present
   => pointer to the interrupted thread's kernel stack
*/
void sched_pick(int_registers_block *regs)
{
   thread *now, *next = NULL;

   mp_core *cpu = &cpu_table[CPU_ID];
   
   SCHED_DEBUG_QUEUES;
   
   /* this is the state of play */
   lock_gate(&(cpu->lock), LOCK_READ);
   now = cpu->current;
   unlock_gate(&(cpu->lock), LOCK_READ);
   
   /* see if there's another thread to run */
   while(!next)
   {
      next = sched_get_next_to_run(CPU_ID);
      
      /* if there's nothing to run and the current thread is
         marked as running then continue with the current 
         thread */
      if(!next && now)
         if(now->state == running) return;
      
      /* avoid running out of any work to do */
      lock_gate(&(cpu->lock), LOCK_WRITE);
      sched_rescan_queues(CPU_ID);
      unlock_gate(&(cpu->lock), LOCK_WRITE);
   }

   /* keep with the currently running thread if it's
      the highest priority thread allowed to run */
   if(next == now)
   {
      /* ensure the thread's state variable reflects its condition */
      lock_gate(&(next->lock), LOCK_WRITE);
      next->state = running;
      unlock_gate(&(next->lock), LOCK_WRITE);
      
      return; /* easy quick switch back to where we were */
   }
   
   if((next->priority > now->priority) &&
      (now->state == running)) return;
   
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
      toqueue = thread to queue up
*/
void sched_move_to_end(unsigned char cpu, thread *toqueue)
{
   mp_thread_queue *cpu_queue;
   unsigned char priority;
   
   if((cpu > mp_cpus) || !toqueue)
      return; /* bail if parameters are insane */
   
   lock_gate(&(toqueue->lock), LOCK_WRITE);
      
   /* remove from the run queue if present */
   if(toqueue->state == running || toqueue->state == inrunqueue)
      sched_remove(toqueue, held);
   
   lock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);

   priority = sched_determine_priority(toqueue);
   cpu_queue = &(cpu_table[cpu].queues[priority]);
   
   /* add it to the end of the queue */
   if(cpu_queue->queue_tail)
      cpu_queue->queue_tail->queue_next = toqueue;

   toqueue->queue_prev = cpu_queue->queue_tail;
   
   /* if the head is empty, then fix up */
   if(cpu_queue->queue_head == NULL)
      cpu_queue->queue_head = toqueue;
   
   cpu_queue->queue_tail = toqueue;
   toqueue->queue_next = NULL;
   
   /* update accounting */
   if(toqueue->state != running && toqueue->state != inrunqueue)
   {
      cpu_table[cpu].queued++;
      sched_inc_queued_threads();
   }

   toqueue->state     = inrunqueue;
   toqueue->timeslice = SCHED_TIMESLICE;
   toqueue->queue     = cpu_queue;

   /* determine the highest priority thread queue to run */
   if(cpu_table[cpu].lowest_queue_filled != priority)
      sched_rescan_queues(cpu);
   
   unlock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   unlock_gate(&(toqueue->lock), LOCK_WRITE);
   
   SCHED_DEBUG("[sched:%i] moved thread %i (%p) of process %i to end of cpu %i queue, priority %i\n",
           CPU_ID, toqueue->tid, toqueue, toqueue->proc->pid, cpu, priority);
}

/* sched_add
   Add a thread to a run queue for a cpu at the priority
   level set in the thread's structure
   => cpu = id of the requested per-cpu run queue
      torun = the thread to add
*/
void sched_add(unsigned char cpu, thread *torun)
{
   if((cpu > mp_cpus) || !torun)
      return; /* bail if parameters are insane */
   
   unsigned char priority;
   mp_thread_queue *cpu_queue;
      
   /* perform some load balancing */
   cpu = sched_pick_queue(cpu);
      
   lock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   lock_gate(&(torun->lock), LOCK_WRITE);

   /* pick the right queue and add it to the start */
   priority = sched_determine_priority(torun);
   cpu_queue = &(cpu_table[cpu].queues[priority]);
   if(cpu_queue->queue_head)
   {
      cpu_queue->queue_head->queue_prev = torun;
      torun->queue_next = cpu_queue->queue_head;
   }
   else
   {
      torun->queue_next = NULL;
   }
   /* if the tail is empty, then fix up */
   if(!(cpu_queue->queue_tail))
      cpu_queue->queue_tail = torun;
   
   cpu_queue->queue_head = torun;
   torun->queue_prev = NULL;
   
   /* update accounting */
   if(torun->state != running && torun->state != inrunqueue)
   {
      cpu_table[cpu].queued++;
      sched_inc_queued_threads();
   }

   torun->state     = inrunqueue;
   torun->timeslice = SCHED_TIMESLICE;
   torun->cpu       = cpu;
   torun->queue     = cpu_queue;
   
   /* take a note of the highest priority level ready to run */
   cpu_queue = &(cpu_table[cpu].queues[cpu_table[cpu].lowest_queue_filled]);
   if(cpu_table[cpu].lowest_queue_filled > priority || !cpu_queue->queue_head)
      cpu_table[cpu].lowest_queue_filled = priority;
   
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
   mp_thread_queue *cpu_queue;
   
   lock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
   lock_gate(&(victim->lock), LOCK_WRITE);
   
   cpu_queue = victim->queue;
   if(!cpu_queue)
   {
      KOOPS_DEBUG("[sched:%i] OMGWTF sched_remove: tried to remove thread %i of process %i (%p) from non-existent queue\n",
                  CPU_ID, victim->tid, victim->proc->pid, victim);
      
      unlock_gate(&(victim->lock), LOCK_WRITE);   
      unlock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
      return;
   }
   
   /* remove it from the queue */
   if(victim->queue_next)
      victim->queue_next->queue_prev = victim->queue_prev;
   else
      /* we were the tail, so fix up */
      cpu_queue->queue_tail = victim->queue_prev;
   if(victim->queue_prev)
      victim->queue_prev->queue_next = victim->queue_next;
   else
   /* we were the queue head, so fixup pointers */
      cpu_queue->queue_head = victim->queue_next;
   
   /* update accounting - why do these checks after we've
      tried to unlink the thread from a run queue?? */
   if(victim->state == running || victim->state == inrunqueue)
   {
      cpu_table[cpu].queued--;
      sched_dec_queued_threads();
   }

   /* warn another processor that its thread has been removed */
   if(victim->state == running) mp_interrupt_thread(victim, INT_IPI_RESCHED);

   victim->state = state; /* update the state; it might be dying or just blocked */
   
   /* determine the highest priority run queue that's non-empty */
   if(cpu_table[cpu].lowest_queue_filled >= victim->queue->priority)
      sched_rescan_queues(cpu);

   SCHED_DEBUG("[sched:%i] removed thread %i (%p) of process %i from cpu %i queue, priority %i\n",
               CPU_ID, victim->tid, victim, victim->proc->pid, cpu, victim->queue->priority);
   
   victim->queue = NULL; /* thread no longer queued */
   
   unlock_gate(&(victim->lock), LOCK_WRITE);   
   unlock_gate(&(cpu_table[cpu].lock), LOCK_WRITE);
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

   /* the number of threads per cpu queue that's 'fair' to run
      is simply the total number of threads divided by cpu queues 
      available */
   lock_spin(&(sched_total_queued_slock));
   max_fair_share = sched_total_queued / mp_cpus;
   unlock_spin(&(sched_total_queued_slock));
   
   if(!max_fair_share) max_fair_share = 1; /* ensure a sane value */

   /* ensure next_queue is sane */
   lock_spin(&(sched_next_queue_slock));
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
   unlock_spin(&(sched_next_queue_slock));
   
   return picked;
}

/* sched_initialise
   Prepare the default scheduler for action */
void sched_initialise(void)
{
   BOOT_DEBUG("[sched:%i] starting operating system...\n", CPU_ID);

   /* initialise pool of sleeping threads awaiting a clock wake-up */
   sched_bedroom = vmm_create_pool(sizeof(snoozing_thread), 4);
   if(!sched_bedroom)
      debug_panic("unable to create queued pool for clock-held sleeping threads");
   
   /* start running process 1, thread 1 in user mode, which
      should spawn system managers and continue the boot process */
   lowlevel_kickstart();
}

#ifdef SCHED_DEBUG
/* sched_list_queues (aka SCHED_DEBUG_QUEUES)
   Output the state of cpus' run queues via kernel debug */
void sched_list_queues(void)
{
   unsigned int cpu_loop, priority_loop;
   mp_thread_queue *cpu_queue;
   thread *t;
   
   SCHED_DEBUG("[sched:%i] ========================\n", CPU_ID);
   
   for(cpu_loop = 0; cpu_loop < mp_cpus; cpu_loop++)
   {      
      for(priority_loop = 0; priority_loop < SCHED_PRIORITY_LEVELS; priority_loop++)
      {
         cpu_queue = &(cpu_table[cpu_loop].queues[priority_loop]);
         t = cpu_queue->queue_head;
         
         if(t)
         {
            SCHED_DEBUG("[sched:%i] Run queue: cpu %i priority %i (lowest %i)\n",
                        CPU_ID, cpu_loop, priority_loop, cpu_table[cpu_loop].lowest_queue_filled);
            SCHED_DEBUG("[sched:%i]            start[t%i p%i] ",
                        CPU_ID, t->tid, t->proc->pid);
            while(t)
            {
               SCHED_DEBUG("(t%i p%i pri%i gpri%i pts%i) ",
                           t->tid, t->proc->pid, t->priority, t->priority_granted, t->priority_points);
               t = t->queue_next;
            }
            
            if(cpu_queue->queue_tail)
            {
               SCHED_DEBUG("end[t%i p%i]\n", cpu_queue->queue_tail->tid, cpu_queue->queue_tail->proc->pid);
            }
            else 
            {
               SCHED_DEBUG("end[NULL]\n");
            }
         }
      }
      
      if((cpu_loop + 1) < mp_cpus)
      {
         SCHED_DEBUG("[sched:%i] ------------------------\n", CPU_ID);
      }
   }
   SCHED_DEBUG("[sched:%i] ========================\n", CPU_ID);
}
#endif   
