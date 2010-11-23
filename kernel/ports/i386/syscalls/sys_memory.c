/* kernel/ports/i386/syscalls/sys_memory.c
 * i386-specific software interrupt handling for managing processes' memory
 * Author : Chris Williams
 * Date   : Sat,29 Nov 2009.18:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>


/* syscall:memory - manipulate a process's virtual memory space
   => eax = DIOSIX_MEMORY_CREATE: create a read-only virtual memory area, which cannot clash with any existing area.
               => ebx = pointer to the start of the page-aligned virtual area
                  ecx = size of the area in whole number of pages in bytes
            DIOSIX_MEMORY_DESTROY: destroy a previously created virtual memory area
               => ebx = pointer to start of virtual area to destroy
            DIOSIX_MEMORY_RESIZE: change the size of a previously created virtual memory area
               => ebx = pointer to start of virtual area to resize
                  ecx = number of bytes to increase the area by, or negative for number of bytes to reduce by
            DIOSIX_MEMORY_ACCESS: set the access flags of a virtual memory area.
               => ebx = pointer to start of virtual area to alter
                  ecx = access bits within the VMA_ACCESS_MASK bitmask
   <= eax = 0 for success or an error code
*/
void syscall_do_memory(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   unsigned int vma_base = regs->ebx;

   SYSCALL_DEBUG("[sys:%i] SYSCALL_MEMORY(%i, %x) called by thread %x in process %x\n",
                 CPU_ID, regs->eax, regs->ebx, current->tid, current->proc->pid);
   
   switch(regs->eax)
   {
      case DIOSIX_MEMORY_CREATE:
         /* vmm_add_vma() has sufficient sanity checking */
         SYSCALL_RETURN(vmm_add_vma(current->proc, vma_base, regs->ecx, VMA_MEMSOURCE, 0));
         
      case DIOSIX_MEMORY_DESTROY:
      {
         /* sanity check pointer */
         if(!vma_base || vma_base >= KERNEL_SPACE_BASE) SYSCALL_RETURN(e_bad_params);
         
         vmm_tree *node = vmm_find_vma(current->proc, vma_base, sizeof(char));
         if(!node) SYSCALL_RETURN(e_bad_address);
         
         SYSCALL_RETURN(vmm_unlink_vma(current->proc, node));
      }
         
      case DIOSIX_MEMORY_RESIZE:
      {
         kresult err;
         
         /* sanity check pointer */
         if(!vma_base || vma_base >= KERNEL_SPACE_BASE) SYSCALL_RETURN(e_bad_params);
         
         vmm_tree *node = vmm_find_vma(current->proc, vma_base, sizeof(char));
         if(!node) SYSCALL_RETURN(e_bad_address);
         
         err = vmm_resize_vma(current->proc, node, (signed int)(regs->ecx));
         if(!err && (signed int)(regs->ecx) < 0)
         {
            /* reload the process's page directory in case we've lost pages */
            x86_load_cr3(KERNEL_LOG2PHYS(current->proc->pgdir));
            mp_interrupt_process(current->proc, INT_IPI_FLUSHTLB);
         }
         SYSCALL_RETURN(err);
      }
         
      case DIOSIX_MEMORY_ACCESS:
      {
         /* sanity check pointer */
         if(!vma_base || vma_base >= KERNEL_SPACE_BASE) SYSCALL_RETURN(e_bad_params);
         
         vmm_tree *node = vmm_find_vma(current->proc, vma_base, sizeof(char));
         if(!node) SYSCALL_RETURN(e_bad_address);
         
         SYSCALL_RETURN(vmm_alter_vma(current->proc, node, regs->ecx & VMA_ACCESS_MASK));
      }
   }
   
   /* fall through to returning an error code because eax was incorrect */
   SYSCALL_RETURN(e_bad_params);
}
