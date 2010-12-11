/* kernel/ports/i386/syscalls/sys_msgs.c
 * i386-specific software interrupt handling for sending and receiving messages
 * Author : Chris Williams
 * Date   : Sat,29 Nov 2009.18:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>


/* syscall:msg_send - send a message to a process and block until a reply is received
   => eax = pointer to message description block
   <= eax = 0 for success or a diosix-specific error code
*/
void syscall_do_msg_send(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   diosix_msg_info *msg = (diosix_msg_info *)regs->eax;
   unsigned int send_result;
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_MSG_SEND(%x) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);

   /* sanitise the input data while we're here */
   if(!msg || ((unsigned int)msg + MEM_CLIP(msg, sizeof(diosix_msg_info)) >= KERNEL_SPACE_BASE))
      SYSCALL_RETURN(e_bad_address);
   
   /* determine the type of message being sent - this code is mostly portable... */
   if(msg->flags & DIOSIX_MSG_SIGNAL)
   {
      process *target;
      
      SYSCALL_DEBUG("[sys:%i] sending signal %i...\n", CPU_ID, msg->signal.number);
      
      /* send a signal to the caller's process group */
      if(msg->flags & DIOSIX_MSG_INMYPROCGRP)
         SYSCALL_RETURN(proc_send_group_signal(current->proc->proc_group_id, current,
                                               msg->signal.number, msg->signal.extra));
      
      /* send a signal to an arbitrary process group - pgid zero means use the caller's */
      if(msg->flags & DIOSIX_MSG_INAPROCGRP)
         SYSCALL_RETURN(proc_send_group_signal(msg->pid, current,
                                               msg->signal.number, msg->signal.extra));
      
      /* default to sending a single signal - check to see if we're sending to a
         specifically named process (by role) or otherwise use the suggested pid */
      if(msg->role)
         target = proc_role_lookup(msg->role);
      else
         target = proc_find_proc(msg->pid);
      if(!target) SYSCALL_RETURN(e_not_found);
      
      /* send the signal */
      send_result = msg_send_signal(target, current, msg->signal.number, msg->signal.extra);
      
      /* write in the target PID if successfully delivered */
      if(!send_result)
         msg->pid = target->pid;
      
      SYSCALL_RETURN(send_result);
   }
   
   /* bail out if it's not a generic sync message */
   if(!(msg->flags & DIOSIX_MSG_GENERIC)) SYSCALL_RETURN(e_bad_params);
   
   /* do the actual sending */
   send_result = msg_send(current, msg);
   
   SYSCALL_DEBUG("[sys:%i] SYSCALL_MSG_SEND(%x) msg_send() returned code %i\n",
                 CPU_ID, regs->eax, send_result);
   
   switch(send_result)
   {
      case success:
         /* reward the thread for multitasking */
         sched_priority_calc(current, priority_reward);
         
         /* should we wait for a follow-up message if this was a reply? */
         if((msg->flags & DIOSIX_MSG_REPLY) && (msg->flags & DIOSIX_MSG_RECVONREPLY))
         {         
            /* clear message flags to perform a recv */
            msg->flags &= DIOSIX_MSG_TYPEMASK;
            
            /* zero the send info and preserve everything else */
            msg->send_size = 0;
            msg->send = NULL;
            
            syscall_do_msg_recv(regs); /* will update eax when it returns */      
         }
         else
            SYSCALL_RETURN(success); /* let the sender know what happened */
         
      /* can't write into the receiver's buffer */
      case e_bad_target_address:
         SYSCALL_RETURN(e_no_receiver);
         
      default:
         /* fall through to returning msg_send()'s error code */
         SYSCALL_RETURN(send_result);
   }
}

/* syscall_post_msg_send
   This is used just before a blocked process is woken up because
   its queued message was successfully delivered or delivery failed.
   => sender = sender being woken up
      result = value to put in eax
*/
void syscall_post_msg_send(thread *sender, kresult result)
{
   /* sanity checks */
   if(!sender) return;
   
   /* only update threads that are actually asleep */
   if(sender->state == waitingforreply)
      sender->regs.eax = result;
}

/* syscall:msg_recv - receive a message or block until a message is received
   => eax = pointer to message description block
   <= eax = 0 for success or a diosix-specific error code
*/
void syscall_do_msg_recv(int_registers_block *regs)
{
   thread *current = cpu_table[CPU_ID].current;
   diosix_msg_info *msg = (diosix_msg_info *)regs->eax;

   SYSCALL_DEBUG("[sys:%i] SYSCALL_MSG_RECV(%x) called by process %i (%p) (thread %i)\n",
           CPU_ID, regs->eax, cpu_table[CPU_ID].current->proc->pid, cpu_table[CPU_ID].current->proc,
           cpu_table[CPU_ID].current->tid);
   
   /* sanitise the input data while we're here */
   if(!msg || (((unsigned int)msg + MEM_CLIP(msg, sizeof(diosix_msg_info))) >= KERNEL_SPACE_BASE))
      SYSCALL_RETURN(e_bad_address);
   
   /* do the actual receiving. syscall_post_msg_recv() will update the receiver later
      if necessary, otherwise return an immediate status code */
   regs->eax = msg_recv(current, msg);
}

/* syscall_post_msg_recv
   Perform the final stage of syscall:msg_recv just before the call returns.
   It is called from the context of the sending thread.
   => receiver = thread that blocked awaiting a message
      result = return code from the syscall
*/
void syscall_post_msg_recv(thread *receiver, kresult result)
{
   diosix_msg_info *msg;
   
   if(!receiver) return; /* sanity check */
   
   /* get the address of the receiver's block - it's been verified by this point.
      FIXME: what if the process pulls the rug from under our feet, having one thread
      block with this msg ptr and another thread marking it invalid immediately after? :-( */
   msg = receiver->msg_src;
   
   MSG_DEBUG("[msg:%i] updating receiver %p (tid %i pid %i) msg %p result %i\n",
             CPU_ID, receiver, receiver->tid, receiver->proc->pid, msg, result);
   
   /* copy the receiver's updated message block back into its address space */
   if(vmm_memcpyuser(msg, receiver->proc, &(receiver->msg), NULL, sizeof(diosix_msg_info)))
      receiver->regs.eax = e_bad_address;
   else
      receiver->regs.eax = result;
}
