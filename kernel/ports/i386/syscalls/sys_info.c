/* kernel/ports/i386/syscalls/sys_info.c
 * i386-specific software interrupt handling for gathering information
 * Author : Chris Williams
 * Date   : Sat,29 Nov 2009.18:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>


/* syscall:info - read information about a thread/process/system
   => eax = DIOSIX_THREAD_INFO: read info about the currently running thread
            DIOSIX_PROCESS_INFO: read info about the currently running process
            DIOSIX_KERNEL_INFO: read info about the currently running kernel
      ebx = pointer to empty diosix_thread_info/diosix_process_info/diosix_kernel_info
            structure for kernel to fill in
   <= eax = 0 for succes or an error code
*/
void syscall_do_info(int_registers_block *regs)
{
   diosix_info_block *block = (diosix_info_block *)(regs->eax);
   thread *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_INFO(%i) called by process %i (thread %i)\n",
                 CPU_ID, regs->ebx, current->proc->pid, current->tid);

   /* sanity check pointer for badness */
   if(!block || (regs->eax + MEM_CLIP(block, sizeof(diosix_info_block))) > KERNEL_SPACE_BASE)
      SYSCALL_RETURN(e_bad_address);
   
   /* fill out the block, a bad pointer will page fault in the caller's virtual space */
   switch(regs->ebx)
   {
      case DIOSIX_THREAD_INFO:
         block->data.t.tid      = current->tid;
         block->data.t.cpu      = current->cpu;
         block->data.t.priority = current->priority;
         SYSCALL_RETURN(success);
         
      case DIOSIX_PROCESS_INFO:
         block->data.p.pid       = current->proc->pid;
         block->data.p.parentpid = current->proc->parentpid;
         block->data.p.flags     = current->proc->flags;
         block->data.p.privlayer = current->proc->layer;
         block->data.p.ruid      = current->proc->uid.real;
         block->data.p.euid      = current->proc->uid.effective;
         block->data.p.ssuid     = current->proc->uid.saved;
         block->data.p.rgid      = current->proc->gid.real;
         block->data.p.egid      = current->proc->gid.effective;
         block->data.p.ssgid     = current->proc->gid.saved;
         block->data.p.proc_group_id = current->proc->proc_group_id;
         block->data.p.session_id    = current->proc->session_id;
         block->data.p.role = current->proc->role;
         SYSCALL_RETURN(success);
         
      /* these are defined externally in the makefile */
      case DIOSIX_KERNEL_INFO:
      {
         vmm_memcpy(&(block->data.k.identifier), KERNEL_IDENTIFIER, vmm_nullbufferlen(KERNEL_IDENTIFIER));
         block->data.k.release_major       = KERNEL_RELEASE_MAJOR;
         block->data.k.release_minor       = KERNEL_RELEASE_MINOR;
         block->data.k.kernel_api_revision = KERNEL_API_REVISION;
         SYSCALL_RETURN(success);
      }
         
      /* return kernel statistics */
      case DIOSIX_KERNEL_STATISTICS:
      {
         block->data.s.kernel_uptime       = sched_msec_counter;
         SYSCALL_RETURN(success);
      }
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}
