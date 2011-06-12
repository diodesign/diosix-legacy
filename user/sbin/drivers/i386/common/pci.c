/* user/sbin/drivers/i386/common/pci.c
 * Interface to the userspace PCI manager
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
#include "pci.h"

/* pci_prep_message
   Prepare a message to send to the PCI userspace manager
   => msg = diosix message header
      req = PCI manager request block
      reply = block to store reply in
*/
void pci_prep_message(diosix_msg_info *msg, pci_request_msg *req, pci_reply_msg *reply)
{
   memset(msg, 0, sizeof(diosix_msg_info));
   memset(req, 0, sizeof(pci_request_msg));
   
   /* set up a blocking message to the manager  */
   msg->role = DIOSIX_ROLE_PCIMANAGER;
   msg->pid = msg->tid = DIOSIX_MSG_ANY_THREAD;
   msg->flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_SENDASUSR | DIOSIX_MSG_QUEUEME;
   msg->send_size = sizeof(pci_request_msg);
   msg->send = req;
   msg->recv_max_size = sizeof(pci_reply_msg);
   msg->recv = reply;
   
   /* fill out the pci request message */
   req->bus_type = pci_bus;
   req->magic = PCI_MSG_MAGIC;
}

/* pci_read_config
   Read a 16-bit configuration word for the given PCI device
   => bus, slot, func = select the PCI device using its
                        bus, slot and function values
      offset = register number to read from the device
      result = store the value of the register at this word
               if the read was successful
   <= 0 for success, or an error code 
*/
kresult pci_read_config(unsigned short bus,
                        unsigned short slot,
                        unsigned short func,
                        unsigned short offset,
                        unsigned short *result)
{
   /* structures to hold the message */
   diosix_msg_info msg;
   pci_request_msg pci_msg;
   pci_reply_msg reply;
   kresult err;
   
   /* fill out the diosix message header to send
      the read request to any available thread in 
      the PCI manager process */
   pci_prep_message(&msg, &pci_msg, &reply);
   
   /* fill out the pci request message */
   pci_msg.req = read_config;
   
   /* here's the device we want to read */
   pci_msg.bus = bus;
   pci_msg.slot = slot;
   pci_msg.func = func;
   pci_msg.offset = offset;
   
   /* send the message and update the result variable
      if successful */
   err = diosix_msg_send(&msg);
   if(err) return err;
   
   if(reply.result == success)
      *result = reply.value & 0xffff;
   
   return reply.result;
}

/* pci_claim_device
   Claim exclusive ownership of a device to stop other device drivers from
   commandeering the PCI card
   => bus, slot = select the PCI device
      pid = PID of process to register, must be zero to indicate this process
   <= 0 for success, or an error code 
*/
kresult pci_claim_device(unsigned short bus, unsigned short slot, unsigned int pid)
{
   /* structures to hold the message */
   diosix_msg_info msg;
   pci_request_msg pci_msg;
   pci_reply_msg reply;
   kresult err;
   
   /* only allow the calling process to claim the device */
   if(pid && getpid() != pid) return e_no_rights;
   
   /* fill out the diosix message header to send
      the claim request to any available thread in 
      the PCI manager process */
   pci_prep_message(&msg, &pci_msg, &reply);
   
   /* fill out the pci request message */
   pci_msg.req = claim_device;
   
   /* here's the device we want to read */
   pci_msg.bus = bus;
   pci_msg.slot = slot;
   pci_msg.func = pci_msg.offset = 0;
   
   /* send the message and update the result variable
    if successful */
   err = diosix_msg_send(&msg);
   if(err) return err;

   return reply.result;
}

/* pci_release_device
   Give up exclusive ownership of a device
   => bus, slot = select the PCI device
   <= 0 for success, or an error code 
*/
kresult pci_release_device(unsigned short bus, unsigned short slot)
{
   /* structures to hold the message */
   diosix_msg_info msg;
   pci_request_msg pci_msg;
   pci_reply_msg reply;
   kresult err;

   /* fill out the diosix message header to send
    the claim request to any available thread in 
    the PCI manager process */
   pci_prep_message(&msg, &pci_msg, &reply);
   
   /* fill out the pci request message */
   pci_msg.req = release_device;
   
   /* here's the device we want to read */
   pci_msg.bus = bus;
   pci_msg.slot = slot;
   pci_msg.func = pci_msg.offset = 0;
   
   /* send the message and update the result variable
    if successful */
   err = diosix_msg_send(&msg);
   if(err) return err;
   
   return reply.result;
}

/* pci_find_device
   Look up the bus and slot numbers of a detected device from its class
   => class = high byte is the class, low byte is the sub-class
      count = for a machine with multiple cards with the same class+subclass,
              this index selects the required card (starting from 0)
      bus, slot = pointers to variables to write bus and slot numbers in, if successful
      pid = pointer to store PID of owning process, or 0 if none, if successful
   <= 0 for success, or an error code
*/
kresult pci_find_device(unsigned short class, unsigned char count,
                        unsigned short *bus, unsigned short *slot, unsigned int *pid)
{
   /* structures to hold the message */
   diosix_msg_info msg;
   pci_request_msg pci_msg;
   pci_reply_msg reply;
   kresult err;
      
   /* fill out the diosix message header to send
    the claim request to any available thread in 
    the PCI manager process */
   pci_prep_message(&msg, &pci_msg, &reply);
   
   /* fill out the pci request message */
   pci_msg.req = find_device;
   
   /* here's the device we want to find */
   pci_msg.class = class;
   pci_msg.count = count;
   
   /* send the message and update the result variable
      if successful - retry sending the request if
      the PCI manager is unavailable, it may have not
      started yet */
   err = diosix_msg_send(&msg);
   if(err) return err;

   /* give up if the manager gave us an error */
   if(reply.result) return reply.result;

   /* write back the found device's details */
   *bus = reply.bus;
   *slot = reply.slot;
   *pid = reply.pid;
   return success;
}
