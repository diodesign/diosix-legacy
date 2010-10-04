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

/* msg_send_signal
   Send a POSIX signal message to a process - this function will not block.
   It queues a signal message for the process and wakes up the thread that's
   supposed to be handling it, then returns. If no signal handler can be found
   then alert the caller.
   => target  = pointer to receiving process
      signum  = signal code to send
      sigcode = additional reason code
   <= 0 for success or an error reason code */
kresult msg_send_signal(process *target, unsigned int signum, unsigned int sigcode)
{
   return e_no_handler;
}

/* msg_test_receiver
   Check if a given thread is capable of receiving the given message
   => sender = threading trying to send the message
      target = thread message is trying to be sent to
      msg = message block trying to be sent
   <= 0 for success, or an error code
*/
kresult msg_test_receiver(thread *sender, thread *target, diosix_msg_info *msg)
{
   diosix_msg_info *tmsg;
   
   /* sanity check */
   if(!sender || !target || !msg)
   {
      KOOPS_DEBUG("[msg:%i] OMGWTF msg_test_receiver() called with sender %p target %p msg %p\n",
                  CPU_ID, sender, target, msg);
      return e_failure;
   }

   MSG_DEBUG("[msg:%i] testing receiver %p (tid %i pid %i buffer %p) for sender %p (tid %i pid %i msg %p)\n",
             CPU_ID, target, target->tid, target->proc->pid, target->msg->recv, sender, sender->tid, sender->proc->pid, msg);
   
   /* protect us from changes to the target's metadata */
   lock_gate(&(target->lock), LOCK_READ);
   
   /* threads can only recieve messages if they are in a layer below the sender - unless it's a 
      reply. we check below whether this is a legit reply */
   if((target->proc->layer > sender->proc->layer) && !(msg->flags & DIOSIX_MSG_REPLY))
   {
      MSG_DEBUG("[msg:%i] recv layer %i not below sender layer %i and message isn't a reply (%i)\n",
                CPU_ID, target->proc->layer, sender->proc->layer, msg->flags);
      goto msg_test_receiver_failure;
   }
   
   /* has the target thread given us a sane message block to access? */
   if(pg_preempt_fault(target, (unsigned int)(target->msg), sizeof(diosix_msg_info), VMA_WRITEABLE))
      goto msg_test_receiver_failure;
   
   /* is this message waiting on a reply from this thread? */
   if((target->state == waitingforreply) &&
      (msg->flags & DIOSIX_MSG_REPLY) &&
      (target->replysource == sender) &&
      target->msg)
      goto msg_test_receiver_success;
   
   /* is the target thread willing to accept the message type? */
   if(target->msg)
   {
      /* don't forget that within the context of the sending thread we can't access the
       receiver's msg structure unless we go via a kernel mapping.. */
      if(pg_user2kernel((unsigned int *)&tmsg, (unsigned int)(target->msg), target->proc))
         goto msg_test_receiver_failure;      
      
      if((target->state == waitingformsg) &&
         ((msg->flags & DIOSIX_MSG_TYPEMASK) & tmsg->flags))
         goto msg_test_receiver_success;
      
      MSG_DEBUG("[msg:%i] something's not right with the receiver: state %i (should be %i) msg type %x recv type %x\n",
                CPU_ID, target->state, waitingformsg, msg->flags & DIOSIX_MSG_TYPEMASK, tmsg->flags);
   }
   
   /* give up */
   unlock_gate(&(target->lock), LOCK_READ);
   MSG_DEBUG("[msg:%i] gave up testing receiver\n", CPU_ID);
   return e_no_receiver;
   
msg_test_receiver_failure:
   unlock_gate(&(target->lock), LOCK_READ);
   MSG_DEBUG("[msg:%i] receiver failed its test\n", CPU_ID);
   return e_failure;
   
   /* unlock and escape with success */
msg_test_receiver_success:
   unlock_gate(&(target->lock), LOCK_READ);
   MSG_DEBUG("[msg:%i] receiver passed its test\n", CPU_ID);
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
   
   MSG_DEBUG("[msg:%i] trying to find a receiver, sender=[tid %i pid %i] msg %p target=[tid %i pid %i]\n",
             CPU_ID, sender->tid, sender->proc->pid, msg, msg->tid, msg->pid);
   
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
   
   /* protect us from metadata changes */
   lock_gate(&(receiver->lock), LOCK_READ);
   
   /* don't forget that within the context of the sending thread we can't access the
      receiver's msg structure unless we go via a kernel mapping - it'll also sanity
      check the addresses for us */
   if(pg_user2kernel((unsigned int *)&rmsg, (unsigned int)(receiver->msg), receiver->proc))
   {
      unlock_gate(&(receiver->lock), LOCK_READ);
      return e_bad_target_address;
   }

   recv = (unsigned int)rmsg->recv;
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
   
   /* get the receiver thread's msg block - msg_find_receiver()
      double-checked it was ok to use the msg block */
   if(pg_user2kernel((void *)&rmsg, (unsigned int)receiver->msg, receiver->proc))
   {
      unlock_gate(&(receiver->lock), LOCK_WRITE);
      unlock_gate(&(receiver->proc->lock), LOCK_READ); /* give up the process lock */
      
      /* TODO: take action against the at-fault receiver process */
      MSG_DEBUG("[msg:%i] receiver %p (tid %i pid %i) tried to use invalid address %p for its receive block\n",
                CPU_ID, receiver, receiver->tid, receiver->proc->pid, receiver->msg);
      
      return e_bad_target_address; /* this shouldn't really happen */
   }

   /* sanatise the receiver's msg buffer we're about to use */
   if(pg_preempt_fault(receiver, (unsigned int)(rmsg->recv), rmsg->recv_max_size, VMA_WRITEABLE))
   {
      unlock_gate(&(receiver->lock), LOCK_WRITE);
      unlock_gate(&(receiver->proc->lock), LOCK_READ); /* give up the process lock */
      
      /* TODO: take action against the at-fault receiver process */
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
      /* restore the receiver's priority if it was bumped up to send this reply */
      receiver->priority_granted = SCHED_PRIORITY_INVALID;
      sched_priority_calc(receiver, priority_check);
   }
   else
   {  
      /* if the sent message wasn't a reply, block sending thread to wait for a reply */
      sched_remove(sender, waitingforreply);
      sender->replysource = receiver;
      
      /* take a copy of the message block ptr */
      sender->msg = msg;
      
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
   if(!(msg->recv) || !(msg->recv_max_size)) return e_bad_address;
   
   /* keep a copy of this pointer */
   lock_gate(&(receiver->lock), LOCK_WRITE);
   receiver->msg = msg;
   unlock_gate(&(receiver->lock), LOCK_WRITE);

   /* remove receiver from the queue until a message comes in */
   sched_remove(receiver, waitingformsg);

   MSG_DEBUG("[msg:%i] tid %i pid %i now receiving (%p buffer %p)\n",
             CPU_ID, receiver->tid, receiver->proc->pid, msg, receiver->msg->recv);

   return success;
}
