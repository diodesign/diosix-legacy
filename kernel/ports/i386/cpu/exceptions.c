/* kernel/ports/i386/cpu/exceptions.c
 * i386-specific program interrupt handling - exceptions and SWIs
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

/* link C code to asm code in start.s via these externs */
/* CPU exceptions */
extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

/* inter-processor interrupts (142-143) - sent to warn other cores of changes */
extern void isr142(); extern void isr143();

/* diosix native software interrupt - 0x90 (144) set by KERNEL_SWI_BASE */
extern void isr144();


/* ---------------------- CPU EXCEPTION / SWI DISPATCH ----------------------- */
/* exception_handler
   Master exception + SWI handler -- investigate and delegate
   => regs = pointer to stacked registers
*/
void exception_handler(int_registers_block regs)
{
#ifdef PERFORMANCE_DEBUG
   unsigned long long debug_cycles = x86_read_cyclecount();
#endif

   XPT_DEBUG("[xpt:%i] IN: ds %x edi %x esi %x ebp %x esp %x ebx %x edx %x ecx %x eax %x\n"
             "        intnum %x errcode %x eip %x cs %x eflags %x useresp %x ss %x\n",
            CPU_ID, regs.ds, regs.edi, regs.esi, regs.ebp, regs.esp, regs.ebx, regs.edx, regs.ecx, regs.eax,
            regs.intnum & 0xff, regs.errcode, regs.eip, regs.cs, regs.eflags, regs.useresp, regs.ss);
   
   regs.intnum &= 0xff; /* just interested in the low byte */
   
   switch(regs.intnum)
   {
      /* cpu exceptions */
      case INT_DOUBLEF: /* DOUBLE FAULT */
         XPT_DEBUG("[xpt:%i] DOUBLE FAULT: code %i (%x)\n",
                   CPU_ID, regs.errcode, regs.errcode & ((1 << 16) - 1));

         proc_kill(cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc);
         break;
         
      case INT_GPF: /* GENERAL PROTECTION FAULT */
         if(regs.eip > KERNEL_SPACE_BASE)
         {
            XPT_DEBUG("[xpt:%i] GPF: code %i (0x%x) eip %x\n"
                      "        ds %x edi %x ebp %x esp %x\n"
                      "        eax %x cs %x eflags %x useresp %x ss %x\n",
                      CPU_ID, regs.errcode, regs.errcode, regs.eip,
                      regs.ds, regs.edi, regs.ebp, regs.esp,
                      regs.eax, regs.cs, regs.eflags, regs.useresp, regs.ss);
            debug_stacktrace();
            debug_panic("unhandled serious fault in the kernel");
         }
         else
         {
            /* if this process was already trying to handle a GPF then kill it.
               the kernel's signal code in msg.c will clear this bit when the
               thread next */
            if(cpu_table[CPU_ID].current->proc->unix_signals_inprogress & (1 << SIGBUS))
               proc_kill(cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc);
            else
            {
               /* mark this process as attempting to handle the fault */
               cpu_table[CPU_ID].current->proc->unix_signals_inprogress |= (1 << SIGBUS);
               
               if(msg_send_signal(cpu_table[CPU_ID].current->proc, NULL, SIGBUS, 0))
                  /* something went wrong, so default action is shoot to kill */
                  proc_kill(cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc);
            }
         }
         break;
         
      case INT_PF: /* PAGE FAULT */
         if(pg_fault(&regs))
         /* something went wrong in the user thread, so default action is to kill
            the process - pg_fault() doesn't return if the kernel hits an unhandled
            kernel page fault */
         {
            /* kill the process if it in the middle of trying to fix-up a page fault */
            if(cpu_table[CPU_ID].current->proc->unix_signals_inprogress & (1 << SIGSEGV))
               syscall_do_exit(&regs);
            else
            {
               /* mark this process as attempting to handle the fault */
               cpu_table[CPU_ID].current->proc->unix_signals_inprogress |= (1 << SIGSEGV);         
   
               if(msg_send_signal(cpu_table[CPU_ID].current->proc, NULL, SIGSEGV, 0))
               {
                  /* something went wrong, so shoot to kill */
                  regs.eax = POSIX_GENERIC_FAILURE;
                  syscall_do_exit(&regs);
               }
            }
         }
         break;
         
      case INT_IPI_RESCHED: /* IPI: Foreced reschedule */
         /* wait until another core marks this thread as
            non-running and then pick another thread */
         lock_gate(&(cpu_table[CPU_ID].current->lock), LOCK_READ);
         while(cpu_table[CPU_ID].current->state != running);
         unlock_gate(&(cpu_table[CPU_ID].current->lock), LOCK_READ);
         break;

      case INT_IPI_FLUSHTLB: /* IPI: flush TLB/reload page tables */
         {
            thread *t = cpu_table[CPU_ID].current;
            if(t && t->state == running)
               x86_load_cr3(KERNEL_LOG2PHYS(t->proc->pgdir));
         }
         break;
         
      case INT_KERNEL_SWI: /* KERNEL SYSTEM CALL */
         /* kernel swi number in edx, params in eax-ecx */
         switch(regs.edx)
         {
            case SYSCALL_EXIT:
               syscall_do_exit(&regs);
               break;
               
            case SYSCALL_FORK:
               syscall_do_fork(&regs);
               break;
               
            case SYSCALL_KILL:
               syscall_do_kill(&regs);
               break;
               
            case SYSCALL_ALARM:
               syscall_do_alarm(&regs);
               break;
               
            case SYSCALL_THREAD_YIELD:
               syscall_do_yield(&regs);
               break;
               
            case SYSCALL_THREAD_EXIT:
               syscall_do_thread_exit(&regs);
               break;
               
            case SYSCALL_THREAD_FORK:
               syscall_do_thread_fork(&regs);
               break;
               
            case SYSCALL_THREAD_KILL:
               syscall_do_thread_kill(&regs);
               break;
               
            case SYSCALL_THREAD_SLEEP:
               syscall_do_thread_sleep(&regs);
               break;
               
            case SYSCALL_MSG_SEND:
               syscall_do_msg_send(&regs);
               break;
               
            case SYSCALL_MSG_RECV:
               syscall_do_msg_recv(&regs);
               break;
      
            case SYSCALL_PRIVS:
               syscall_do_privs(&regs);
               break;
               
            case SYSCALL_INFO:
               syscall_do_info(&regs);
               break;
               
            case SYSCALL_DRIVER:
               syscall_do_driver(&regs);
               break;
               
            case SYSCALL_SET_ID:
               syscall_do_set_id(&regs);
               break;

            default:
               XPT_DEBUG("[xpt:%i] unknown syscall %x by thread %i in process %i\n",
                         CPU_ID, regs.edx, cpu_table[CPU_ID].current->tid,
                         cpu_table[CPU_ID].current->proc->pid);
               break;
         }
         break;
      
      default:
         XPT_DEBUG("[xpt:%i] unhandled exception %x/%x received!\n",
                   CPU_ID, regs.intnum, regs.errcode);
   }
   
   /* there might be a thread of a higher-priority waiting to be run or the current process
      may not exist - so prod the scheduler to switch to another thread if need be -
      but don't try to switch out if we just did a kernel->kernel exception */
   if((regs.eip < KERNEL_SPACE_BASE) || (cpu_table[CPU_ID].current->state != running))
      sched_pick(&regs);

   XPT_DEBUG("[xpt:%i] OUT: ds %x edi %x esi %x ebp %x esp %x ebx %x edx %x ecx %x eax %x\n"
             "      intnum %x errcode %x eip %x cs %x eflags %x useresp %x ss %x\n",
             CPU_ID, regs.ds, regs.edi, regs.esi, regs.ebp, regs.esp, regs.ebx, regs.edx, regs.ecx, regs.eax,
             regs.intnum, regs.errcode, regs.eip, regs.cs, regs.eflags, regs.useresp, regs.ss);
   
   PERFORMANCE_DEBUG("[xpt:%i] software interrupt %x took about %i cycles to process\n", CPU_ID,
                     regs.intnum, (unsigned int)(x86_read_cyclecount() - debug_cycles));
}

/* exceptions_initialise
   laboriously setup the basic exception handlers
   kernel code segment is the 4th entry in the GDT,
   hence (4 - 1) * (sizeof GDT entry) = 3 * 8 = 24 = 0x18
   flags of 0x83 = int gate, 32bit wide, present */
void exceptions_initialise(void)
{
   XPT_DEBUG("[xpt:%i] initialising common x86 exception handlers...\n", CPU_ID);
   
   int_set_gate(0,  (unsigned int)isr0,  0x18, 0x8E, 0);
   int_set_gate(1,  (unsigned int)isr1,  0x18, 0x8E, 0);
   int_set_gate(2,  (unsigned int)isr2,  0x18, 0x8E, 0);
   int_set_gate(3,  (unsigned int)isr3,  0x18, 0x8E, 0);
   int_set_gate(4,  (unsigned int)isr4,  0x18, 0x8E, 0);
   int_set_gate(5,  (unsigned int)isr5,  0x18, 0x8E, 0);
   int_set_gate(6,  (unsigned int)isr6,  0x18, 0x8E, 0);
   int_set_gate(7,  (unsigned int)isr7,  0x18, 0x8E, 0);
   int_set_gate(8,  (unsigned int)isr8,  0x18, 0x8E, 0);
   int_set_gate(9,  (unsigned int)isr9,  0x18, 0x8E, 0);
   int_set_gate(10, (unsigned int)isr10, 0x18, 0x8E, 0);
   int_set_gate(11, (unsigned int)isr11, 0x18, 0x8E, 0);
   int_set_gate(12, (unsigned int)isr12, 0x18, 0x8E, 0);
   int_set_gate(13, (unsigned int)isr13, 0x18, 0x8E, 0);
   int_set_gate(14, (unsigned int)isr14, 0x18, 0x8E, 0);
   int_set_gate(15, (unsigned int)isr15, 0x18, 0x8E, 0);
   int_set_gate(16, (unsigned int)isr16, 0x18, 0x8E, 0);
   int_set_gate(17, (unsigned int)isr17, 0x18, 0x8E, 0);
   int_set_gate(18, (unsigned int)isr18, 0x18, 0x8E, 0);
   int_set_gate(19, (unsigned int)isr19, 0x18, 0x8E, 0);
   int_set_gate(20, (unsigned int)isr20, 0x18, 0x8E, 0);
   int_set_gate(21, (unsigned int)isr21, 0x18, 0x8E, 0);
   int_set_gate(22, (unsigned int)isr22, 0x18, 0x8E, 0);
   int_set_gate(23, (unsigned int)isr23, 0x18, 0x8E, 0);
   int_set_gate(24, (unsigned int)isr24, 0x18, 0x8E, 0);
   int_set_gate(25, (unsigned int)isr25, 0x18, 0x8E, 0);
   int_set_gate(26, (unsigned int)isr26, 0x18, 0x8E, 0);
   int_set_gate(27, (unsigned int)isr27, 0x18, 0x8E, 0);
   int_set_gate(28, (unsigned int)isr28, 0x18, 0x8E, 0);
   int_set_gate(29, (unsigned int)isr29, 0x18, 0x8E, 0);
   int_set_gate(30, (unsigned int)isr30, 0x18, 0x8E, 0);
   int_set_gate(31, (unsigned int)isr31, 0x18, 0x8E, 0);
   
   /* define the software interrupt vector - starting from 0x90 (144)
    OR flags with 0x60 to set DPL to ring3, so userland can call them */
   int_set_gate(INT_KERNEL_SWI, (unsigned int)isr144, 0x18, 0x8E | 0x60, 1); /* reload idt */
}
