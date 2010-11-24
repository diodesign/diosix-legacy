/* kernel/core/msg.c
 * diosix portable message-passing functions
 * Author : Chris Williams
 * Date   : Sat,14 Nov 2009.17:43:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* Return an error code from msg_send_signal() */
#define MSG_SIGNAL_REJECT(a) { err = a; goto msg_send_signal_exit; }

/* msg_send_signal
   Send a POSIX signal message to a process - this function will not block.
   It queues a signal message for the process and wakes up the thread that's
   supposed to be handling it, then returns. If no signal handler can be found
   then alert the caller.
   => target  = pointer to receiving process
      sender  = pointer to thread structure of sender, or NULL for kernel
      signum  = signal code to send
      sigcode = additional reason code
   <= 0 for success or an error reason code */
kresult msg_send_signal(process *target, thread *sender, unsigned int signum, unsigned int sigcode)
{
   kresult err = success;
   diosix_msg_info wakemsg;
   thread *towake;
   kpool *pool = NULL; /* pool queue - no pun intended */
   
   /* sanity checks - no NULL pointers nor SIGZERO */
   if(!target || !signum) return e_bad_params;
   
   /* protect from changes */
   err = lock_gate(&(target->lock), LOCK_WRITE);
   if(err) return err;
   
   /* only authorised senders and the kernel can send UNIX signals */
   if(signum <= SIG_UNIX_MAX &&
      (!sender || (sender->proc->flags & PROC_FLAG_CANUNIXSIGNAL)))
   {
      /* it's a UNIX-compatible signal, but is it accepted? */
      if(!(target->unix_signals_accepted & (1 << signum)))
         MSG_SIGNAL_REJECT(e_no_receiver);
      
      /* is the signal pending? */
      if(target->unix_signals_inprogress & (1 << signum))
         MSG_SIGNAL_REJECT(e_signal_pending);
      
      /* create a queued pool if one doesn't exist */
      if(!(target->system_signals))
      {
         pool = vmm_create_pool(sizeof(queued_signal), 4);
         if(!pool) MSG_SIGNAL_REJECT(e_failure);
      }
      else pool = target->system_signals;
      
      /* signal is in progress */
      target->unix_signals_inprogress |= (1 << signum);
   }
   
   /* only kernel can send kernel signals */
   if(signum >= SIG_KERNEL_MIN && signum <= SIG_KERNEL_MAX &&
      !sender)
   {
      /* is this kernel signal accepted? */
      if(!(target->kernel_signals_accepted & (1 << (signum - SIG_KERNEL_MIN))))
         MSG_SIGNAL_REJECT(e_no_receiver);
      
      /* create a queued pool if one doesn't exist */
      if(!(target->system_signals))
      {
         pool = vmm_create_pool(sizeof(queued_signal), 4);
         if(!pool) MSG_SIGNAL_REJECT(e_failure);
      }
      else pool = target->system_signals;
   }
   
   /* the kernel shouldn't really send user signals */
   if(signum >= SIG_USER_MIN && sender)
   {
      /* create a queued pool for the user signals if one doesn't exist */
      if(!(target->user_signals))
      {
         pool = vmm_create_pool(sizeof(queued_signal), 4);
         if(!pool) MSG_SIGNAL_REJECT(e_failure);
      }
      else pool = target->user_signals;
   }      
   
   /* if pool hasn't been set by now and we haven't bailed out
    of the function by now then something's wrong with signum */
   if(!pool) MSG_SIGNAL_REJECT(e_bad_params); 
   
   /* find a thread in the receiver to wake up to receive the signal */
   wakemsg.pid = target->pid;
   wakemsg.tid = DIOSIX_MSG_ANY_THREAD;
   wakemsg.flags = DIOSIX_MSG_SIGNAL;
   towake = msg_find_receiver(sender, &wakemsg);
   
   if(towake)
   {
      /* update the receiver with details of the signal sender */
      if(sender)
      {
         towake->msg.pid = sender->proc->pid;
         towake->msg.tid = sender->tid;
      }
      else
         towake->msg.pid = towake->msg.tid = RESERVED_PID;
      
      towake->msg.signal.number = signum;
      towake->msg.signal.extra = sigcode;
      
      /* wake the receiver */
      syscall_post_msg_recv(towake, success);
      sched_add(towake->cpu, towake);
      
      MSG_DEBUG("[msg:%i] woke thread %p (tid %i pid %i) to receive signal %i\n",
                CPU_ID, towake, towake->tid, towake->proc->pid, signum);
   }
   else
   {
      /* no receiver to wake up just yet, so queue the message:
         allocate our new signal block and write into it */      
      queued_signal *newsig;     
      
      err = vmm_alloc_pool((void **)&newsig, pool);
      if(err) MSG_SIGNAL_REJECT(err);
      
      newsig->signal.number = signum;
      newsig->signal.extra = sigcode;
      if(sender)
      {
         /* sender is a user thread */
         newsig->sender_pid = sender->proc->pid;
         newsig->sender_tid = sender->tid;
      }
      else
      /* sender is the kernel */
         newsig->sender_pid = newsig->sender_tid = RESERVED_PID;
      
      MSG_DEBUG("[msg:%i] queued signal %i:0x%x to pid %i from process %x\n",
                CPU_ID, signum, sigcode, target->pid, sender);      
   }
   
msg_send_signal_exit:
   unlock_gate(&(target->lock), LOCK_WRITE);
   return err;
}

/* msg_test_receiver
   Check if a given thread is capable of receiving the given message
   => sender = threading trying to send the message, or NULL for the 
               kernel trying to send a signal.
      target = thread message is trying to be sent to
      msg = message block trying to be sent
   <= 0 for success, or an error code
*/
kresult msg_test_receiver(thread *sender, thread *target, diosix_msg_info *msg)
{
   unsigned char layer;
   
   /* sanity check */
   if(!target || !msg)
   {
      KOOPS_DEBUG("[msg:%i] OMGWTF msg_test_receiver() called with sender %p target %p msg %p\n",
                  CPU_ID, sender, target, msg);
      return e_failure;
   }
   
   /* protect us from changes to the target's metadata */
   lock_gate(&(target->lock), LOCK_READ);
   
   switch(msg->flags & DIOSIX_MSG_TYPEMASK)
   {
      case DIOSIX_MSG_SIGNAL:
         /* is the target thread willing to accept the message type? */
         if((target->state == waitingformsg) &&
            ((target->msg.flags & DIOSIX_MSG_TYPEMASK) == DIOSIX_MSG_SIGNAL))
            goto msg_test_receiver_success;
         break;
      
      case DIOSIX_MSG_GENERIC:
         if(!sender) goto msg_test_receiver_failure;
         
         /* calculate layer - if DIOSIX_MSG_SENDASUSR is set then the message is
            being sent as an unprivileged user program */
         if((sender->proc->flags & PROC_FLAG_CANMSGASUSR) &&
            (msg->flags & DIOSIX_MSG_SENDASUSR))
            layer = LAYER_MAX;
         else
            layer = sender->proc->layer;
   
         /* threads can only recieve messages if they are in a layer below the sender - unless it's a 
            reply. we check below whether this is a legit reply */
         if((target->proc->layer >= layer) && !(msg->flags & DIOSIX_MSG_REPLY))
            goto msg_test_receiver_failure;
   
         /* is this message waiting on a reply from this thread? */
         if((target->state == waitingforreply) &&
            (msg->flags & DIOSIX_MSG_REPLY) &&
            (target->replysource == sender))
            goto msg_test_receiver_success;
   
         /* is the target thread willing to accept the message type? */
         if((target->state == waitingformsg) &&
            ((target->msg.flags & DIOSIX_MSG_TYPEMASK) == DIOSIX_MSG_GENERIC))
            goto msg_test_receiver_success;
   }
   
   /* give up */
   unlock_gate(&(target->lock), LOCK_READ);
   return e_no_receiver;
   
msg_test_receiver_failure:
   unlock_gate(&(target->lock), LOCK_READ);
   return e_failure;
   
   /* unlock and escape with success */
msg_test_receiver_success:
   unlock_gate(&(target->lock), LOCK_READ);
   return success;
}

/* msg_find_receiver
   Identify a potential receiver of a message sent by a thread.
   => sender = thread trying to send the message
      msg = message block trying to be sent
   <= pointer to thread to send the message to, or NULL for none
*/
thread *msg_find_receiver(thread *sender, diosix_msg_info *msg)
{
   process *proc;
   thread *recv;
   
   /* start with basic checks */
   if(!msg) return NULL;
   proc = proc_find_proc(msg->pid);
   if(!proc) return NULL;
   
   MSG_DEBUG("[msg:%i] trying to find a receiver, sender %p msg %p target=[tid %i pid %i]\n",
             CPU_ID, sender, msg, msg->tid, msg->pid);
   
   /* if a specific tid is given, then try that one */ 
   if(msg->tid != DIOSIX_MSG_ANY_THREAD)
   {
      recv = thread_find_thread(proc, msg->tid);
      if(msg_test_receiver(sender, recv, msg) == success) return recv;
   }
   else
   {
      /* otherwise search the targetted process for a thread that's blocking on
       receive */
      unsigned int loop;
      
      /* protect us from process table changes */
      lock_gate(&proc_lock, LOCK_READ);
      
      for(loop = 0; loop < THREAD_HASH_BUCKETS; loop++)
      {
         recv = proc->threads[loop];

         while(recv)
         {
            /* only check threads that are actually waiting for a msg */
            if((recv->state == waitingforreply) ||
               (recv->state == waitingformsg))
               if(msg_test_receiver(sender, recv, msg) == success)
               {
                  unlock_gate(&proc_lock, LOCK_READ);
                  return recv;
               }
            recv = recv->hash_next;
         }
      }

      unlock_gate(&proc_lock, LOCK_READ);
   }
   
   /* fall through to returning with nothing */
   return NULL;
}

/* msg_copy
   Copy message data from a sender into a receiver's buffer
   => receiver = thread to receive the data
      data = pointer to start of data to copy
      size = number of bytes to copy
      offset = pointer to word containing the offset into the receiver's buffer to start writing
               NB: this function updates this word to contain the offset after the copied data
      sender = thread sending the message
   <= 0 for success, or an error code
*/
kresult msg_copy(thread *receiver, void *data, unsigned int size, unsigned int *offset, thread *sender)
{
   diosix_msg_info *rmsg;
   unsigned int recv;
   unsigned int recv_base;
   
   /* sanity checks - no NULL pointers or zero-byte copies */
   if(!receiver || !data || !sender) return e_bad_params;
   if(!size) return success; /* copying zero bytes also succeeds */
   
   /* protect us from metadata changes */
   if(lock_gate(&(receiver->lock), LOCK_READ)) return e_failure;
   
   rmsg = &(receiver->msg);
   recv = (unsigned int)rmsg->recv;
   if(!recv)
   {
      /* protect us from NULL pointers */
      unlock_gate(&(receiver->lock), LOCK_READ);
      return e_bad_address;
   }
   recv_base = recv + (*(offset));
   
   /* stop abusive processes trying to smash out of a recv buffer */
   if((size > DIOSIX_MSG_MAX_SIZE) ||
      ((recv_base + size) > (recv + rmsg->recv_max_size)))
   {
      unlock_gate(&(receiver->lock), LOCK_READ);
      return e_too_big;
   }
   
   lock_gate(&(sender->lock), LOCK_READ);
   
   /* hand it over to the vmm to copy process-to-process - it'll sanity check the addresses */
   if(vmm_memcpyuser((void *)recv_base, receiver->proc, data, sender->proc, size))
   {      
      unlock_gate(&(sender->lock), LOCK_READ);
      unlock_gate(&(receiver->lock), LOCK_READ);
      return e_bad_address;
   }

   /* update offset */
   *(offset) += size;
   
   unlock_gate(&(sender->lock), LOCK_READ);
   unlock_gate(&(receiver->lock), LOCK_READ);
   return success;
}

/* msg_share_mem
   Link a virtual memory area in one process with another process through the inter-process
   messaging system. The entire vma must be linked and it must not collide with any other vmas.
   => target = process the vma will be linked with
      target_mem = structure describing where to link the vma in the target
      source = process the vma is already linked with
      source_mem = structure describing where to fidn the vma to link
   <= 0 for success, or an error code
*/
kresult msg_share_mem(process *target, diosix_share_request *target_mem,
                      process *source, diosix_share_request *source_mem)
{
   vmm_tree *source_node, *target_node;
   kresult result;
   
   /* sanity checks - stop null pointers and zero-sized mappings */
   if(!target || !source || !target_mem->size || !target_mem->base ||
      !source_mem->size || !source_mem->base)
      return e_bad_params;
   
   /* the two processes must agree on a size for the mapping */
   if(target_mem->size != source_mem->size) return e_bad_params;
   
   /* the processes cannot attempt to map over the kernel */
   if(target_mem->base + MEM_CLIP(target_mem->base, target_mem->size) >= KERNEL_SPACE_BASE)
      return e_bad_target_address;
   if(source_mem->base + MEM_CLIP(source_mem->base, source_mem->size) >= KERNEL_SPACE_BASE)
      return e_bad_source_address;

   lock_gate(&(source->lock), LOCK_READ);
   lock_gate(&(target->lock), LOCK_READ);

   source_node = vmm_find_vma(source, source_mem->base, sizeof(char));

   /* check to see if the vma exists in the source */
   if(source_node)
   {
      lock_gate(&(source_node->area->lock), LOCK_WRITE);

      /* the vma size and location must match with what the source process claims */
      if((source_node->base == source_mem->base) &&
         (source_node->area->size == source_mem->size))
      {
         /* this is the vma we are trying to link */
         vmm_area *vma = source_node->area;
         
         /* check for vma collision in the target process */
         target_node = vmm_find_vma(target, target_mem->base, target_mem->size);
         if(!target_node)
         {
            /* no collision - we are green for go */
            result = vmm_link_vma(target, target_mem->base, vma);
            
            /* Discovery has cleared the tower */
            MSG_DEBUG("[msg:%i] sharing vma %p (base %x size %i) in process %i with "
                      "process %i at %x\n [result = %i]",
                      CPU_ID, vma, source_mem->base, source_mem->size, source->pid,
                      target->pid, target_mem->base, result);
         }
         else result = e_bad_target_address;
      }
      
      unlock_gate(&(source_node->area->lock), LOCK_WRITE);
   }
   else result = e_bad_source_address;

   unlock_gate(&(target->lock), LOCK_READ);
   unlock_gate(&(source->lock), LOCK_READ);
      
   return result;
}

/* msg_send
   Send a basic diosix IPC message to a thread within another process.
   the kernel doesn't care about the contents of the message. threads
   can only send messages to listening threads (those blocked on msg_recv())
   in layers below them. this call will unschedule the calling thread
   so that it will block until a reply via msg_send() is sent from the receiver
   => sender = thread trying to send the message
      msg = message block
   <= 0 for success, or an error code
*/
kresult msg_send(thread *sender, diosix_msg_info *msg)
{
   thread *receiver;
   kresult err;
   unsigned int bytes_copied = 0;
   diosix_msg_info *rmsg;

   /* sanity check the msg data */
   if(!msg || !sender) return e_bad_address;
   
   /* identify the receiver */
   receiver = msg_find_receiver(sender, msg);
   if(!receiver) return e_no_receiver;

   /* protect us from changes to the receiver's memory structure */
   lock_gate(&(receiver->proc->lock), LOCK_READ);
   lock_gate(&(receiver->lock), LOCK_WRITE);
   
   rmsg = &(receiver->msg);

   /* sanatise the receiver's msg buffer we're about to use */
   if(pg_preempt_fault(receiver, (unsigned int)(rmsg->recv), rmsg->recv_max_size, VMA_WRITEABLE))
   {
      unlock_gate(&(receiver->lock), LOCK_WRITE);
      unlock_gate(&(receiver->proc->lock), LOCK_READ); /* give up the process lock */
      
      /* let the receiver know about its buffer screw up */
      syscall_post_msg_recv(receiver, e_bad_target_address);
                     
      MSG_DEBUG("[msg:%i] receiver %p (tid %i pid %i) tried to use invalid address %p for its receive buffer\n",
                CPU_ID, receiver, receiver->tid, receiver->proc->pid, rmsg->recv);
      
      return e_bad_target_address; /* bail out now if the receiver's screwed */
   }
   
   /* protect us from changes to the sender thread */
   lock_gate(&(sender->lock), LOCK_WRITE);
   
   /* copy the message data */
   if(msg->flags & DIOSIX_MSG_MULTIPART)
   {
      /* gather the multipart message blocks */
      unsigned int loop;
      
      diosix_msg_multipart *parts = msg->send;
      
      /* check that the multipart pointer isn't bogus */
      if((unsigned int)parts + MEM_CLIP(parts, msg->send_size * sizeof(diosix_msg_multipart)) >= KERNEL_SPACE_BASE)
      {
         unlock_gate(&(sender->lock), LOCK_WRITE);
         unlock_gate(&(receiver->lock), LOCK_WRITE);
         unlock_gate(&(receiver->proc->lock), LOCK_READ); /* give up the process lock */
         return e_bad_address;
      }
      
      /* do the multipart copy */
      for(loop = 0; loop < msg->send_size; loop++)
      {
         err = msg_copy(receiver, parts[loop].data, parts[loop].size, &bytes_copied, sender);
         if(err || (bytes_copied > DIOSIX_MSG_MAX_SIZE)) break;
      }
   }
   else
      /* do a simple message copy */
      err = msg_copy(receiver, msg->send, msg->send_size, &bytes_copied, sender);
      
   if(err)
   {
      unlock_gate(&(sender->lock), LOCK_WRITE);
      unlock_gate(&(receiver->lock), LOCK_WRITE);
      unlock_gate(&(receiver->proc->lock), LOCK_READ); /* give up the process lock */
      return err;
   }
   
   /* update the sender's and receiver's message block*/
   msg->pid = receiver->proc->pid;
   msg->tid = receiver->tid;
   
   rmsg->recv_size = bytes_copied;
   rmsg->pid = sender->proc->pid;
   rmsg->tid = sender->tid;
   
   /* was the send a reply or an actual send? */
   if(msg->flags & DIOSIX_MSG_REPLY)
   {
      /* restore the replier's priority if it was bumped up to send this reply */
      sender->priority_granted = SCHED_PRIORITY_INVALID;
      sched_priority_calc(sender, priority_check);
      
      /* check to see if the two conversing processes want to share memory */
      if(rmsg->flags & DIOSIX_MSG_SHAREVMA)
      {
         if(msg->flags & DIOSIX_MSG_SHAREVMA)
         {
            kresult err = msg_share_mem(receiver->proc, &(rmsg->mem_req),
                                        sender->proc, &(msg->mem_req));
            if(err)
            {
               /* clear the flag to indicate the share failed */
               msg->flags &= ~DIOSIX_MSG_SHAREVMA;
               rmsg->flags &= ~DIOSIX_MSG_SHAREVMA;
            }
         }
         else
            /* replier didn't want to share so clear the flag */
            rmsg->flags &= ~DIOSIX_MSG_SHAREVMA;
      }
   }
   else
   {  
      /* if the sent message wasn't a reply, block sending thread to wait for a reply */
      sched_remove(sender, waitingforreply);
      sender->replysource = receiver;
      
      /* take a copy of the message block */
      vmm_memcpy(&(sender->msg), msg, sizeof(diosix_msg_info));
      
      /* did the message include a share request? */
      if(msg->flags & DIOSIX_MSG_SHAREVMA)
      {
         /* notify the receiver of the request, the context should be in the message payload */
         rmsg->mem_req.size = msg->mem_req.size;
         rmsg->mem_req.base = 0; /* don't reveal the requester's memory map */
         rmsg->flags |= DIOSIX_MSG_SHAREVMA;
      }
      
      /* bump the receiver's priority up if the sender has a higher priority to
         avoid priority inversion */
      if(sender->priority < receiver->priority)
         receiver->priority_granted = sender->priority;
      else
         receiver->priority_granted = SCHED_PRIORITY_INVALID;
      sched_priority_calc(receiver, priority_check);
   }

   unlock_gate(&(sender->lock), LOCK_WRITE);
   unlock_gate(&(receiver->lock), LOCK_WRITE);
   unlock_gate(&(receiver->proc->lock), LOCK_READ); /* give up the process lock */

   /* wake up the receiving thread */
   syscall_post_msg_recv(receiver, success);
   sched_add(receiver->cpu, receiver);
   
   MSG_DEBUG("[msg:%x] thread %i of process %i sent message %x (%i bytes first word %x) to thread %i of process %i\n",
             CPU_ID, sender->tid, sender->proc->pid, msg->send, msg->send_size, *((unsigned int *)msg->send),
             receiver->tid, receiver->proc->pid);
   
   return success;
}

/* msg_recv
   Block a thread until a message or signal comes in
   => receiver = thread waiting to receive
      info = block to fill with received message details
   <= success or an error code
*/
kresult msg_recv(thread *receiver, diosix_msg_info *msg)
{
   /* basic sanity checks */
   if(!receiver || !msg) return e_bad_address;
   if((unsigned int)msg + MEM_CLIP(msg, sizeof(diosix_msg_info)) >= KERNEL_SPACE_BASE)
      return e_bad_address;
   
   if(lock_gate(&(receiver->lock), LOCK_WRITE)) return e_failure;
   
   /* if receiving a signal, return immediately if one's queued and ready to go */
   if((msg->flags & DIOSIX_MSG_TYPEMASK) == DIOSIX_MSG_SIGNAL)
   {
      queued_signal *sig = NULL;
      kpool *pool = receiver->proc->system_signals; /* the default pool */
      
      /* check kernel and unix signals first */
      if(receiver->proc->system_signals)
      {
         /* are we looking for kernel-generated signals only? */
         if(msg->flags & DIOSIX_MSG_KERNELONLY)
         {
            sig = vmm_next_in_pool(sig, receiver->proc->system_signals);
            while(sig && (sig->sender_pid != RESERVED_PID))
            {
               sig = vmm_next_in_pool(sig, receiver->proc->system_signals);
               if(!sig) break; /* no suitable signals to be found */
            }
         }
         else sig = vmm_next_in_pool(sig, receiver->proc->system_signals);
      }

      /* if sig remains NULL then a system signal wasn't found... */
      if(!sig)
         /* ...so check for user signals */
         if(receiver->proc->user_signals)
         {
            sig = vmm_next_in_pool(NULL, receiver->proc->user_signals);
            pool = receiver->proc->user_signals;
         }
      
      /* if sig is set by now then a signal was found */
      if(sig)
      {
         /* fill in the signal details */
         receiver->msg.signal.number = sig->signal.number;
         receiver->msg.signal.extra = sig->signal.extra;
         receiver->msg.pid = sig->sender_pid;
         receiver->msg.tid = sig->sender_tid;
         
         /* free the block */
         vmm_free_pool(sig, pool);
         
         /* unlock and return immediately */
         MSG_DEBUG("[msg:%i] returned queued signal %i to tid %i pid %i\n",
                   CPU_ID, receiver->msg.signal.number, receiver->tid,
                   receiver->proc->pid);
         
         return success;
      }
   }
   
   /* grab a copy of the user's message block, we'll fault the thread if the address is dodgy */
   vmm_memcpy(&(receiver->msg), msg, sizeof(diosix_msg_info));
   
   unlock_gate(&(receiver->lock), LOCK_WRITE);

   /* remove receiver from the queue until a message comes in */
   sched_remove(receiver, waitingformsg);

   MSG_DEBUG("[msg:%i] tid %i pid %i now receiving (%p)\n",
             CPU_ID, receiver->tid, receiver->proc->pid, msg);

   return success;
}
