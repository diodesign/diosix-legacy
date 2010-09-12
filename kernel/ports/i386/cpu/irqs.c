/* kernel/ports/i386/cpu/irqs.c
 * i386-specific hardware interrupt request handling
 * Author : Chris Williams
 * Date   : Tues,13 Oct 2009.23:23:00

Cribbed from... http://www.jamesmolloy.co.uk/tutorial_html/
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* protect the IRQ data structures */
rw_gate irq_lock;

/* array of per-IRQ line handlers */
irq_driver_entry *irq_drivers[IRQ_MAX_LINES];

/* -------------------- DEVICE INTERRUPT DISPATCH ---------------------- */
/* irq_handler
 Master interrupt handler -- investigate and delegate
 => r = pointer to stacked registers
 */
void irq_handler(int_registers_block *regs)
{   
#ifdef PERFORMANCE_DEBUG
   unsigned long long debug_cycles = x86_read_cyclecount();
#endif

   irq_driver_entry *driver;
   
   /* make sure we only consider the low byte, which contains the irq number */
   regs->intnum = regs->intnum % IRQ_MAX_LINES;

   IRQ_DEBUG("[irq:%i] processing IRQ %i (registers at %p)\n", CPU_ID, regs->intnum, regs);

   lock_gate(&irq_lock, LOCK_READ);
   
   /* find the registered drivers */
   driver = irq_drivers[regs->intnum];
   while(driver)
   {
      switch(driver->flags & IRQ_DRIVER_TYPEMASK)
      {
         case IRQ_DRIVER_FUNCTION:
            /* call the kernel function */
            (driver->func)(regs->intnum, regs);
            break;
            
         case IRQ_DRIVER_PROCESS:
         {
            /* poke the driver process */
            process *target = proc_find_proc(driver->pid);
            if(target)
               msg_send_signal(target, SIGXIRQ, regs->intnum);
         }
         break;
      }
      
      driver = driver->next;
   }
      
   unlock_gate(&irq_lock, LOCK_READ);
   
   PERFORMANCE_DEBUG("[irq:%i] hardware interrupt %x took about %i cycles to process\n",
                     CPU_ID, regs->intnum, (unsigned int)(x86_read_cyclecount() - debug_cycles));
}
/* ----------------------------------------------------------------------- */

/* irq_find_driver
   Locate a registered driver's entry structure 
   => irq_num = IRQ line to search
      type    = required driver type (see irq_register_driver)
      pid     = process id to match (or 0 for n/a)
      func    = in-kernel function to match (or NULL for n/a)
   <= pointer to structure or NULL for no match
*/
irq_driver_entry *irq_find_driver(unsigned int irq_num, unsigned int type,
                                  unsigned int pid,
                                  kresult (*func)(unsigned char intnum, int_registers_block *regs))
{
   irq_driver_entry *search;
   
   /* bail out on a wild irq_num */
   if(irq_num >= IRQ_MAX_LINES) return NULL;
   
   /* protect the data structure during read */
   lock_gate(&irq_lock, LOCK_READ);
   
   search = irq_drivers[irq_num];
   while(search)
   {
      switch(search->flags & IRQ_DRIVER_TYPEMASK)
      {
         case IRQ_DRIVER_FUNCTION:
            if(search->func == func) goto irq_driver_entry_exit;
            break;
            
         case IRQ_DRIVER_PROCESS:
            if(search->pid == pid) goto irq_driver_entry_exit;
            break;
      }
      
      /* the search continues */
      search = search->next;
   }
   
irq_driver_entry_exit:
   unlock_gate(&irq_lock, LOCK_READ);
   return search;
}

/* irq_deregister_driver
    Remove a driver from the given IRQ line
    => irq_num  = IRQ line to deattach the driver from
       type     = define the driver type and any other flags, types are:
                  IRQ_DRIVER_THREAD, IRQ_DRIVER_PROCESS, IRQ_DRIVER_FUNCTION
       pid      = process id to match (or 0 for none)
       func     = in-kernel function to match, or NULL for none
    <= 0 for success or else an error code 
*/
kresult irq_deregister_driver(unsigned int irq_num, unsigned int type,
                              unsigned int pid, 
                              kresult (*func)(unsigned char intnum, int_registers_block *regs))
{
   irq_driver_entry *victim;
   
   /* bail out on a wild irq_num */
   if(irq_num >= IRQ_MAX_LINES) return e_bad_params;
   
   /* locate the victim and bail out if we can't find what we're looking for */
   victim = irq_find_driver(irq_num, type, pid, func);
   if(!victim) return e_bad_params;
   
   lock_gate(&irq_lock, LOCK_WRITE);
   
   /* remove it from the table */
   if(victim->next)
      victim->next->previous = victim->previous;
   if(victim->previous)
      victim->previous->next = victim->next;
   else
      /* we were the irq line head, so fix up */
      irq_drivers[irq_num] = victim->next;

   vmm_free(victim);
   
   unlock_gate(&irq_lock, LOCK_WRITE);
   
   IRQ_DEBUG("[irq:%i] deregistered driver %p from IRQ %i\n", CPU_ID,
             victim, irq_num);
   
   return success;
}

/* irq_register_driver
   Register a driver with the given IRQ line
   => irq_num  = IRQ line to attach this driver to
      flags    = define the driver type and any other flags, types are:
                 IRQ_DRIVER_PROCESS, IRQ_DRIVER_FUNCTION
      pid      = process id to send the irq signal to (or 0 for none)
      func     = in-kernel function to call, or NULL for none
   <= 0 for success or else an error code 
*/
kresult irq_register_driver(unsigned int irq_num, unsigned int flags,
                            unsigned int pid,
                            kresult (*func)(unsigned char intnum, int_registers_block *regs))
{
   kresult err;
   irq_driver_entry *new, *head;
   
   /* bail out on a wild irq_num */
   if(irq_num >= IRQ_MAX_LINES) return e_bad_params;
   
   /* allocate memory for the new driver entry and zero it */
   err = vmm_malloc((void **)&new, sizeof(irq_driver_entry));
   if(err) return err;
   vmm_memset((void *)&new, 0, sizeof(irq_driver_entry));
   
   /* fill in the blanks */
   if(flags & IRQ_DRIVER_FUNCTION)
      new->func = func; /* driver is a kernel function */
   else
      new->pid = pid; /* driver is a userspace process */
   
   /* protect the data structure during update */
   lock_gate(&irq_lock, LOCK_WRITE);
   
   /* attach the driver entry to the head of the list for the chosen IRQ line */
   head = irq_drivers[irq_num];
   if(head)
   {
      /* update the old head */
      new->next = head;
      head->previous = new;
   }
   
   irq_drivers[irq_num] = new;
   
   unlock_gate(&irq_lock, LOCK_WRITE);
   
   IRQ_DEBUG("[irq:%i] registered driver %p to IRQ %i (pid %i func %p flags %x)\n", CPU_ID,
             new, irq_num, pid, func, flags);
   
   return success;
}

/* prepare the IRQ handling system */
void irq_initialise(void)
{
   /* zero the table of IRQ pointers and the lock */
   vmm_memset((void *)&irq_drivers, 0, sizeof(irq_driver_entry *) * IRQ_MAX_LINES);
   vmm_memset((void *)&irq_lock, 0, sizeof(rw_gate));
}
