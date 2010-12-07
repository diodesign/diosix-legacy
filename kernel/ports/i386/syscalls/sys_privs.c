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

/* syscall_do_id_change
   Update a process's id block depending on the action code supplied.
   => uid = user id of the process requesting the alterations
      id = pointer to a process's id block to update, either its uid or gid block
      action = DIOSIX_SETEUID, DIOSIX_SETREUID, DIOSIX_SETRESUID or one of the corresponding
               group id action codes. their behaviour is defined below.
      eid, rid, sid = new effective, real and saved ids for the process, or -1 for no change
      unless mandatory.
   <= 0 for success, or an error code
*/
kresult syscall_do_id_change(unsigned int uid, posix_id_set *id, unsigned int action,
                             unsigned int eid, int unsigned rid, unsigned int sid)
{
   /* assumes lock is held on the target process */
   
   switch(action)
   {
      /* set the effective id - eid is mandatory */
      case DIOSIX_SETEUID:
      case DIOSIX_SETEGID:
      {         
         if((uid == SUPERUSER_ID) || (id->real == eid) ||
            (id->effective == eid) || (id->saved == eid))
            id->effective = eid;
         else
            return e_no_rights;
      }
      return success;
         
      /* set the real and effective user id with a saved id */
      case DIOSIX_SETREUID:
      case DIOSIX_SETREGID:
      {
         /* if real id is written to or effective id is written to
            and is not equal to the original real id, then set this
            flag to 1 to change the saved id to the effective id */ 
         unsigned char do_save = 0;

         /* check we want to change the effective id */
         if((signed int)eid != -1)
         {
            if((uid == SUPERUSER_ID) || (id->real == eid) ||
               (id->effective == eid) || (id->saved == eid))
            {
               if(eid != id->real) do_save = 1;
               id->effective = eid;
            }
            else
               return e_no_rights;
         }
         
         /* check we want to change the real id */
         if((signed int)rid != -1)
         {
            if((uid == SUPERUSER_ID) || (id->real == rid) ||
               (id->effective == rid))
            {
               id->real = rid;
               do_save = 1;
            }
            else
               return e_no_rights;
         }
         
         /* save the effective id if necessary */
         if(do_save) id->saved = id->effective;
      }
      return success;
         
      /* set the real, effective and saved ids */
      case DIOSIX_SETRESUID:
      case DIOSIX_SETRESGID:
      {
         /* check we want to change the effective id */
         if((signed int)id->effective != -1)
         {
            if((uid == SUPERUSER_ID) || (id->real == eid) ||
               (id->effective == eid) || (id->saved == eid))
               id->effective = eid;
            else
               return e_no_rights;
         }
         
         /* check we want to change the real id */
         if((signed int)id->real != -1)
         {
            if((uid == SUPERUSER_ID) || (id->real == rid) ||
               (id->effective == rid) || (id->saved == rid))
               id->real = rid;
            else
               return e_no_rights;
         }
         
         /* check we want to change the saved id */
         if((signed int)id->saved != -1)
         {
            if((uid == SUPERUSER_ID) || (id->real == sid) ||
               (id->effective == sid) || (id->saved == sid))
               id->saved = sid;
            else
               return e_no_rights;
         }
      }
      return success;
   }
   
   /* fall through to failure */
   return e_bad_params;
}

/* unlock the (a) process (if set) in syscall_do_set_id() and return (b) from the syscall */
#define SYSCALL_SET_ID_RETURN(a, b) { if((a)) unlock_gate(&((a)->lock), LOCK_WRITE); SYSCALL_RETURN(a); }

/* syscall: set_id - set various credential values inside a process,
   subject to security checks, namely the user and group ids of a program.
   => eax = DIOSIX_SETPGID: set process group id within same session
            ebx = pid of process to alter, or 0 to use calling process's pid
            ecx = new process group id to use, or 0 to use ebx
            DIOSIX_SETSID: set process session id and process group id to the process's pid
            DIOSIX_SETEUID: set the effective user id
            ebx = new effective user id
            DIOSIX_SETREUID: set the real and effective user id with saved id
            ebx = new effective user id, or -1 for don't change
            ecx = new real user id, or -1 for don't change
            DIOSIX_SETRESUID: set the real, effective and saved user id
            ebx = pointer to posix_id_set structure with new real, effective and saved user ids
            DIOSIX_SETEGID, DIOSIX_SETREGID or DIOSIX_SETRESGID:
            as above but for the group id rather than process id
   <= 0 for success, or an error code
*/
void syscall_do_set_id(int_registers_block *regs)
{
   unsigned int action = regs->eax;
   process *target = NULL, *current = cpu_table[CPU_ID].current->proc;
   posix_id_set *ids;

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
               if(target != current) SYSCALL_SET_ID_RETURN(target, e_no_rights);
               is_child = 0;
            }

            /* cannot use this call on a child if it has called exec() */
            if((target->flags & PROC_FLAG_HASCALLEDEXEC) && is_child)
               SYSCALL_SET_ID_RETURN(target, e_no_rights);
            
            /* session leader may not change pgid */
            if(target->session_id == pid)
               SYSCALL_SET_ID_RETURN(target, e_no_rights);
            
            /* process cannot be a child and in a separate session */
            if((target->session_id != current->session_id) && is_child)
               SYSCALL_SET_ID_RETURN(target, e_no_rights);
            
            /* correct the pgid if zero */
            if(!pgid) pgid = pid;
            
            /* the process group id must be valid and in same session */
            if(pgid != pid && !proc_is_valid_pgid(pgid, current->session_id, NULL))
               SYSCALL_SET_ID_RETURN(target, e_no_rights);
            
            /* finally update the target process */
            target->proc_group_id = pgid;
         }
         else SYSCALL_RETURN(e_not_found); /* target not locked */
      }
      SYSCALL_SET_ID_RETURN(target, success);

      case DIOSIX_SETSID:
      {
         /* set the session and process group id
            http://www.opengroup.org/onlinepubs/009695399/functions/setsid.html */
         
         lock_gate(&(current->lock), LOCK_WRITE);
         
         /* target cannot be a process group leader */
         if(target->proc_group_id == target->pid) SYSCALL_SET_ID_RETURN(current, e_no_rights);
         
         /* process group cannot exist elsewhere */
         if(proc_is_valid_pgid(target->pid, 0, target)) SYSCALL_SET_ID_RETURN(current, e_no_rights);
         
         /* update the process */
         target->session_id = target->proc_group_id = target->pid;
      }
      SYSCALL_SET_ID_RETURN(current, success);

      /* alter the process's user ids */
      case DIOSIX_SETEUID:
         lock_gate(&(current->lock), LOCK_WRITE);
         SYSCALL_SET_ID_RETURN(current,
                               syscall_do_id_change(current->uid.effective, &(current->uid),
                                                    action, regs->ebx, NULL, NULL));

      case DIOSIX_SETREUID:
         lock_gate(&(current->lock), LOCK_WRITE);
         SYSCALL_SET_ID_RETURN(current,
                               syscall_do_id_change(current->uid.effective, &(current->uid),
                                                    action, regs->ebx, regs->ecx, NULL));

      case DIOSIX_SETRESUID:
         ids = (posix_id_set *)regs->ebx;
         
         /* sanity check address */
         if(!ids || ((unsigned int)ids + MEM_CLIP(ids, sizeof(posix_id_set)) > KERNEL_SPACE_BASE))
            SYSCALL_RETURN(e_bad_params);
         
         lock_gate(&(current->lock), LOCK_WRITE);
         SYSCALL_SET_ID_RETURN(current,
                               syscall_do_id_change(current->uid.effective, &(current->uid),
                                                    action, ids->effective, ids->real, ids->saved));
                               
      /* alter the process's group ids */
      case DIOSIX_SETEGID:
         lock_gate(&(current->lock), LOCK_WRITE);
         SYSCALL_SET_ID_RETURN(current,
                               syscall_do_id_change(current->uid.effective, &(current->gid),
                                                    action, regs->ebx, NULL, NULL));
         
      case DIOSIX_SETREGID:
         lock_gate(&(current->lock), LOCK_WRITE);
         SYSCALL_SET_ID_RETURN(current,
                               syscall_do_id_change(current->uid.effective, &(current->gid),
                                                    action, regs->ebx, regs->ecx, NULL));
         
      case DIOSIX_SETRESGID:
         ids = (posix_id_set *)regs->ebx;
         
         /* sanity check address */
         if(!ids || ((unsigned int)ids + MEM_CLIP(ids, sizeof(posix_id_set)) > KERNEL_SPACE_BASE))
            SYSCALL_RETURN(e_bad_params);
         
         lock_gate(&(current->lock), LOCK_WRITE);
         SYSCALL_SET_ID_RETURN(current,
                               syscall_do_id_change(current->uid.effective, &(current->gid),
                                                    action, ids->effective, ids->real, ids->saved));
                               
      default:
         SYSCALL_RETURN(e_bad_params);
   }
}
