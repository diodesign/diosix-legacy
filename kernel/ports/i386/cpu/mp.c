/* kernel/ports/i386/cpu/mp.c
 * i386-specific multiprocessor support
 * Author : Chris Williams
 * Date   : Sun,15 Oct 2009.22:54.00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* totals of resources available on this system */
unsigned char mp_cpus = 0;
unsigned char mp_ioapics = 0;
unsigned char mp_boot_cpu = 0;
unsigned char mp_domains = 0;

/* pointer to table of cpu descriptors */
mp_core *cpu_table = NULL;
 
/* set to 1 by an AP to indicate it's reached startup, reset to 0 before attempting to bake an AP */
volatile unsigned char mp_ap_ready = 0;

/* mp_catch_ap
   This is the point where an application processor joins the kernel proper */
void _mp_catch_ap(void)
{   
   MP_DEBUG("[mp:%i] I'm alive! application processor waiting for work\n", CPU_ID);
   
   /* signal that we're done to the boot cpu */
   mp_ap_ready = 1;
   
   /* don't forget to initialise interrupts for this cpu */
   lapic_initialise(INT_IAMAP);
   int_reload_idtr();

   /* loop waiting for the first thread to run */
   lowlevel_kickstart();
}

/* mp_delay
   Cause the processor to pause for a few cycles - the length of a normal
   scheduling quantum */
void mp_delay(void)
{   
   /* put the local APIC timer into one-shot mode and wait for it to hit zero */
   lapic_write(LAPIC_LVT_TIMER, IRQ_APIC_TIMER);
   lapic_write(LAPIC_TIMERDIV,  LAPIC_DIV_128); /* divide down the bus clock by 128 */
   lapic_write(LAPIC_TIMERINIT, *(LAPIC_TIMERINIT));

   while(*(LAPIC_TIMERNOW)) __asm__ __volatile__("pause");
         
   /* restore the local APIC timer settings */
   lapic_write(LAPIC_LVT_TIMER, IRQ_APIC_TIMER | LAPIC_TIMER_TP);
   lapic_write(LAPIC_TIMERDIV,  LAPIC_DIV_128); /* divide down the bus clock by 128 */
   lapic_write(LAPIC_TIMERINIT, *(LAPIC_TIMERINIT));
}

/* mp_init_ap
   Initialise and start up an application processor
   => id = target CPU's local APIC ID
*/
kresult mp_init_ap(unsigned int id)
{
   volatile unsigned int ap_timeout = 0xffffff; /* sufficiently crude timeout */
   unsigned int apstack_top, gdtsize, newgdt, is_not_82489dx;
   
   /* determine APIC version - if a 82489dx is present (external APIC)
      then the APIC version byte will be 0 */
   is_not_82489dx = (*LAPIC_VER_REG) & 0xff;
   
   /* allocate about 1K memory for the ap boot stack to stash away */
   if(vmm_malloc((void **)&apstack_top, MP_AP_START_STACK_SIZE))
   {
      MP_DEBUG("[mp:%i] unable to allocate boot stack for AP %i\n", CPU_ID, id);
      return e_failure;
   }
   
   /* duplicate the BSP's GDT */
   gdtsize = (unsigned int)&KernelGDTEnd - (unsigned int)&KernelGDT;
   if(vmm_malloc((void **)&newgdt, gdtsize))
   {
      MP_DEBUG("[mp:%i] unable to allocate fresh GDT for AP %i\n", CPU_ID, id);
      vmm_free((void *)apstack_top);
      return e_failure;
   }
   vmm_memcpy((void *)newgdt, &KernelGDT, gdtsize);
   
   /* set up the GDT pointers for the cpu */
   cpu_table[id].gdtptr.size = gdtsize - 1;
   cpu_table[id].gdtptr.ptr = newgdt;
   /* TSS selector is in the final selector slot in the GDT.. */
   cpu_table[id].tssentry = (gdt_entry *)((newgdt + gdtsize) - sizeof(gdt_entry));
   
   /* update the word holding the stack pointer */
   apstack_top += MP_AP_START_STACK_SIZE; /* stacks grow down.. */
   *((volatile unsigned int *)&APStack) = apstack_top;
   
   MP_DEBUG("[mp:%i] initialising AP %i: GDT %x boot stack %x APIC version %x...\n",
           CPU_ID, id, newgdt, apstack_top, is_not_82489dx);
   
   mp_ap_ready = 0; /* the AP will set this flag when it's done initialising */

   /* following the intel MP spec docs.. */
   lapic_ipi_send_init(id); /* send INIT IPI to the AP */
   mp_delay();
   if(is_not_82489dx)
   {

      MP_DEBUG("[mp:%i] sending startup IPI..\n", CPU_ID);

      /* pulse startup IPIs until it wakes up */
      lapic_ipi_send_startup(id, MP_AP_START_VECTOR);
      mp_delay();
      if(!mp_ap_ready)
      {
         lapic_ipi_send_startup(id, MP_AP_START_VECTOR);
         mp_delay();
      }
   }
   
   MP_DEBUG("[mp:%i] waiting for AP %i to wake up..\n", CPU_ID, id);
   
   /* wait for AP to signal it's done initialising or give up waiting */
   while(!mp_ap_ready && ap_timeout)
   {
      ap_timeout--;
      __asm__ __volatile__("pause");
   }
   
   if(mp_ap_ready)
   {
      MP_DEBUG("[mp:%i] AP %i is cooking on gas\n", CPU_ID, id);
      return success;
   }

   MP_DEBUG("[mp:%i] initialisation of AP %i timed out\n", CPU_ID, id);
   return e_failure;
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
   unsigned int ap_loop, wokenup = 0;
   unsigned int *kaddr, phys_gdt, phys_gdtptr, phys_pgdir;
   gdtptr_descr gdtptr;
   
   /* get the processors ready to run */
   mp_init_cpu_table();
   
   if(mp_cpus < 2) return success; /* uniproc machines need not apply for the rest */

   /* calculate address to copy the trampoline code into */
   kaddr = KERNEL_PHYS2LOG(MP_AP_START_VECTOR << MP_AP_START_VECTOR_SHIFT);

   /* copy trampoline code into a low physical page */
   vmm_memcpy(kaddr, &x86_start_ap, (unsigned int)&x86_start_ap_end - (unsigned int)&x86_start_ap);
   
   /* stash the kernel's GDT ptr into the base of the physical page above the trampoline code
      plus the actual boot GDT into the base of the page above that and a copy of the kernel's
      boot page directory in the page above that */
   phys_gdtptr = (MP_AP_START_VECTOR << MP_AP_START_VECTOR_SHIFT) + MEM_PGSIZE;
   phys_gdt    = (MP_AP_START_VECTOR << MP_AP_START_VECTOR_SHIFT) + (2 * MEM_PGSIZE);
   phys_pgdir  = (MP_AP_START_VECTOR << MP_AP_START_VECTOR_SHIFT) + (3 * MEM_PGSIZE);
   gdtptr.size = (unsigned int)&KernelGDTEnd - (unsigned int)&KernelGDT;
   gdtptr.ptr = phys_gdt;
   
   /* copy the gdtptr into the base of the page above the trampoline and the GDT into the
      second page above the trampoline */
   vmm_memcpy(KERNEL_PHYS2LOG(phys_gdtptr), &gdtptr, sizeof(gdtptr_descr));
   vmm_memcpy(KERNEL_PHYS2LOG(phys_gdt), &KernelGDT, gdtptr.size);
   vmm_memcpy(KERNEL_PHYS2LOG(phys_pgdir), &KernelPageDirectory, 1024 * sizeof(unsigned int));
   
   MP_DEBUG("[mp:%i] copied AP trampoline code %x to %x (mapped to %x) (%i bytes)..\n"
           "       ..plus boot GDT to %x (GDT ptr at %x) (%i bytes) and page directory to %x\n",
           CPU_ID, &x86_start_ap, (MP_AP_START_VECTOR << MP_AP_START_VECTOR_SHIFT),
           kaddr, (unsigned int)&x86_start_ap_end - (unsigned int)&x86_start_ap,
           phys_gdt, phys_gdtptr, gdtptr.size, phys_pgdir);
   
   /* an init'd AP will reboot and jump to 0x467 (40:67) for a start address
      if we set the warm-boot flag */
   *(volatile unsigned int *)KERNEL_PHYS2LOG(0x467) = MP_AP_START_VECTOR << MP_AP_START_VECTOR_SHIFT;   

   /* program the BIOS CMOS for a warm reset - see: http://wiki.osdev.org/CMOS */
   x86_cmos_write(X86_CMOS_RESET_BYTE, X86_CMOS_RESET_WARM);   
   
   /* and let's do battle.. */
   for(ap_loop = 0; ap_loop < mp_cpus; ap_loop++)
   {
      /* don't boot the already running bootstrap cpu */
      if(ap_loop != CPU_ID)
      {
         if(mp_init_ap(ap_loop) == success)
         {
            wokenup++;
            cpu_table[ap_loop].state = enabled;
         }
         else
            /* stop the scheduler from using APs that haven't been started up */
            cpu_table[ap_loop].state = disabled;
      }
      else
         cpu_table[ap_loop].state = enabled; /* enabled boot processor is enabled */
   }

   BOOT_DEBUG("[mp:%i] woke up %i application processor(s) out of %i\n", CPU_ID, wokenup, mp_cpus - 1);
   
   /* put the CMOS byte back to normal */
   x86_cmos_write(X86_CMOS_RESET_BYTE, X86_CMOS_RESET_COLD);
   
   return success;
}

kresult mp_initialise(void)
{
   unsigned char i;
   unsigned int search;
   unsigned char mp_found = 0;
   mp_header_block *mp_header= NULL;
   mp_config_block *mp_config = NULL;
   mp_entry_block *info_block;
   
   /* groked a lot of detail from... http://www.osdever.net/tutorials/view/multiprocessing-support-for-hobby-oses-explained
   and http://people.freebsd.org/~fsmp/SMP/SMP.html
   http://people.freebsd.org/~fsmp/SMP/mptable/mptable.c */
   
   /* search for the magic MP signature word to determine if this is a UP or MP machine */
   
   /* start with the extended BIOS data area (128K - 0xA0000) */
   MP_DEBUG("[mp] searching for multiprocessor info in EBDA...\n");
   for(search = MP_EBDA_START; search < MP_EBDA_END; search += sizeof(mp_header_block *))
   {
      mp_header = (mp_header_block *)search;
      if(mp_header->signature == MP_MAGIC_SIG)
      {
         mp_found = 1;
         break;
      }
   }
   
   if(!mp_found)
   {
      /* search the last 1024 bytes of base memory (ends at 640K) */
      MP_DEBUG("[mp] searching for multiprocessor info in top of base memory...\n");
      for(search = MP_LASTK_START; search < MP_LASTK_END; search += sizeof(mp_header_block *))
      {
         mp_header = (mp_header_block *)search;
         if(mp_header->signature == MP_MAGIC_SIG)
         {
            mp_found = 1;
            break;
         }
      }
   }

   if(!mp_found)
   {
      /* search the BIOS ROM space */
      MP_DEBUG("[mp] searching for multiprocessor info in the BIOS...\n");   
      for(search = MP_ROM_START; search < MP_ROM_END; search += sizeof(mp_header_block *))
      {
         mp_header = (mp_header_block *)search;
         if(mp_header->signature == MP_MAGIC_SIG)
         {
            mp_found = 1;
            break;
         }
      }
   }
   
   /* must be a single processor system if we can't find the signature :( */
   if(!mp_found)
   {
      BOOT_DEBUG("[mp] assuming uniprocessor machine\n");
      mp_cpus = 1;
      mp_boot_cpu = 0;
      int_initialise_common();
      return success;
   }

   /* we're in multiprocessor territory now */
   
   BOOT_DEBUG("[mp] multiprocessor machine detected\n");
   
   /* start pulling apart system info */
   mp_config = mp_header->configptr;
   BOOT_DEBUG("[mp] system: ");
   for(i = 0; i < 8; i++)
      BOOT_DEBUG("%c", mp_config->oemid[i]);
   BOOT_DEBUG(" ");
              
   BOOT_DEBUG(" ");
   for(i = 0; i < 12; i++)
      BOOT_DEBUG("%c", mp_config->productid[i]);
   BOOT_DEBUG("\n");

   info_block = (mp_entry_block *)((unsigned int)mp_config + sizeof(mp_config_block));
   for(i = 0; i < mp_config->entry_count; i++)
   {
      unsigned char block_size;
      
      switch(info_block->type)
      {
         case 0: /* cpu */
            BOOT_DEBUG("[mp] found cpu %i (flags %x) ",
                         info_block->id, info_block->entry.cpu.flags);
            if(info_block->entry.cpu.flags & MP_IS_BSP)
            {
               mp_boot_cpu = info_block->id;
               BOOT_DEBUG("[boot processor]");
            }
            else
            {
               BOOT_DEBUG("[application processor]");
            }
            BOOT_DEBUG("\n");
            mp_cpus++;
            block_size = 20;
            break;
            
         case 1: /* bus entry */
            bus_register_name(info_block->id, (char *)&(info_block->entry.bus.name));
            block_size = 8;
            break;
            
         case 2: /* ioapic */
            BOOT_DEBUG("[mp] found ioapic %i (id %i flags %x addr %p)\n", mp_ioapics,
                       info_block->id, info_block->entry.ioapic.flags, info_block->entry.ioapic.physaddr);
            if(info_block->entry.ioapic.flags & MP_IS_IOAPIC_ENABLED)
            {
               /* if this chip is set to go, then register it with the system */
               if(ioapic_register_chip(info_block->id, (unsigned int)info_block->entry.ioapic.physaddr) == success)
                  mp_ioapics++;
            }

         case 3: /* ->ioapic routing info */
            bus_add_route(info_block->entry.interrupt.flags,
                          info_block->entry.interrupt.source_bus_id,
                          info_block->entry.interrupt.source_bus_irq,
                          info_block->entry.interrupt.dest_ioapic_id,
                          info_block->entry.interrupt.dest_ioapic_irq);
            block_size = 8;
            break;
            
         default:
            block_size = 8;
            break;
      }
      
      info_block = (mp_entry_block *)((unsigned int)info_block + block_size);
   }
   
   MP_DEBUG("[mp] checking apic reg access... bsp apic id %i version %x\n",
           *(LAPIC_ID_REG), *(LAPIC_VER_REG));
   
   /* let the bootstrap processor continue the initialisation */
   int_initialise_common();
   
   return success;
}
