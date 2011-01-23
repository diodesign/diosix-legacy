/* user/sbin/drivers/i386/pcimngr/main.c
 * Driver for the PCI bus 
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.22:54:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "lowlevel.h"
#include "pci.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* define a registered PCI device */
typedef struct pci_device pci_device;
struct pci_device
{
   unsigned short bus, slot;
   unsigned int pid;
   
   /* hash table links */
   pci_device *prev, *next;
};

#define PCI_HASH_MAX         (16)
#define PCI_HASH_INDEX(b, s) (((b) + (s)) % PCI_HASH_MAX)

/* hash table to store registered PCI devices */
pci_device *pci_hash_tbl[PCI_HASH_MAX];

/* ----------------------------------------------------------------------
   read data from a PCI device
   ---------------------------------------------------------------------- */
   
/* pci_read_config
   Read a 16-bit configuration word for the given PCI device
   => bus, slot, func = select the PCI device using its
                        bus, slot and function values
      offset = register number to read from the device
      result = store the value of the register at this word if the read was successful
   <= 0 for success, or an error code 
*/
kresult pci_read_config(unsigned short bus, unsigned short slot, unsigned short func, unsigned short offset, unsigned short *result)
{
   unsigned int address, data;

   /* sanitise the input */
   if(!result) return e_bad_params;
   bus &= 0xff;  /* 8 bits wide */
   slot &= 0x1f; /* 5 bites wide */
   func &= 0x07; /* 3 bits wide */

   /* augh the magic numbers! */
   address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc);
   
   /* select the required address */
   write_port_word(PCI_CONFIG_ADDRESS, address);
   
   /* and fetch the response */
   data = (unsigned short)(read_port_word(PCI_CONFIG_DATA) >> ((offset & 0x02) << 3)) & 0xffff;

   /* the PCI controller returns all 1s for a non-existent device */
   if(data == 0xffff) return e_not_found;
   
   /* save the result and return success */
   *result = data;
   return success;
}

/* ----------------------------------------------------------------------
   manage device ownership
   ---------------------------------------------------------------------- */

/* pci_claim_device
   Claim exclusive ownership of a device to stop other device drivers from
   commandeering the PCI card
   => bus, slot = select the PCI device
      pid = PID of the process claiming the device
   <= 0 for success, or an error code 
*/
kresult pci_claim_device(unsigned short bus, unsigned short slot, unsigned int pid)
{
   unsigned char index = PCI_HASH_INDEX(bus, slot);
   pci_device *device;
   pci_device *new = malloc(sizeof(pci_device));
   
   if(!new) return e_failure;
   
   /* fill out the details */
   new->bus = bus;
   new->slot = slot;
   new->pid = pid;
   
   /* add to the start of the hashed linked list */
   device = pci_hash_tbl[index];
   
   if(device)
   {
      device->prev = new;
      new->next = device;
   }
   else
      new->next = NULL;
   
   pci_hash_tbl[index] = new;
   new->prev = NULL;
   
   return success;
}

/* pci_release_device
   Give up exclusive ownership of a device
   => bus, slot = select the PCI device
   <= 0 for success, or an error code 
*/
kresult pci_release_device(unsigned short bus, unsigned short slot)
{
   unsigned char index = PCI_HASH_INDEX(bus, slot);
   pci_device *victim = pci_hash_tbl[index];
   
   while(victim)
   {
      if(victim->bus == bus && victim->slot == slot)
      {
         /* found it - remove device */
         if(victim->next)
            victim->next->prev = victim->prev;
         if(victim->prev)
            victim->prev->next = victim->next;
         else
            /* we were the hash table entry head, so fixup table */
            pci_hash_tbl[index] = victim->next;
         
         
         free(victim);
         return success;
      }
      
      victim = victim->next;
   }
   
   return e_not_found;
}

/* lookup_pid
   Look up the PID of a process registered with the given bus and slot
   => bus, slot = device numbers to search for
   <= PID of owning process, or 0 for none
*/
unsigned int lookup_pid(unsigned short bus, unsigned short slot)
{
   unsigned char index = PCI_HASH_INDEX(bus, slot);
   pci_device *search = pci_hash_tbl[index];
   
   while(search)
   {
      if(search->bus == bus && search->slot == slot)
         return search->pid; /* found a PID */
      
      search = search->next;
   }
   
   return 0; /* not found */
}

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
void reply_to_request(diosix_msg_info *msg, kresult result, unsigned int value)
{
   pci_reply_msg reply;
   
   /* sanity check */
   if(!msg) return;
   
   /* ping a reply back to unlock the requesting thread */
   reply.result = result;
   reply.value = value;
   
   msg->send = &reply;
   msg->send_size = sizeof(pci_reply_msg);
   
   diosix_msg_reply(msg);
}

void wait_for_request(void)
{
   diosix_msg_info msg;
   pci_request_msg request;
   unsigned int owner;
   
   memset(&msg, 0, sizeof(diosix_msg_info));
   
   /* wait for a generic message to come in */
   msg.flags = DIOSIX_MSG_GENERIC;
   msg.recv = &request;
   msg.recv_max_size = sizeof(pci_request_msg);
   
   if(diosix_msg_receive(&msg) == success)
   {      
      if(msg.recv_size < sizeof(pci_request_msg))
      {
         /* malformed request, it's too small to even hold a request type */
         reply_to_request(&msg, e_too_small, 0);
         return;
      }
      
      /* sanity check the message header */
      if(request.magic != PCI_MSG_MAGIC)
      {
         /* malformed request, bad magic */
         reply_to_request(&msg, e_bad_magic, 0);
         return;
      }
      
      /* we only respond to privileged processes and
         we only service requests to unclaimed
         devices or devices claimed by the sender */
      owner = lookup_pid(request.bus, request.slot);
      if(msg.uid != DIOSIX_SUPERUSER_ID ||
         (owner != msg.pid && owner != 0))
      {
         reply_to_request(&msg, e_no_rights, 0);
         return;
      }
      
      /* we only support vanilla PCI at the moment */
      if(request.bus_type != pci_bus)
      {
         reply_to_request(&msg, e_bad_arch, 0);
         return;
      }
      
      /* decode the request type */
      switch(request.req)
      {
         case read_config:
         {
            unsigned short value;
            kresult err = pci_read_config(request.bus, request.slot,
                                          request.func, request.offset, &value);
            if(err)
               reply_to_request(&msg, err, 0);
            else
               reply_to_request(&msg, success, value);
         }
         break;
         
         case claim_device:
         {
            /* already claimed? */
            if(owner)
               reply_to_request(&msg, e_exists, 0);
            else
               reply_to_request(&msg, pci_claim_device(request.bus, request.slot, msg.pid), 0);
         }
         break;
         
         case release_device:
         {
            /* already claimed? */
            if(owner)
               reply_to_request(&msg, pci_release_device(request.bus, request.slot), 0);
            else
               reply_to_request(&msg, e_bad_params, 0);
         }
         break;
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params, 0);
      }
   }
}


int main(void)
{
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_PCIMANAGER);
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
