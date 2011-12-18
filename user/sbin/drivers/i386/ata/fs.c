/* user/sbin/drivers/i386/ata/fs.c
 * Driver for ATA-connected storage devices 
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.22:54:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "ata.h"

#define RECEIVE_BUFFER_SIZE   (2048)

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
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
            
      if(msg.recv_size < sizeof(diosix_vfs_request_head))
      {
         /* malformed request, it's too small to even hold a request type */
         reply_to_request(&msg, e_too_small);
         return;
      }
      
      /* sanity check the message header */
      if(req_head->magic != VFS_MSG_MAGIC)
      {
         /* malformed request, bad magic */
         reply_to_request(&msg, e_bad_magic);
         return;
      }
            
      /* decode the request type */
      switch(req_head->type)
      {
         /* discard requests that aren't appropriate */
         case link_req:
         case readlink_req:
         case unlink_req:
         case symlink_req:
         case lseek_req:
         case register_req:
         case deregister_req:
            
         /* discard requests that aren't implemented (yet) */
         case chown_req:
         case fstat_req:
         case stat_req:
            reply_to_request(&msg, e_no_handler);
            break;

         /* open device exclusively for root only */
         case open_req:
            printf("open request from pid %i!\n", msg.pid);
            reply_to_request(&msg, success);
            break;

         case read_req:
            printf("read request from pid %i!\n", msg.pid);
            reply_to_request(&msg, e_failure);
            break;
            
         case close_req:
         case write_req:
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params);
      }
   }
}
