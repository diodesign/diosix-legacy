/* user/sbin/drivers/i386/common/pci.h
 * Defines for interfacing with the PCI bus from userspace
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.23:21:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _PCI_H
#define _PCI_H 1

#include "diosix.h" /* for kresult */

#define PCI_CONFIG_ADDRESS (0xcf8)
#define PCI_CONFIG_DATA    (0xcfc)

/* PCI message definitions */
#define PCI_MSG_MAGIC  (0xd1000002)

/* define the type of bus */
typedef enum
{
   pci_bus = 0 /* conventional/vanilla PCI */
} pci_bus_type;

/* define the types of request */
typedef enum
{
   read_config,
   claim_device,
   release_device
} pci_req_type;

/* the main request structure */
typedef struct
{
   /* define the request */
   pci_bus_type bus_type;
   pci_req_type req;
   unsigned int magic;
   
   /* common details for all requests */
   unsigned short bus, slot;

   /* used for read_config requests */
   unsigned short func, offset;
} pci_request_msg;

/* the main reply structure */
typedef struct
{
   kresult result; /* return code */
   unsigned int value; /* any extra data */
} pci_reply_msg;

/* prototypes */
kresult pci_read_config(unsigned short bus, unsigned short slot, unsigned short func, unsigned short offset, unsigned short *result);
kresult pci_claim_device(unsigned short bus, unsigned short slot, unsigned int pid);
kresult pci_release_device(unsigned short bus, unsigned short slot);

#endif
