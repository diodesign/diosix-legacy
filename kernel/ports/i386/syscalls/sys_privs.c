/* kernel/ports/i386/syscalls/sys_privs.c
 * i386-specific software interrupt handling for managing processes' privileges
 * Author : Chris Williams
 * Date   : Sat,29 Nov 2009.18:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>


/* syscall:privs - alter the privs/rights of a process
   => eax = DIOSIX_PRIV_LAYER_UP: move up a privilege layer
            DIOSIX_RIGHTS_CLEAR:
              => ebx = set a bit to remove the corresponding PROC_FLAGS rights flag
            DIOSIX_IORIGHTS_REMOVE: remove *all* the process's IO port access
            DIOSIX_IORIGHTS_CLEAR: remove selected IO port access from a process
              => ebx = index (in 32bit words) into 8bit x (2^16) bitmap
                 ecx = word to apply to bitmap. Set bits to remove corresponding port access
            DIOSIX_UNIX_SIGNALS:
              => ebx = bitfield of POSIX-compatible signals that process will accept
            DIOSIX_KERNEL_SIGNALS:
              => ebx = bitfield of kernel-generated signals that process will accept
   <= eax = 0 for success or an error code
*/
void syscall_do_privs(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_PRIVS(%i) called by process %i (thread %i)\n",
                 CPU_ID, regs->eax, current->proc->pid, current->tid);
   
   switch(regs->eax)
   {
      case DIOSIX_PRIV_LAYER_UP:
         SYSCALL_RETURN(proc_layer_up(current->proc));
         
      case DIOSIX_RIGHTS_CLEAR:
         SYSCALL_RETURN(proc_clear_rights(current->proc, regs->ebx));
         
      case DIOSIX_IORIGHTS_REMOVE:
         SYSCALL_RETURN(x86_ioports_clear_all(current->proc));
         
      case DIOSIX_IORIGHTS_CLEAR:
         SYSCALL_RETURN(x86_ioports_clear(current->proc, regs->ebx, regs->ecx));
         
      case DIOSIX_UNIX_SIGNALS:
         current->proc->unix_signals_accepted = regs->ebx;
         SYSCALL_RETURN(success);
         
      case DIOSIX_KERNEL_SIGNALS:
         current->proc->kernel_signals_accepted = regs->ebx;
         SYSCALL_RETURN(success);
   }
   
   /* fall through to returning an error code */
   SYSCALL_RETURN(e_bad_params);
}
