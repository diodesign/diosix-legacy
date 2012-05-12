/* kernel/ports/arm/lowlevel.c
 * perform low-level operations with the ARM port
 * Author : Chris Williams
 * Date   : Sat,12 Mar 2011.22:28:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

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

// ---------------------------- generic veneers ---------------------------

void lowlevel_thread_switch(thread *now, thread *next, int_registers_block *regs)
{
   KOOPS_DEBUG("lowlevel_thread_switch(%p, %p, %p): not yet implemented\n",
               now, next, regs);
}

void lowlevel_proc_preinit(void)
{
   /* call the second-half initialisation of the paging system */
   pg_post_init((unsigned int *)KernelPageDirectory);
   
   /* nothing to do except announce that the kernel is ready to roll */
   BOOT_DEBUG(PORT_BANNER "\n[arm] ARMv5-compatible port initialised, boot processor is %i\n", CPU_ID);
}

void lowlevel_kickstart(void)
{
   KOOPS_DEBUG("lowlevel_kickstart: not yet implemented\n");
}

void lowlevel_stacktrace(void)
{
   KOOPS_DEBUG("lowlevel_stacktrace: not yet implemented\n");
}

void lowlevel_disable_interrupts(void)
{
   KOOPS_DEBUG("lowlevel_disable_interrupts: not yet implemented\n");
}

void lowlevel_enable_interrupts(void)
{
   KOOPS_DEBUG("lowlevel_enable_interrupts: not yet implemented\n");
}
