/* user/sbin/drivers/common/nic.h
 * Defines for interfacing with a userspace NIC driver
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.23:21:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _NIC_H
#define _NIC_H 1

#include "diosix.h" /* for kresult */

#define NIC_MSG_MAGIC   (0xd1000003)
#define NIC_MAC_LENGTH  (6)

#define NIC_NAME_LENGTH (6)

/* define the types of request */
typedef enum
{
   nic_announce_device = 0,
   nic_share_buffer,
   nic_lock_buffer,
   nic_unlock_buffer
} nic_req_type;

typedef enum
{
   nic_send_buffer = 0,
   nic_recv_buffer
} nic_buffer_type;

/* describe a NIC device in terms of buffers etc */
typedef struct
{
   unsigned char id; /* per-driver process device id */
   
   /* interface name of the device */
   char name[NIC_NAME_LENGTH];

   /* describe the buffer set up of the device */
   unsigned char send_buffers, recv_buffers;
   unsigned int send_buffer_size, recv_buffer_size;
} diosix_nic_announce_req;

typedef struct
{
   nic_buffer_type type;
   unsigned char buffer_nr;
   unsigned short buffer_size; /* used in unlocking a send */
} diosix_nic_buffer_req;

/* the main request structure */
typedef struct
{
   /* define the request */
   nic_req_type type;
   unsigned int magic;
   
   union
   {
      diosix_nic_announce_req announce_req;
      diosix_nic_buffer_req buffer_req;
   } data;

} diosix_nic_req;

/* the main reply structure */
typedef struct
{
   kresult result; /* return code */
   unsigned char buffer_nr; /* returned buffer number (if required) */
} diosix_nic_reply;


/* prototypes */
kresult nic_send_to_stack(nic_req_type type, void *data, diosix_nic_reply *reply);

#endif
