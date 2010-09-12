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
#ifndef UNIPROC
void unlock_spin(volatile unsigned int *spinlock)
{
   /* just slap a zero in it to unlock */
   *spinlock = 0;
}
#endif

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

#ifdef LOCK_TIME_CHECK
   unsigned long long ticks = x86_read_cyclecount();
#endif
   
   LOCK_DEBUG("[lock:%i] -> lock_gate(%p, %x) on cpu %i\n", CPU_ID, gate, flags, CPU_ID);
      
   /* sanity checks */
   if(!gate) return e_failure;
   if(!cpu_table) return success; /* only one processor running */
   
   while(1)
   {
      lock_spin(&(gate->spinlock));
      
      /* we're in - is the gate claimed? */
      if(gate->owner)
      {
         /* it's in use - but is it another thread? */
         if(gate->owner != (unsigned int)cpu_table[CPU_ID].current)
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
         gate->owner = (unsigned int)cpu_table[CPU_ID].current;
         gate->flags = flags;
         gate->refcount = 1; /* first in */
         goto exit_lock_gate;
      }
      
      unlock_spin(&(gate->spinlock));
      
      /* small window of opportunity for the other thread to
         release the gate :-/ */
      /* hint to newer processors that this is a spin-wait loop or
       NOP for older processors */
      __asm__ __volatile__("pause");
      
#ifdef LOCK_TIME_CHECK
      if((x86_read_cyclecount() - ticks) > LOCK_TIMEOUT)
      {
         KOOPS_DEBUG("[lock:%i] OMGWTF waited too long for gate %p to become available (flags %x)\n"
                     "         lock is owned by %p", CPU_ID, gate, flags, gate->owner);
         if(gate->owner)
         {
            thread *t = (thread *)(gate->owner);
            KOOPS_DEBUG(" (thread %i process %i)", t->tid, t->proc->pid);
         }
         KOOPS_DEBUG("\n");
         debug_stacktrace();
         err = e_failure;
         goto exit_lock_gate;
      }
#endif
   }

exit_lock_gate:
   /* release the gate so others can inspect it */
   unlock_spin(&(gate->spinlock));
   
   LOCK_DEBUG("[lock:%i] locked %p with %x on cpu %i\n", CPU_ID, gate, flags, 0);
   
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
   LOCK_DEBUG("[lock:%i] unlock_gate(%p, %x) on cpu %i\n", CPU_ID, gate, flags, 0);
   
   /* sanity checks */
   if(!gate) return e_failure;
   if(!cpu_table) return success; /* only one processor running */   
   
   lock_spin(&(gate->spinlock));
   
   /* if this is our gate then unset details */
   if(gate->owner == (unsigned int)cpu_table[CPU_ID].current)
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
   Program the onboard timer to fire freq-times-a-second, or 0 to disable */
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
      x86_outportb(0x43, 1 << 4);
      return;
   }
   
   /* calculate the divisor - see http://wiki.osdev.org/Programmable_Interval_Timer
      for more details */
   div = 1193180 / freq;
   
   /* Send byte to command port - generate a square wave pulse
      interrupt at a set rate (mode 3) on channel 0 (wired to IRQ0) */
   x86_outportb(0x43, 0x36);
   
   /* divisor has to be sent byte-wise, lo then hi */
   lo = (unsigned char)(div & 0xff);
   hi = (unsigned char)((div >> 8) & 0xff);
   
   /* send the frequency divisor to channel 0 data port */
   x86_outportb(0x40, lo);
   x86_outportb(0x40, hi);
}

/* x86_load_cr3
   Reload the page directory base register with a new physical address
   => ptr = value to move into cr3
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

/* x86_proc_preinit
 Perform any port-specific pre-initialisation before we start the operating system.
 Assuming microkernel virtual memory model is now active */
void x86_proc_preinit(void)
{
   /* grab a copy of the GDT pointers for the boot cpu */
   cpu_table[CPU_ID].gdtptr.size = ((unsigned int)&KernelGDTEnd - (unsigned int)&KernelGDT) - 1;
   cpu_table[CPU_ID].gdtptr.ptr = (unsigned int)&KernelGDT;
   cpu_table[CPU_ID].tssentry = (gdt_entry *)&TSS_Selector;
   
   BOOT_DEBUG(PORT_BANNER "\n[x86] i386 port initialised\n");
}

/* x86_thread_switch
   Freeze a thread and select another to run
   => now = running thread to stop, or NULL for no thread
      next = thread to reload
      regs = pointer to kernel stack
*/
void x86_thread_switch(thread *now, thread *next, int_registers_block *regs)
{
   tss_descr *new_tss = (tss_descr *)&(next->tss);
   
   LOLVL_DEBUG("[x86:%i] switching thread %p for %p (regs %p) (ds/ss %x cs %x ss0 %x esp0 %x)\n",
           CPU_ID, now, next, regs, new_tss->ss, new_tss->cs, new_tss->ss0, new_tss->esp0);
   LOLVL_DEBUG("[x86:%i] PRE-SWAP: ds %x edi %x esi %x ebp %x esp %x ebx %x edx %x ecx %x eax %x\n"
           "      intnum %x errcode %x eip %x cs %x eflags %x useresp %x ss %x\n",
           CPU_ID, regs->ds, regs->edi, regs->esi, regs->ebp, regs->esp, regs->ebx, regs->edx, regs->ecx, regs->eax,
           regs->intnum, regs->errcode, regs->eip, regs->cs, regs->eflags, regs->useresp, regs->ss);
   
   /* preserve state of the thread */
   vmm_memcpy(&(now->regs), regs, sizeof(int_registers_block));
   
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
   new_tss->esp0 = next->kstackbase;
   x86_change_tss(&(cpu_table[CPU_ID].gdtptr), cpu_table[CPU_ID].tssentry, new_tss);
}

/* x86_init_tss
   Configure a thread's TSS for the first (and only) time */
void x86_init_tss(thread *toinit)
{
   tss_descr *tss = (tss_descr *)&(toinit->tss);
   
   /* initialise the correct registers */
   /* kernel data seg is 0x10, code seg is 0x18, ORd 0x3 for ring-3 access */
   tss->ss  = tss->ds = tss->es = tss->fs = tss->gs = 0x10 | 0x03;
   tss->cs  = 0x18 | 0x03;
   tss->ss0 = 0x10; /* kernel stack seg (aka data seg) for the IRQ handler */
   tss->esp0 = toinit->kstackbase;
}

/* x86_warm_kickstart
   Jumpstart the processor with its top queued thread that's already been put into usermode */
void x86_warm_kickstart(void)
{
   thread *next = cpu_table[CPU_ID].queue_head;
   int_registers_block *regs = &(next->regs);
   tss_descr *next_tss = (tss_descr *)&(next->tss);
   
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
   x86_change_tss(&(cpu_table[CPU_ID].gdtptr), cpu_table[CPU_ID].tssentry, next_tss);

   /* now return to the usermode thread - all the registers are stacked up in the 
      thread's reg block - see int_handler in locore.s for this return code */
   __asm__ __volatile__
   ("movl %0, %%esp; \
     pop %%eax; \
     mov %%ax, %%ds; \
     mov %%ax, %%es; \
     mov %%ax, %%fs; \
     mov %%ax, %%gs; \
     popa; \
     addl $8, %%esp; \
     sti; \
     iret;" : : "a" (regs));
   
   /* shouldn't reach here.. */
}

/* x86_kickstart
   Jumpstart the processor by executing the top queued thread in usermode  */
void x86_kickstart(void)
{
   thread *torun = cpu_table[CPU_ID].queue_head;
   process *proc;
   tss_descr *tss;
   unsigned int usresp;
   
   if(!torun)
   {
      KOOPS_DEBUG("[x86:%i] OMGWTF: cannot find a thread to kickstart.\n", CPU_ID);
      return;
   }
   
   /* has this thread already entered usermode? */
   if(torun->flags & THREAD_INUSERMODE)
   {
      x86_warm_kickstart();
      return;
   }
   
   tss = (tss_descr *)&(torun->tss);
   proc = torun->proc;
   
   LOLVL_DEBUG("[x86:%i] kickstarting cold thread %i (%p) of process %i (%p) stackbase %x kstackbase %x tss %x at EIP %x\n",
           CPU_ID, torun->tid, torun, proc->pid, proc, torun->stackbase, torun->kstackbase, &(torun->tss), proc->entry);
   
   /* get page tables loaded, TSS initialised and GDT updated */
   x86_load_cr3(KERNEL_LOG2PHYS(proc->pgdir));
   x86_init_tss(torun);
   x86_change_tss(&(cpu_table[CPU_ID].gdtptr), cpu_table[CPU_ID].tssentry, tss);
   
   /* keep the scheduler happy */
   torun->state = running;
   torun->timeslice = SCHED_TIMESLICE;
   cpu_table[CPU_ID].current = torun;
   torun->flags = THREAD_INUSERMODE; /* well, we're about to be.. */
   
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
}

/* x86_change_tss
   Update the given tss and point to it from the given gdt entry */
void x86_change_tss(gdtptr_descr *cpugdt, gdt_entry *gdt, tss_descr *tss)
{
   unsigned int base = (unsigned int)tss;
   unsigned int limit = sizeof(tss_descr);
   
   LOLVL_DEBUG("[x86:%i] changing TSS: gdtptr %p (gdt base %x size %i bytes) entry %p tss %p\n",
           CPU_ID, cpugdt, cpugdt->ptr, cpugdt->size, gdt, tss);
   
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

// ----------------------- interrupt management ----------------------------
/* typical x86 PC values for the two basic PIC chips */
#define PIC1               (0x20)   /* IO base address for master PIC */
#define PIC2               (0xA0)   /* IO base address for slave PIC */
#define PIC1_COMMAND         (PIC1)
#define PIC1_DATA            (PIC1+1)
#define PIC2_COMMAND         (PIC2)
#define PIC2_DATA            (PIC2+1)

#define ICW1_ICW4            (0x01)   /* ICW4 (not) needed */
#define ICW1_SINGLE         (0x02)   /* Single (cascade) mode */
#define ICW1_INTERVAL4      (0x04)   /* Call address interval 4 (8) */
#define ICW1_LEVEL         (0x08)   /* Level triggered (edge) mode */
#define ICW1_INIT            (0x10)   /* Initialization - required! */
   
#define ICW4_8086            (0x01)   /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO            (0x02)   /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE      (0x08)   /* Buffered mode/slave */
#define ICW4_BUF_MASTER      (0x0C)   /* Buffered mode/master */
#define ICW4_SFNM            (0x10)   /* Special fully nested (not) */

/* x86_pic_remap
   Reinitialise the two motherboard PIC chips to reprogram basic
   interrupt routing to the kernel using an offset.
   => offset1 = vector offset for master PIC
                vectors on the master become offset1 to offset1 + 7
   => offset2 = same for slave PIC: offset2 to offset2 + 7
*/
void x86_pic_remap(unsigned int offset1, unsigned int offset2)
{
   unsigned char a1, a2;
   
   /* save the masks */
   a1 = x86_inportb(PIC1_DATA);
   a2 = x86_inportb(PIC2_DATA);
   
   /* reinitialise the chipset */
   x86_outportb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);
   x86_outportb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);
   /* send the new offsets */
   x86_outportb(PIC1_DATA, offset1);
   x86_outportb(PIC2_DATA, offset2);
   /* complete the reinitialisation sequence */
   x86_outportb(PIC1_DATA, 4);
   x86_outportb(PIC2_DATA, 2);
   x86_outportb(PIC1_DATA, ICW4_8086);
   x86_outportb(PIC2_DATA, ICW4_8086);
   
   /* restore saved masks */
   x86_outportb(PIC1_DATA, a1);
   x86_outportb(PIC2_DATA, a2);
}

/* x86_pic_reset
   Send an end-of-interrupt/reset signal to a PIC
   => pic = 1 for master
            2 for slave
*/
void x86_pic_reset(unsigned char pic)
{
   if(pic == 1)
      x86_outportb(PIC1_COMMAND, 0x20);
   else
      x86_outportb(PIC2_COMMAND, 0x20);
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
   if(now->lock.owner)
   {
      thread *o = (thread *)(now->lock.owner);
      LOLVL_DEBUG("[x86:%i] switching from thread %p (tid %i pid %i) with lock %p still engaged!\n",
              CPU_ID, now, now->tid, now->proc->pid, &(now->lock));
      LOLVL_DEBUG("        lock owner is %p (tid %i pid %i)\n", o, o->tid, o->proc->pid);
      LOLVL_DEBUG(" *** halting.\n");
      while(1);
   }
   unlock_spin(&(now->lock.spinlock));
   
   lock_spin(&(now->proc->lock.spinlock));
   if(now->proc->lock.owner)
   {
      thread *o = (thread *)(now->proc->lock.owner);
      LOLVL_DEBUG("[x86:%i] switching from process %p (pid %i) with lock %p still engaged!\n",
              CPU_ID, now->proc, now->proc->pid, &(now->proc->lock));
      LOLVL_DEBUG("        lock owner is %p (tid %i pid %i)\n", o, o->tid, o->proc->pid);
      LOLVL_DEBUG(" *** halting.\n");
      while(1);
   }
   unlock_spin(&(now->proc->lock.spinlock));
#endif
   
   x86_thread_switch(now, next, regs);
}

void lowlevel_proc_preinit(void)
{
   x86_proc_preinit();
}

void lowlevel_kickstart(void)
{
   x86_kickstart();
}
