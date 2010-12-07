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

/* unlock the target process (if set) in syscall_do_set_id() and return from the syscall */
#define SYSCALL_SET_ID_RETURN(a) { if(target) unlock_gate(&(target->lock), LOCK_WRITE); SYSCALL_RETURN(a); }

/* syscall: set_id - set various credential values inside a process,
   subject to security checks, namely the user and group ids of a program.
   => eax = DIOSIX_SETPGID: set process group id within same session
            ebx = pid of process to alter, or 0 to use calling process's pid
            ecx = new process group id to use, or 0 to use ebx
            DIOSIX_SETSID, DIOSIX_SETEUID, DIOSIX_SETREUID,
            DIOSIX_SETRESUID, DIOSIX_SETEGID, DIOSIX_SETREGID or DIOSIX_SETRESGID
   <= 0 for success, or an error code
*/
void syscall_do_set_id(int_registers_block *regs)
{
   unsigned int action = regs->eax;
   process *target = NULL, *current = cpu_table[CPU_ID].current->proc;

   SYSCALL_DEBUG("[sys:%i] SYSCALL_SET_ID(%i) called by process %i (%p) (thread %i)\n",
                 CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid,
                 cpu_table[CPU_ID].current->proc, cpu_table[CPU_ID].current->tid);
                 
   switch(action)
   {
      /* set a new process group id in the given process
         http://www.opengroup.org/onlinepubs/009695399/functions/setpgid.html */
      case DIOSIX_SETPGID:
      {
         unsigned int pid = regs->ebx;
         unsigned int pgid = regs->ecx;
         unsigned int is_child;

         /* pid of zero means use the calling process's pid */
         if(!pid)
         {
            pid = current->pid;
            target = current;
         }
         else
            target = proc_find_proc(pid);
         if(target)
         {
            lock_gate(&(target->lock), LOCK_WRITE);
            
            /* only perform this lookup once */
            if(proc_is_child(current, target) == success)
               is_child = 1;
            else
            {
               /* if we're altering another process and it's
                  not a child then give up */
               if(target != current) SYSCALL_SET_ID_RETURN(e_no_rights);
               is_child = 0;
            }

            /* cannot use this call on a child if it has called exec() */
            if((target->flags & PROC_FLAG_HASCALLEDEXEC) && is_child)
               SYSCALL_SET_ID_RETURN(e_no_rights);
            
            /* session leader may not change pgid */
            if(target->session_id == pid)
               SYSCALL_SET_ID_RETURN(e_no_rights);
            
            /* process cannot be a child and in a separate session */
            if((target->session_id != current->session_id) && is_child)
               SYSCALL_SET_ID_RETURN(e_no_rights);
            
            /* correct the pgid if zero */
            if(!pgid) pgid = pid;
            
            /* the process group id must be valid and in same session */
            if(pgid != pid && !proc_is_valid_pgid(pgid, current->session_id, NULL))
               SYSCALL_SET_ID_RETURN(e_no_rights);
            
            /* finally update the target process */
            target->proc_group_id = pgid;
         }
         else SYSCALL_RETURN(e_not_found); /* target not locked */
      }
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETSID:
      {
         /* set the session and process group id
            http://www.opengroup.org/onlinepubs/009695399/functions/setsid.html */
         target = current;
         
         lock_gate(&(target->lock), LOCK_WRITE);
         
         /* target cannot be a process group leader */
         if(target->proc_group_id == target->pid) SYSCALL_SET_ID_RETURN(e_no_rights);
         
         /* process group cannot exist elsewhere */
         if(proc_is_valid_pgid(target->pid, 0, target)) SYSCALL_SET_ID_RETURN(e_no_rights);
         
         /* update the process */
         target->session_id = target->proc_group_id = target->pid;
      }
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETEUID:
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETREUID:
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETRESUID:
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETEGID:
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETREGID:
      SYSCALL_SET_ID_RETURN(success);

      case DIOSIX_SETRESGID:
      SYSCALL_SET_ID_RETURN(success);
      
      default:
         SYSCALL_SET_ID_RETURN(e_bad_params);
   }
}
