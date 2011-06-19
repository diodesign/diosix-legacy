/* user/sbin/drivers/i386/common/nic.h
 * Defines for interfacing with Interface to a userspace NIC driver
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

#define NIC_MSG_MAGIC (0xd1000003)

/* define the types of request */
typedef enum
{
   read_packet = 0,
   write_packet,
   config
} nic_req_type;

/* the main request structure */
typedef struct
{
   /* define the request */
   pci_req_type type;
   unsigned int magic;
   
} diosix_nic_req;

/* the main reply structure */
typedef struct
{
   kresult result; /* return code */
} diosix_nic_reply;

#endif
