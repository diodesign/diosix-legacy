/* kernel/ports/arm/lowlevel.c
 * perform low-level operations with the ARM port
 * Author : Chris Williams
 * Date   : Sat,12 Mar 2011.22:28:00

Copyright (c) Chris Williams and individual contributors
 
PIC timer code cribbed from: http://wiki.osdev.org/PIC and http://wiki.osdev.org/PIT#Channel_0

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

#ifdef LOCK_TIME_CHECK
volatile unsigned int lock_time_check_lock = 0;
#endif

// --------------------- atomic locking support ---------------------------

/* lock_spin
   Block until we've acquired the lock */
void lock_spin(volatile unsigned int *spinlock)
{      
}

/* unlock_spin
   Release a lock */
void unlock_spin(volatile unsigned int *spinlock)
{
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

/* generic veneers */
void lowlevel_proc_preinit(void)
{
}

void lowlevel_kickstart(void)
{
}

void lowlevel_stacktrace(void)
{
}

void lowlevel_disable_interrupts(void)
{
}

void lowlevel_enable_interrupts(void)
{
}
