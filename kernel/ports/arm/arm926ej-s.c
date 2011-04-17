/* kernel/ports/arm/arm926ej-s.c
 * ARM-specific processor support for the ARM926EJ-S chip
 * Author : Chris Williams
 * Date   : Sun,12 Mar 2011.03:39.00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* -----------------------------------------------------------
                  Boot the ARM9 processor
 ----------------------------------------------------------- */

/* totals of resources available on this system */
unsigned char mp_cpus = 0;
unsigned char mp_ioapics = 0;
unsigned char mp_boot_cpu = 0;
unsigned char mp_domains = 0;

/* pointer to table of cpu descriptors */
mp_core *cpu_table = NULL;

/* mp_interrupt_process
 Send a given interrupt to all cores (but this one) running the given process */
void mp_interrupt_process(process *proc, unsigned char interrupt)
{
   return;
}

/* mp_interrupt_thread
   Send a given interrupt to a core (not this one) running the given thread */
void mp_interrupt_thread(thread *target, unsigned char interrupt)
{
   return;
}

/* mp_delay
   Cause the processor to pause for a few cycles - the length of a normal
   scheduling quantum */
void mp_delay(void)
{
   return;
}

/* mp_init_ap
   Initialise and start up an application processor
   => id = target CPU's local APIC ID
*/
kresult mp_init_ap(unsigned int id)
{
   return e_notimplemented;
}

/* mp_init_cpu_table
   Initialise the table of present cpus and prepare the slave cpu launch trampoline */
void mp_init_cpu_table(void)
{
   unsigned int cpu_loop, priority_loop;
   
   if(vmm_malloc((void **)&cpu_table, sizeof(mp_core) * mp_cpus))
   {
      MP_DEBUG("[mp:%i] can't allocate cpu table! halting...\n");
      while(1);
   }
   
   /* zero everything */
   vmm_memset((void *)cpu_table, 0, sizeof(mp_core) * mp_cpus);
   
   /* initialise all cpu run queues */
   for(cpu_loop = 0; cpu_loop < mp_cpus; cpu_loop++)
      for(priority_loop = 0; priority_loop < SCHED_PRIORITY_MAX; priority_loop++)
      {
         mp_thread_queue *cpu_queue = &(cpu_table[cpu_loop].queues[priority_loop]);
         cpu_queue->priority = priority_loop;
      }   
   
   MP_DEBUG("[mp:%i] initialised run-time cpu table %p (%i cpus)\n", CPU_ID, cpu_table, mp_cpus);
}

/* mp_post_initialise
   Bring up processors prior to running the first processes */
kresult mp_post_initialise(void)
{
   /* get the processors ready to run */
   mp_init_cpu_table();
   
   return success;
}

/* mp_initialise
   Initialise the boot processor */
kresult mp_initialise(void)
{
   return success;
}




