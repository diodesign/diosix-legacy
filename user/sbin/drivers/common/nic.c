/* user/sbin/drivers/common/nic.c
 * Interface to a userspace NIC driver
 * Author : Chris Williams
 * Date   : Tue,7 Dec 2010.06:55:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "diosix.h"
#include "functions.h"
#include "roles.h"
#include "nic.h"

/* nic_send_to_stack
   Send a message to the networking stack and return with a reply
   => type = type of the request to send
      data = pointer to payload to send with the message
      reply = pointer to structure to store the reply from the stack
   <= 0 for succesful message exchange, or a failure code
*/
kresult nic_send_to_stack(nic_req_type type, void *data, diosix_nic_reply *reply)
{
   diosix_msg_info msg;
   diosix_nic_req req;
   
   req.type = type;
   req.magic = NIC_MSG_MAGIC;
   
   switch(type)
   {
      case nic_announce_device:
         memcpy(&(req.data.announce_req), data, sizeof(diosix_nic_announce_req));
         break;
         
      case nic_share_buffer:
      case nic_lock_buffer:
      case nic_unlock_buffer:
         memcpy(&(req.data.buffer_req), data, sizeof(diosix_nic_buffer_req));
         break;
         
      default:
         return e_bad_params;
   }
   
   msg.role = DIOSIX_ROLE_NETWORKSTACK;
   msg.pid = msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_SENDASUSR | DIOSIX_MSG_QUEUEME;
   msg.send_size = sizeof(diosix_nic_req);
   msg.send = &req;
   msg.recv_max_size = sizeof(diosix_nic_reply);
   msg.recv = reply;

   return diosix_msg_send(&msg);
}



