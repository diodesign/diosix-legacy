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

#include <stdlib.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

/* default size of the initial receive buffer */
#define RECEIVE_BUFFER_SIZE   (2048)

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
      
      if(msg.recv_size < sizeof(diosix_vfs_request_head))
      {
         /* malformed request, it's too small to even hold a request type */
         reply_to_request(&msg, e_too_small);
         return;
      }
      
      /* decode the request type */
      switch(req_head->type)
      {
         case chown_req:
         case close_req:
         case fstat_req:
         case link_req:
         case lseek_req:
         case open_req:
         case read_req:
         case readlink_req:
         case stat_req:
         case symlink_req:
         case unlink_req:
         case write_req:
         case register_req:
         case deregister_req:
            reply_to_request(&msg, success);
            break;
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params);
      }
   }
}

int main(void)
{   
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_VFS);

   /* fork so that we have multiple worker threads */
   diosix_thread_fork();

   /* get some work to do */
   while(1) wait_for_request();
}
