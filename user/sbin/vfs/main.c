/* user/sbin/vfs/main.c
 * Core functions for the virtual filesystem process
 * Author : Chris Williams
 * Date   : Sun,19 Dec 2010.04:27:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "vfs.h"

/* ------------------------------------------
   message interface with other processes
   ------------------------------------------ */

/* reply_to_request
   Send a reply to a vfs request
   => msg = message block to use, will contain
            the results from a diosix_msg_receive()
            that picked up the request.
      result = result of the request
*/
void reply_to_request(diosix_msg_info *msg, kresult result)
{
   diosix_vfs_reply reply;
   
   /* sanity check */
   if(!msg) return;
   
   /* ping a reply back to unlock the requesting thread */
   reply.result = result;
   msg->send = &reply;
   msg->send_size = sizeof(diosix_vfs_reply);
   
   diosix_msg_reply(msg);
}

/* reply_to_pid_request
   Send a reply to a vfs request that requires a PID and/or
   file handle in its reply. Assumes successful request.
   => msg = message block to use, will contain
            the results from a diosix_msg_receive()
            that picked up the request.
      reply = pid+filesdesc block to send back
 */
void reply_to_pid_request(diosix_msg_info *msg, diosix_vfs_pid_reply *reply)
{   
   /* sanity check */
   if(!msg || !reply) return;
   
   /* ping a pid + file reply back to unlock the requesting thread */
   reply->result = success;
   msg->send = reply;
   msg->send_size = sizeof(diosix_vfs_pid_reply);
   
   diosix_msg_reply(msg);
}

/* wait_for_request
   Block until a request comes in */
void wait_for_request(void)
{
   diosix_msg_info msg;
   unsigned char recv_buffer[RECEIVE_BUFFER_SIZE];
   
   memset(&msg, 0, sizeof(diosix_msg_info));
   
   /* wait for a generic message to come in */
   msg.flags = DIOSIX_MSG_GENERIC;
   msg.recv = recv_buffer;
   msg.recv_max_size = RECEIVE_BUFFER_SIZE;
   
   if(diosix_msg_receive(&msg) == success)
   {
      diosix_vfs_request_head *req_head = (diosix_vfs_request_head *)recv_buffer;

      /* we cannot trust anything in the message payload */
      if(VFS_MSG_MIN_SIZE_CHECK(msg, 0))
      {
         /* malformed request, it's too small */
         reply_to_request(&msg, e_too_small);
         return;
      }
      
      /* decode the request type */
      switch(req_head->type)
      {
         /* request to open a file */
         case open_req:
         {
            kresult result;
            diosix_vfs_pid_reply reply;

            /* extract the request data from the message payload */
            diosix_vfs_request_open *req_info = VFS_MSG_EXTRACT(req_head, 0);
            char *path = VFS_MSG_EXTRACT(req_head, sizeof(diosix_vfs_request_open));
            
            /* the payload might not be big enough to contain the request details */
            if(VFS_MSG_MIN_SIZE_CHECK(msg, sizeof(diosix_vfs_request_open)))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }

            /* we cannot trust the string length parameter in the payload, so check it */
            if(VFS_MSG_MAX_SIZE_CHECK(msg, sizeof(diosix_vfs_request_open), req_info->length))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }

            /* is the path zero-terminated? */
            if(path[req_info->length - 1])
            {
               reply_to_request(&msg, e_bad_params);
               return;
            }

            /* all seems clear */
            result = open_file(&msg, req_info->flags, req_info->mode, path, &reply);
            
            if(result == success)
               reply_to_pid_request(&msg, &reply);
            else
               reply_to_request(&msg, result);
         }
         break;

         /* request to register a filesystem/device in the vfs filespace */
         case register_req:
         {
            kresult result;

            /* extract the request's data from the message payload */
            diosix_vfs_request_register *req_info = VFS_MSG_EXTRACT(req_head, 0);
            char *path = VFS_MSG_EXTRACT(req_head, sizeof(diosix_vfs_request_register));

            /* the payload might not be big enough to contain the request details */
            if(VFS_MSG_MIN_SIZE_CHECK(msg, sizeof(diosix_vfs_request_register)))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }

            /* we cannot trust the string length parameter in the payload, so check it */
            if(VFS_MSG_MAX_SIZE_CHECK(msg, sizeof(diosix_vfs_request_register), req_info->length))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            /* is the path zero-terminated? */
            if(path[req_info->length - 1])
            {
               reply_to_request(&msg, e_bad_params);
               return;
            }
            
            /* all seems clear */
            result = register_process(&msg, path);
            reply_to_request(&msg, result);
         }
         break;

         /* deregister a filesystem/device in the vfs filespace */
         case deregister_req:
         {
            kresult result;
            diosix_vfs_request_register *req_info = VFS_MSG_EXTRACT(req_head, 0);
            char *path = (char *)VFS_MSG_EXTRACT(req_head, sizeof(diosix_vfs_request_register));
            
            /* the payload might not be big enough to contain the request details */
            if(VFS_MSG_MIN_SIZE_CHECK(msg, sizeof(diosix_vfs_request_register)))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            /* we cannot trust the string length parameter in the payload, so check it */
            if(VFS_MSG_MAX_SIZE_CHECK(msg, sizeof(diosix_vfs_request_register), req_info->length))
            {
               reply_to_request(&msg, e_too_big);
               return;
            }
            
            /* is the path zero-terminated? */
            if(path[req_info->length - 1])
            {
               reply_to_request(&msg, e_bad_params);
               return;
            }
            
            result = deregister_process(&msg, path);
            reply_to_request(&msg, result);
         }
         break;
            
         case read_req:
         {
            kresult result = success;
            diosix_debug_write("read requested!\n");
            reply_to_request(&msg, result);
         }
         break;
            
         case chown_req:
         case close_req:
         case fstat_req:
         case link_req:
         case lseek_req:
         case readlink_req:
         case stat_req:
         case symlink_req:
         case unlink_req:
         case write_req:
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params);
      }
   }
}

/* ------------------------------------------
   greetings, program
   ------------------------------------------ */

int main(void)
{
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_VFS);

   /* step into the correct layer - layer 3 */
   diosix_priv_layer_up(3);
   
   /* create some worker threads */
   diosix_thread_fork();
   diosix_thread_fork();
   
   /* get some work to do */
   while(1) wait_for_request();
}
