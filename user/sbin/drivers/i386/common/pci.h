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

/* IO port access */
#define PCI_CONFIG_ADDRESS    (0xcf8)
#define PCI_CONFIG_DATA       (0xcfc)

/* PCI config header bits */
#define PCI_HEADER_MULTIFUNC  (1 << 7)

/* PCI config header types */
#define PCI_HEADER_GENERIC    (0)
#define PCI_HEADER_PCI2PCI    (1)
#define PCI_HEADER_CARDBUS    (2)

/* PCI config header values */
#define PCI_HEADER_VENDORID   (0x00)
#define PCI_HEADER_DEVICEID   (0x02)
#define PCI_HEADER_CLASS      (0x0a)
#define PCI_HEADER_TYPE       (0x0e)
#define PCI_HEADER_BAR0_LOW   (0x10)
#define PCI_HEADER_BAR0_HIGH  (PCI_HEADER_BAR1_LOW + 2)
#define PCI_HEADER_BAR1_LOW   (0x14)
#define PCI_HEADER_BAR1_HIGH  (PCI_HEADER_BAR2_LOW + 2)
#define PCI_HEADER_BAR2_LOW   (0x18)
#define PCI_HEADER_BAR2_HIGH  (PCI_HEADER_BAR3_LOW + 2)
#define PCI_HEADER_BAR3_LOW   (0x1c)
#define PCI_HEADER_BAR3_HIGH  (PCI_HEADER_BAR4_LOW + 2)
#define PCI_HEADER_BAR4_LOW   (0x20)
#define PCI_HEADER_BAR4_HIGH  (PCI_HEADER_BAR5_LOW + 2)
#define PCI_HEADER_BAR5_LOW   (0x24)
#define PCI_HEADER_BAR5_HIGH  (PCI_HEADER_BAR6_LOW + 2)
#define PCI_HEADER_INT_LINE   (0x3c)
#define PCI_HEADER_INT_PIN    (0x3d)

/* PCI class and sub-classes */
#define PCI_CLASS_CALC(c, s) (unsigned short)((((c) & 0xff) << 8) | ((s) & 0xff))
#define PCI_CLASS_MASSSTORAGE (0x01)
#define PCI_SUBCLASS_IDE      (0x01)

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
   read_config = 0,
   claim_device,
   release_device,
   find_device
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
   
   /* used for find_device requests */
   unsigned short class;
   unsigned char count;
} pci_request_msg;

/* the main reply structure */
typedef struct
{
   kresult result; /* return code */
   
   /* read_config data */
   unsigned int value;
   
   /* find_device results */
   unsigned short bus, slot;
   unsigned int pid;
} pci_reply_msg;

/* prototypes */
kresult pci_read_config(unsigned short bus, unsigned short slot, unsigned short func, unsigned short offset, unsigned short *result);
kresult pci_claim_device(unsigned short bus, unsigned short slot, unsigned int pid);
kresult pci_release_device(unsigned short bus, unsigned short slot);
kresult pci_find_device(unsigned short class, unsigned char count, unsigned short *bus, unsigned short *slot, unsigned int *pid);

#endif
