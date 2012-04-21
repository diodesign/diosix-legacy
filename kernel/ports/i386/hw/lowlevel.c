/* kernel/ports/i386/hw/lowlevel.c
 * perform low-level operations with the i386-compatible processor
 * Author : Chris Williams
 * Date   : Tue,03 Apr 2007.17:19:17

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
#ifndef UNIPROC
   /* x86_test_and_set(1, ptr) will return 0 if we have the lock, or 1
      if someone else has it */
   while(x86_test_and_set(1, spinlock))
   {
      /* loop with normal reads to wait for the lock to appear to be ready, then
         we can try it again - the lock will contain 1 while it's in use - we do
         this to avoid spamming the bus with locked read/writes */
      while(*spinlock)
         /* hint to newer processors that this is a spin-wait loop or
            NOP for older processors - branch predictors go nuts over
            tight loops like this otherwise */
         __asm__ __volatile__("pause");
   }

   /* lock acquired, continue.. */
#endif
}

/* unlock_spin
   Release a lock */
void unlock_spin(volatile unsigned int *spinlock)
{
#ifndef UNIPROC
   /* just slap a zero in it to unlock */
   x86_test_and_set(0, spinlock);
#endif
}

// ------------------------- I/O device support ---------------------------
/* x86_inportb
   Read from an IO port
   => port = port number to read
   <= data recieved
*/
unsigned x86_inportb(unsigned short port)
{
   unsigned char ret_val;

   __asm__ __volatile__("inb %1,%0"
                        : "=a"(ret_val)
                        : "d"(port));
   return ret_val;
}

/* x86_outportb
   Write to an IO port
   => port = port number to access
      val = value to write
*/
void x86_outportb(unsigned port, unsigned val)
{
   __asm__ __volatile__("outb %b0,%w1"
                        :
                        : "a"(val), "d"(port));
}

/* x86_ioports_clear_all
   Remove a process's right to use IO ports
   => p = process to lose access
   <= 0 for success or an error code
*/
kresult x86_ioports_clear_all(process *p)
{
   /* sanity check */
   if(!p) return e_failure;
   
   lock_gate(&(p->lock), LOCK_WRITE);

   /* free the process's IO bitmap */
   if(p->ioport_bitmap)
   {
      vmm_free(p->ioport_bitmap);
      p->ioport_bitmap = NULL;
   }
   
   unlock_gate(&(p->lock), LOCK_WRITE);
   return success;
}

/* x86_ioports_clear
   Set particular bits in a process's IO port bitmap. A set bit 
   indicates no access for the corresponding IO port.
   => p = process to manipulate
      index = 32-bit word number into the bitmap
      value = bits to set in the indexed word
   <= 0 for success or an error code
*/
kresult x86_ioports_clear(process *p, unsigned int index, unsigned int value)
{
   /* sanity check */
   if(!p || !(p->ioport_bitmap) || index > X86_IOPORT_MAXWORDS)
      return e_failure;
   
   lock_gate(&(p->lock), LOCK_WRITE);
   
   p->ioport_bitmap[index] |= value;
   
   unlock_gate(&(p->lock), LOCK_WRITE);
   return success;
}

/* x86_ioports_clone
   Copy a process's IO port bitmap to another
   => target = process to update
      source = process to take the bitmap from
   <= 0 for success or an error code
*/
kresult x86_ioports_clone(process *target, process *source)
{
   /* sanity checks */
   if(!target || !source || !(source->ioport_bitmap))
      return e_failure;
   
   lock_gate(&(target->lock), LOCK_WRITE);
   
   /* allocate an io port bitmap if necessary */
   if(!(target->ioport_bitmap))
   {
      kresult err = vmm_malloc((void **)&(target->ioport_bitmap), X86_IOPORT_BITMAPSIZE);
      if(err)
      {
         unlock_gate(&(target->lock), LOCK_WRITE);
         return err;
      }
   }
   
   /* do the actual cloning */
   lock_gate(&(source->lock), LOCK_READ);
   vmm_memcpy(target->ioport_bitmap, source->ioport_bitmap, X86_IOPORT_BITMAPSIZE);
   
   /* don't forget to release locks */
   unlock_gate(&(source->lock), LOCK_READ);
   unlock_gate(&(target->lock), LOCK_WRITE);
   return success;
}

/* x86_ioports_new
   Create a new IO port bitmap with all bits cleared
   => p = process to create the bitmap for
   <= 0 for success or an error code
*/
kresult x86_ioports_new(process *p)
{
   kresult err;
   
   /* sanity check */
   if(!p || p->ioport_bitmap) return e_failure;
   
   lock_gate(&(p->lock), LOCK_WRITE);

   err = vmm_malloc((void **)&(p->ioport_bitmap), X86_IOPORT_BITMAPSIZE);
   if(err)
   {
      unlock_gate(&(p->lock), LOCK_WRITE);
      return err;
   }
   
   /* clear the bitmap */
   vmm_memset(p->ioport_bitmap, 0, X86_IOPORT_BITMAPSIZE);
   
   unlock_gate(&(p->lock), LOCK_WRITE);
   return success;
}

/* x86_ioports_enable
   Enable a thread in a driver process to begin using IO ports allowed for the process.
   => t = thread to operate on
   <= 0 for success or an error code
*/
kresult x86_ioports_enable(thread *t)
{
   tss_descr *tss;
   
   /* sanity check */
   if(!t) return e_bad_params;

   /* give up if this process isn't allowed to be a driver or if a process
      has given up all its IO port rights */
   if(!(t->proc->flags & PROC_FLAG_CANBEDRIVER)) return e_no_rights;
   if(!(t->proc->ioport_bitmap)) return e_no_rights;
   /* give up if this thread is already enabled */
   if(t->flags & THREAD_FLAG_HASIOBITMAP) return e_failure;
   
   lock_gate(&(t->lock), LOCK_WRITE);
   
   t->flags |= THREAD_FLAG_HASIOBITMAP;
   
   /* recreate this thread's TSS */
   tss = t->tss;
   if(x86_init_tss(t))
   {
      unlock_gate(&(t->lock), LOCK_WRITE);
      return e_failure;
   }
   
   /* tell the CPU we've moved the TSS if need be */
   if(tss != t->tss)
      x86_change_tss(&(cpu_table[CPU_ID].gdtptr),
                     cpu_table[CPU_ID].tssentry, t->tss,
                     THREAD_FLAG_HASIOBITMAP);
   
   unlock_gate(&(t->lock), LOCK_WRITE);
   return success;
}

/* x86_ioports_disable
   Disable a thread's IO bitmap and thus block it from accessing IO ports
   => t = thread to operate on
   <= 0 for success or an error code
*/
kresult x86_ioports_disable(thread *t)
{
   tss_descr *tss;
   
   /* sanity check */
   if(!t) return e_bad_params;
   
   /* give up if this thread wasn't enabled as a driver */
   if(!(t->flags & THREAD_FLAG_HASIOBITMAP)) return e_failure;

   lock_gate(&(t->lock), LOCK_WRITE);
   
   t->flags &= (~THREAD_FLAG_HASIOBITMAP);
   
   /* recreate this thread's TSS */
   tss = t->tss;
   x86_init_tss(t);
   
   /* tell the CPU we've moved the TSS if need be */
   if(tss != t->tss)
      x86_change_tss(&(cpu_table[CPU_ID].gdtptr),
                     cpu_table[CPU_ID].tssentry, t->tss, 0);
   
   unlock_gate(&(t->lock), LOCK_WRITE);
   return success;
}

/* x86_ioports_check
   Check if a process has access to an IO port
   => p = process to check
      port = port number to check for
   <= 0 for access allow, or an error code
*/
kresult x86_ioports_check(process *p, unsigned short port)
{
   /* calculate an index into the ioport table */
   unsigned int index = port >> 5;
   unsigned int offset = port - (index << 5);
   kresult result;
   
   /* sanity check */
   if(!p || !(p->ioport_bitmap) || index > X86_IOPORT_MAXWORDS)
      return e_failure;
   
   lock_gate(&(p->lock), LOCK_WRITE);
   
   /* a set bit in the io port table means no access */
   if(p->ioport_bitmap[index] & (1 << offset))
      result = e_no_rights;
   else
      result = success;
   
   unlock_gate(&(p->lock), LOCK_WRITE);
   return result;

}

// ------------------- performance monitoring support ----------------------
/* x86_read_cyclecount
   <= returns the CPU's current cycle counter value
*/
unsigned long long x86_read_cyclecount(void)
{
   unsigned int high, low;
   
   __asm__ __volatile__("rdtsc"
                        : "=a"(low), "=d"(high));
   
   return ((unsigned long long)low) | (((unsigned long long)high) << 32);
}

// ------------------------- CMOS memory support ---------------------------
/* x86_cmos_write
   Update a byte in the BIOS NVRAM
   => addr = byte number to change
      vlue = new value to write into the CMOS byte
*/
void x86_cmos_write(unsigned char addr, unsigned char value)
{
   x86_outportb(X86_CMOS_ADDR_PORT, addr);
   x86_outportb(X86_CMOS_DATA_PORT, value);   
}

// ------------------------ multitasking support ---------------------------

/* x86_timer_init
   Program the onboard timer to fire freq-times-a-second, or 0 to disable
   Note: this will also alter the PIC mask to enable/disable its PIT's IRQ line
   as required.
*/
void x86_timer_init(unsigned char freq)
{
   unsigned char lo, hi;
   unsigned int div;
   
   if(!freq)
   {
      /* disable the timer by parking it in interrupt on 
         terminal count mode. the chip will wait for a
         value to be loaded into the data register, 
         which it won't be */
      x86_outportb(X86_PIT_COMMAND, 1 << 4);
      
      /* mask out the interrupt line connecting the PIT 
         to the PIC */
      pic_mask_disable(PIC_8254_IRQ_FIXED);
      
      return;
   }
   
   /* calculate the divisor - see http://wiki.osdev.org/Programmable_Interval_Timer
      for more details */
   div = 1193180 / freq;
   
   /* Send byte to command port - generate a square wave pulse
      interrupt at a set rate (mode 3) on channel 0 (wired to IRQ0) */
   x86_outportb(X86_PIT_COMMAND, 0x36);
   
   /* divisor has to be sent byte-wise, lo then hi */
   lo = (unsigned char)(div & 0xff);
   hi = (unsigned char)((div >> 8) & 0xff);
   
   /* send the frequency divisor to channel 0 data port */
   x86_outportb(X86_PIT_CHANNEL0, lo);
   x86_outportb(X86_PIT_CHANNEL0, hi);

   /* ensure the PIT's IRQ line to the PIC is not masked out */
   pic_mask_enable(PIC_8254_IRQ_FIXED);
   
   LOLVL_DEBUG("[x86:%i] programmed PIT to fire interrupt %i times a second\n",
               CPU_ID, freq);
}

/* x86_load_cr3
   Reload the page directory base register with a new physical address
   => ptr = value to move into CR3
*/
void x86_load_cr3(void *ptr)
{
   __asm__ __volatile__("movl %%eax, %%cr3"
                        :
                        : "a"(ptr));
}

/* x86_read_cr2
   <= return the contents of CR2 (faulting address reg)
 */
unsigned int x86_read_cr2(void)
{
   unsigned int ret_val;
   __asm__ __volatile__("movl %%cr2, %%eax"
                        : "=a" (ret_val));
   return ret_val;
}

/* x86_load_cr0
   => val = value to move into CR0 (general processor control register)
*/
void x86_load_cr0(unsigned int val)
{
   __asm__ __volatile__("movl %%eax, %%cr0"
                        :
                        : "a"(val));
}

/* x86_read_cr0
   <= return the contents of CR0 (general processor control register)
*/
unsigned int x86_read_cr0(void)
{
   unsigned int ret_val;
   __asm__ __volatile__("movl %%cr0, %%eax"
                        : "=a" (ret_val));
   return ret_val;
}

/* x86_proc_preinit
   Perform any port-specific pre-initialisation before we start the operating system.
   Assuming microkernel virtual memory model is now active */
void x86_proc_preinit(void)
{
   /* grab a copy of the GDT pointers for the boot cpu */
   cpu_table[CPU_ID].gdtptr.size = ((unsigned int)&KernelGDTEnd - (unsigned int)&KernelGDT) - 1;
   cpu_table[CPU_ID].gdtptr.ptr = (unsigned int)&KernelGDT;
   cpu_table[CPU_ID].tssentry = (gdt_entry *)&TSS_Selector;
   
   BOOT_DEBUG(PORT_BANNER "\n[x86] i386 port initialised, boot processor is %i\n", CPU_ID);
}

/* x86_thread_switch
   Freeze a thread and select another to run
   => now = running thread to stop
      next = thread to reload, or NULL to just preserve the now thread
      regs = pointer to kernel stack
*/
void x86_thread_switch(thread *now, thread *next, int_registers_block *regs)
{
   tss_descr *new_tss;

   /* bail out if invalid pointers */
   if(!now || !regs)
   {
      KOOPS_DEBUG("OMGWTF! x86_thread_switch() called with invalid pointer: now = %p, next = %p, regs = %p\n",
                  now, next, regs);
      debug_panic("x86_thread_switch() cannot continue with a wild pointer");
      /* shouldn't return */
   }
      
#ifdef LOLVL_DEBUG
   if(next)
   {
      new_tss = next->tss;
         
      LOLVL_DEBUG("[x86:%i] switching thread %p for %p (regs %p state %i) (ds/ss %x cs %x ss0 %x esp0 %x)\n",
              CPU_ID, now, next, regs, next->state, new_tss->ss, new_tss->cs, new_tss->ss0, new_tss->esp0);
      LOLVL_DEBUG("[x86:%i] PRE-SWAP: ds %x edi %x esi %x ebp %x esp %x ebx %x edx %x ecx %x eax %x\n"
              "      intnum %x errcode %x eip %x cs %x eflags %x useresp %x ss %x\n",
              CPU_ID, regs->ds, regs->edi, regs->esi, regs->ebp, regs->esp, regs->ebx, regs->edx, regs->ecx, regs->eax,
              regs->intnum, regs->errcode, regs->eip, regs->cs, regs->eflags, regs->useresp, regs->ss);
   }
#endif
   
   /* check to see if we have to preserve the FP context */
   if(KernelFPPresent == X86_FPU_PRESENT)
   {      
      /* check to see if the thread running used FP.
         if TS is cleared then FP was used */
      if(!(x86_read_cr0() & X86_CR0_TS))
         fpu_save_state(now); /* XXX can this fail here? the memory is already allocated */
         
      /* ensure TS is set in cr0 for the next thread. if it uses FP,
         an exception will occur and we'll demand load its FP state */
      x86_load_cr0(x86_read_cr0() | X86_CR0_TS);
   }
   
   /* preserve state of the thread */
   vmm_memcpy(&(now->regs), regs, sizeof(int_registers_block));

   if(next)
   {
      /* load state for new thread */
      vmm_memcpy(regs, &(next->regs), sizeof(int_registers_block));

      LOLVL_DEBUG("[x86:%i] POST-SWAP: ds %x edi %x esi %x ebp %x esp %x ebx %x edx %x ecx %x eax %x\n"
                  "      intnum %x errcode %x eip %x cs %x eflags %x useresp %x ss %x\n",
                  CPU_ID, regs->ds, regs->edi, regs->esi, regs->ebp, regs->esp, regs->ebx, regs->edx, regs->ecx, regs->eax,
                  regs->intnum, regs->errcode, regs->eip, regs->cs, regs->eflags, regs->useresp, regs->ss);
      
      /* reload page directory if we're switching to a new address space */
      if(now->proc->pgdir != next->proc->pgdir)
         x86_load_cr3(KERNEL_LOG2PHYS(next->proc->pgdir));
      
      /* inform the CPU that things have changed */
      new_tss = next->tss;
      new_tss->esp0 = next->kstackbase;
      x86_change_tss(&(cpu_table[CPU_ID].gdtptr),
                     cpu_table[CPU_ID].tssentry, new_tss,
                     next->flags & THREAD_FLAG_HASIOBITMAP);
   }
}

/* x86_init_tss
   Configure a thread's TSS, creating one if need be and 
   ensuring an IO port bitmap is present if required.
   => toinit = thread to initialise a TSS for
   <= 0 for success or an error code if a failure occurred
*/
kresult x86_init_tss(thread *toinit)
{
   tss_descr *tss;
   unsigned int size_req;
   
   /* quick sanity check */
   if(!toinit) return e_bad_params;

   /* free the TSS if one's already allocated */
   if(toinit->tss)
   {
      if(vmm_free(toinit->tss))
      {
         KOOPS_DEBUG("[x86:%i] OMGWTF! x86_init_tss: failed to free previous TSS %p from thread %p (tid %i pid %i)\n",
                     CPU_ID, toinit->tss, toinit, toinit->tid, toinit->proc->pid);
         return e_failure;
      }

      toinit->tss = NULL;
   }

   /* calculate size of TSS, it may need space for an IO bitmap after */
   size_req = sizeof(tss_descr);

   if((toinit->flags & THREAD_FLAG_HASIOBITMAP) && (toinit->proc->ioport_bitmap))
      size_req += X86_IOPORT_BITMAPSIZE;
         
   /* allocate a new TSS */
   if(vmm_malloc((void **)&(toinit->tss), size_req))
   {
      KOOPS_DEBUG("[x86:%i] OMGWTF! x86_init_tss: failed to allocate %i bytes for TSS for thread %p (tid %i pid %i)\n",
                  CPU_ID, size_req, toinit, toinit->tid, toinit->proc->pid);
      return e_failure;
   }

   /* copy in the process's IO map into the TSS if required, the IO
      map will be located at the end of the tss_descr structure */
   if(size_req > sizeof(tss_descr))
      vmm_memcpy((void *)((unsigned int)(toinit->tss) + sizeof(tss_descr)),
                 toinit->proc->ioport_bitmap, X86_IOPORT_BITMAPSIZE);

   tss = toinit->tss;
   vmm_memset(tss, 0, sizeof(tss_descr)); /* let's not forget to zero the TSS */
   
   /* initialise the correct registers */
   /* kernel data seg is 0x10, code seg is 0x18, ORd 0x3 for ring-3 access */
   tss->ss  = tss->ds = tss->es = tss->fs = tss->gs = 0x10 | 0x03;
   tss->cs  = 0x18 | 0x03;
   tss->ss0 = 0x10; /* kernel stack seg (aka data seg) for the IRQ handler */
   tss->esp0 = toinit->kstackbase;
   tss->iomap_base = sizeof(tss_descr);

   LOLVL_DEBUG("[x86:%i] initialised TSS %p size %i for thread %p tid %i pid %i\n",
               CPU_ID, tss, size_req, toinit, toinit->tid, toinit->proc->pid);
   
   return success;
}

/* x86_change_tss
   Tell the processor about a new TSS.
   => cpugdt = pointer to the cpu's GDTR
      gdt = pointer to the TSS entry in the cpu's GDT
      tss = pointer to the new TSS structure
      flags = THREAD_FLAG_HASIOBITMAP: The TSS has an IO bitmap in it.
*/
void x86_change_tss(gdtptr_descr *cpugdt, gdt_entry *gdt, tss_descr *tss, unsigned char flags)
{
   unsigned int base = (unsigned int)tss;
   unsigned int limit = sizeof(tss_descr);
   
   /* sanity checks */
   if(!cpugdt || !gdt || !tss) debug_panic("x86_change_tss() called with a NULL pointer");
   
   /* calculate the correct size of the TSS */
   if(flags & THREAD_FLAG_HASIOBITMAP)
      limit += X86_IOPORT_BITMAPSIZE;
   
   LOLVL_DEBUG("[x86:%i] changing TSS: gdtptr %p (gdt base %x size %i bytes) entry %p tss %p limit %i\n",
               CPU_ID, cpugdt, cpugdt->ptr, cpugdt->size, gdt, tss, limit);
   
   gdt->base_low    = (base & 0xFFFF);
   gdt->base_middle = (base >> 16) & 0xFF;
   gdt->base_high   = (base >> 24) & 0xFF;
   gdt->limit_low   = (limit & 0xFFFF);
   gdt->granularity = ((limit >> 16) & 0x0F) | 0x40; /* 32bit mode */
   gdt->access      = 0x89; /* flags: present, executable, accessed */
   
   /* inform the cpu of changes */
   x86_load_gdtr((unsigned int)cpugdt);
   x86_load_tss();
}

/* x86_warm_kickstart
   Jumpstart the processor with its top queued thread that's already been put into usermode */
void x86_warm_kickstart(void)
{
   thread *next = sched_get_next_to_run(CPU_ID);
   int_registers_block *regs = &(next->regs);
   tss_descr *next_tss = next->tss;
   
   LOLVL_DEBUG("[x86:%i] resuming warm thread %i (%p) of process %i (%p) (regs %p) (ds/ss %x cs %x ss0 %x esp0 %x)\n",
           CPU_ID, next->tid, next, next->proc->pid, next->proc, regs,
           next_tss->ss, next_tss->cs, next_tss->ss0, next_tss->esp0);
   LOLVL_DEBUG("[x86:%i] warm context: ds %x edi %x esi %x ebp %x esp %x ebx %x edx %x ecx %x eax %x\n"
           "      intnum %x errcode %x eip %x cs %x eflags %x useresp %x ss %x\n",
           CPU_ID, regs->ds, regs->edi, regs->esi, regs->ebp, regs->esp, regs->ebx, regs->edx, regs->ecx, regs->eax,
           regs->intnum, regs->errcode, regs->eip, regs->cs, regs->eflags, regs->useresp, regs->ss);

   /* load page directory */
   x86_load_cr3(KERNEL_LOG2PHYS(next->proc->pgdir));
   
   /* inform the CPU that things have changed */   
   next_tss->esp0 = next->kstackbase;
   x86_change_tss(&(cpu_table[CPU_ID].gdtptr),
                  cpu_table[CPU_ID].tssentry, next_tss,
                  next->flags & THREAD_FLAG_HASIOBITMAP);
   
   /* this seems to be the only sensible place to set these state variables */
   next->state = running;
   cpu_table[CPU_ID].current = next;
   
#ifdef LOLVL_DEBUG
   if((regs->useresp <= regs->ebp) && (regs->ebp < KERNEL_SPACE_BASE))
   {
      unsigned int loop;
      LOLVL_DEBUG("[x86:%i] inspecting thread's stack: usresp %x ebp %x...\n",
                  CPU_ID, regs->useresp, regs->ebp);
      for(loop = regs->ebp + 4; loop >= regs->useresp; loop -= sizeof(unsigned int))
         LOLVL_DEBUG("      %x : %x\n", loop, *((volatile unsigned int *)loop));
   }
#endif
   
   /* prepare FP support if needed */
   if(KernelFPPresent == X86_FPU_PRESENT)
   /* ensure TS is set in cr0 for the next thread. if it uses FP,
    an exception will occur and we'll demand load its FP state */
      x86_load_cr0(x86_read_cr0() | X86_CR0_TS);
   
   /* now return to the usermode thread - all the registers are stacked up in the 
      thread's reg block - see int_handler in locore.s for this return code */
   regs->eflags |= 0x200; /* iret must reenable interrupts so force the flag */
   __asm__ __volatile__
   ("movl %0, %%esp; \
     pop %%eax; \
     mov %%ax, %%ds; \
     mov %%ax, %%es; \
     mov %%ax, %%fs; \
     mov %%ax, %%gs; \
     popa; \
     addl $8, %%esp; \
     iret;" : : "a" (regs));
   
   /* shouldn't reach here.. */
}

/* x86_kickstart
   Jumpstart the processor by executing the given thread in usermode. If torun is NULL, then
   execute the top queued thread in usermode */
void x86_kickstart(thread *torun)
{
   process *proc;
   unsigned int usresp;
   
   /* loop waiting for a thread to become ready to run */
   while(!torun)
      torun = sched_get_next_to_run(CPU_ID);
   
   /* has this thread already entered usermode? */
   if(torun->flags & THREAD_FLAG_INUSERMODE)
   {
      x86_warm_kickstart();
      return;
   }
   
   proc = torun->proc;
   
   LOLVL_DEBUG("[x86:%i] kickstarting cold thread %i (%p) of process %i (%p) stackbase %x kstackbase %x at EIP %x\n",
           CPU_ID, torun->tid, torun, proc->pid, proc, torun->stackbase, torun->kstackbase, proc->entry);
   
   /* get page tables loaded, TSS initialised and GDT updated */
   x86_load_cr3(KERNEL_LOG2PHYS(proc->pgdir));
   x86_init_tss(torun);
   x86_change_tss(&(cpu_table[CPU_ID].gdtptr),
                  cpu_table[CPU_ID].tssentry, torun->tss,
                  torun->flags & THREAD_FLAG_HASIOBITMAP);
   
   /* keep the scheduler happy */
   torun->state = running;
   torun->timeslice = SCHED_TIMESLICE;
   cpu_table[CPU_ID].current = torun;
   torun->flags |= THREAD_FLAG_INUSERMODE; /* well, we're about to be.. */
   
   /* prepare FP support if needed */
   if(KernelFPPresent == X86_FPU_PRESENT)
      /* ensure TS is set in cr0 for the next thread. if it uses FP,
      an exception will occur and we'll demand load its FP state */
      x86_load_cr0(x86_read_cr0() | X86_CR0_TS);
   
   /* assume we're going to kickstart an i386-elf, so prepare the stack pointer appropriately.
      the page holding the stack will have been zero'd, so we just move the ptr down a few words
      to keep the C-front end of the kick-started program from accessing stack that's not there */
   usresp = torun->stackbase - (4 * (sizeof(unsigned int)));
   /* see: http://asm.sourceforge.net/articles/startup.html for minimal stack layout */

   /* x86 voodoo to switch to user mode and enable interrupts (eflags OR 0x200 in the code) */
   /* segment number 0x23 = 0x20 | 0x3 = user data seg 0x20 with ring3 bits set
                     0x2B = 0x28 | 0x3 = user code seg 0x28 with ring3 bits set */
   /* also: pass new stack pointer and jump to the thread's entry point */
   __asm__ __volatile__
   ("movl $0x23, %%eax; \
     movl %%eax, %%ds; \
     movl %%eax, %%es; \
     movl %%eax, %%fs; \
     movl %%eax, %%gs; \
     movl %0, %%eax; \
     pushl $0x23; \
     pushl %%eax; \
     pushf; \
     popl %%eax; \
     or $0x200, %%eax; \
     pushl %%eax; \
     pushl $0x2B; \
     movl %1, %%eax; \
     pushl %%eax; \
     iret;" : : "b" (usresp), "c" (proc->entry));
   
   /* execution shouldn't really return to here */
   KOOPS_DEBUG("[x86:%i] x86_kickstart: thread %p (tid %i pid %i) failed to kickstart\n",
               CPU_ID, torun, torun->tid, torun->proc->pid);
}

/* x86_enable_interrupts and x86_disable_interrupts
 These functions enable and disable interrupt handling.
 Pending interrupts remain pending while handling is
 disabled */
void x86_enable_interrupts(void)
{
   __asm__ __volatile__("sti");
}
void x86_disable_interrupts(void)
{
   __asm__ __volatile__("cli");
}


/* generic veneers */
void lowlevel_thread_switch(thread *now, thread *next, int_registers_block *regs)
{
#ifdef LOCK_SANITY_CHECK
   /* thread+owner process locks should be released prior to switching tasks to avoid deadlocks */
   lock_spin(&(now->lock.spinlock));
   if(now->lock.owner && now->state != dead)
   {
      thread *o = (thread *)(now->lock.owner);
      KOOPS_DEBUG("[x86:%i] switching from thread %p (tid %i pid %i) with lock %p still engaged!\n",
              CPU_ID, now, now->tid, now->proc->pid, &(now->lock));
      KOOPS_DEBUG("        lock owner is %p (tid %i pid %i)\n", o, o->tid, o->proc->pid);
      KOOPS_DEBUG(" *** halting.\n");
      while(1);
   }
   unlock_spin(&(now->lock.spinlock));
   
   lock_spin(&(now->proc->lock.spinlock));
   if(now->proc->lock.owner)
   {
      thread *o = (thread *)(now->proc->lock.owner);
      KOOPS_DEBUG("[x86:%i] switching from process %p (pid %i) with lock %p still engaged!\n",
              CPU_ID, now->proc, now->proc->pid, &(now->proc->lock));
      KOOPS_DEBUG("        lock owner is %p (tid %i pid %i)\n", o, o->tid, o->proc->pid);
      KOOPS_DEBUG(" *** halting.\n");
      while(1);
   }
   unlock_spin(&(now->proc->lock.spinlock));
   
   /* make sure the IRQ lock isn't going to be held across thread switches */
   lock_spin(&(irq_lock.spinlock));
   if(irq_lock.owner == (unsigned int)now && (unsigned int)now != (unsigned int)next)
   {
      KOOPS_DEBUG("[x86:%i] OMGWTF! lowlevel_thread_switch: thread %i of process %i (%x -> %x) holds IRQ lock!\n",
                  CPU_ID, now->tid, now->proc->pid, now, next);
   }
   unlock_spin(&(irq_lock.spinlock));
#endif
   
   /* if the thread is already in usermode then switch to it */
   if(next->flags & THREAD_FLAG_INUSERMODE)
   {
      LOLVL_DEBUG("[x86:%i] switching to warm thread %i process %i (%p) from thread %i process %i (%p)\n",
                   CPU_ID, next->tid, next->proc->pid, next, now->tid, now->proc->pid, now);
      x86_thread_switch(now, next, regs);
   }
   else
   {
      LOLVL_DEBUG("[x86:%i] switching to cold thread %i process %i (%p) from thread %i process %i (%p)\n",
                  CPU_ID, next->tid, next->proc->pid, next, now->tid, now->proc->pid, now);
      
      /* suspend the currently running thread */
      x86_thread_switch(now, NULL, regs);
      
      /* and kickstart the next one into userspace */
      x86_kickstart(next);
   }
}

/* perform a stack trace */
void lowlevel_stacktrace(void)
{
   unsigned int *ptr, *base, sym_base;
   unsigned int tid, pid;
   char buffer[32];

   /* first, work out where the stack starts */
   if(cpu_table && cpu_table[CPU_ID].current)
   {
      /* we're running threads, so find the current thread's kernel
       stack base */
      base = (unsigned int *)cpu_table[CPU_ID].current->tss->esp0;
      tid = cpu_table[CPU_ID].current->tid;
      pid = cpu_table[CPU_ID].current->proc->pid;
   }
   else
   {
      /* kernel is still booting and using the initial stack */
      base = (unsigned int *)&(KernelBootStackBase);
      tid = pid = 0;
   }

   /* grab a copy of the stack pointer */
   __asm__ __volatile__("movl %%esp, %0" : "=a" (ptr));

   dprintf("[debug:%i] kernel stack backtrace requested: base %p stackptr %p size %i (tid %i pid %i)\n",
           CPU_ID, base, ptr, ((unsigned int)base - (unsigned int)ptr), tid, pid);

   while(base >= ptr)
   {
      /* look up a kernel symbol if relevant */
      if(*(base) >= KERNEL_VIRTUAL_BASE)
         {
         if(debug_lookup_symbol(*(base), buffer, 32, &sym_base) == success)
            {
            dprintf("        [0x%p]  0x%x [func %s + 0x%x]\n",
                    base, *(base), buffer, (*base) - sym_base);
            }
         else
            {
            dprintf("        [0x%p]  0x%x\n", base, *(base));
            }
         }
      base--;
   }
   dprintf("\n");
}

void lowlevel_proc_preinit(void)
{
   x86_proc_preinit();
}

void lowlevel_kickstart(void)
{   
   x86_kickstart(NULL);
}

void lowlevel_ioports_clone(process *new, process *current)
{
   x86_ioports_clone(new, current);
}

void lowlevel_ioports_new(process *new)
{
   x86_ioports_new(new);
}