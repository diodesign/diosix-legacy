/* kernel/ports/i386/syscalls/sys_debug.c
 * i386-specific software interrupt handling for debugging userland software
 * Author : Chris Williams
 * Date   : Sun,09 Jan 2011.19:07:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

unsigned char *debug_roles[DIOSIX_ROLES_NR] = {
   "system executive",
   "vfs",
   "pager",
   "networkstack",
   "video console",
   "keyboard console",
   "pcimanager"
};

/* syscall:usrdebug - use the kernel to aid debugging of a userland process.
   => eax = DIOSIX_DEBUG_WRITE: write string to the kernel's debug channel
                                (root-owned processes only)
            ebx = pointer to null-terminated string
   <= eax = 0 for succes or an error code
*/
void syscall_do_debug(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_USRDEBUG(%i) called by process %i (thread %i)\n",
                 CPU_ID, regs->eax, current->proc->pid, current->tid);

   /* fill out the block, a bad pointer will page fault in the caller's virtual space */
   switch(regs->eax)
   {
      case DIOSIX_DEBUG_WRITE:
      {
         char *str = (char *)regs->ebx;
         
         /* sanity check pointer for badness */
         if(!str) SYSCALL_RETURN(e_bad_address);
         
         /* check permissions */
         if(current->proc->uid.effective != DIOSIX_SUPERUSER_ID)
            SYSCALL_RETURN(e_no_rights);
       
         if(current->proc->role)
         {
            dprintf("[debug:%i] [%s]: ",
                    CPU_ID, debug_roles[current->proc->role - 1]);
         }
         else
         {
            dprintf("[debug:%i] pid %i tid %i: ",
                    CPU_ID, current->proc->pid, current->tid);
         }
         
         /* output one byte at a time, checking not to collide with kernel space */
         while((unsigned int)str < KERNEL_SPACE_BASE && *str)
            dprintf("%c", *str++);
         
         SYSCALL_RETURN(success);
      }
         
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}
