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

/* define a detected PCI device */
typedef struct pci_device pci_device;
struct pci_device
{
   unsigned short bus, slot;
   unsigned short deviceid, vendorid, func, class;
   unsigned int pid;
   
   /* hash table links */
   pci_device *phys_prev, *phys_next;
   pci_device *class_prev, *class_next;
};

#define PCI_HASH_MAX         (16)
#define PCI_PHYS_INDEX(b, s) (((b) + (s)) % PCI_HASH_MAX)
#define PCI_CLASS_INDEX(c) ((((c) >> 8) + ((c) & 0xff)) % PCI_HASH_MAX)

/* used to guarantee shared data integrity */
volatile unsigned char pci_lock = 0;

/* table to store detected PCI devices, hashed by physical connection */
pci_device *pci_phys_tbl[PCI_HASH_MAX];

/* table to store detected PCI devices, hashed by class sub-class */
pci_device *pci_class_tbl[PCI_HASH_MAX];

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
   Claim (or release) exclusive ownership of a device to stop other
   device drivers from commandeering the PCI card.
   => bus, slot, func = select the PCI device
      pid = PID of the process claiming the device, or 0 to deregister the device
   <= 0 for success, or an error code 
*/
kresult pci_claim_device(unsigned short bus, unsigned short slot, unsigned short func, unsigned int pid)
{
   unsigned char index = PCI_PHYS_INDEX(bus, slot);
   pci_device *device;
   
   lock_spin(&pci_lock);
   
   device = pci_phys_tbl[index];
   
   while(device)
   {
      if(device->bus == bus && device->slot == slot && device->func == func)
      {
         /* found it - change the pid */
         device->pid = pid;
         unlock_spin(&pci_lock);
         return success;
      }
      
      device = device->phys_next;
   }
   
   unlock_spin(&pci_lock);
   return e_not_found;
}

/* pci_find_device
   Look up the bus and slot numbers of a detected device from its class
   => class = high byte is the class, low byte is the sub-class
      count = for a machine with multiple cards with the same class+subclass,
              this index selects the required card (starting from 0)
      bus, slot, func = pointers to variables to write bus, slot and function numbers in, if successful
      pid = pointer to store PID of owning process, or 0 if none, if successful
   <= 0 for success, or an error code
*/
kresult pci_find_device(unsigned short class, unsigned char count,
                        unsigned short *bus, unsigned short *slot, unsigned short *func,
                        unsigned int *pid)
{
   unsigned char index = PCI_CLASS_INDEX(class);
   pci_device *search;
   
   if(!bus || !slot) return e_bad_params;

   lock_spin(&pci_lock);
   search = pci_class_tbl[index];
   
   while(search)
   {
      if(search->class == class)
      {
         if(count)
            count--;
         else
         {
            *bus = search->bus;
            *slot = search->slot;
            *func = search->func;
            *pid = search->pid;
            unlock_spin(&pci_lock);
            return success;
         }
      }
      
      search = search->class_next;
   }
   
   unlock_spin(&pci_lock);
   return e_not_found;
}

/* lookup_pid_from_phys
   Look up the PID of a process registered with the given bus and slot
   => bus, slot, func = device numbers to search for
   <= PID of owning process, or 0 for none
*/
unsigned int lookup_pid_from_phys(unsigned short bus, unsigned short slot, unsigned short func)
{
   unsigned char index = PCI_PHYS_INDEX(bus, slot);
   pci_device *search;
   
   lock_spin(&pci_lock);
   search = pci_phys_tbl[index];
   
   while(search)
   {
      if(search->bus == bus && search->slot == slot && search->func == func)
      {
         unlock_spin(&pci_lock);
         return search->pid; /* found a PID */
      }
      
      search = search->phys_next;
   }
   
   unlock_spin(&pci_lock);
   return 0; /* not found */
}

/* ----------------------------------------------------------------------
   enumerate devices
   ---------------------------------------------------------------------- */
/* discover_devices
   Populate the class hash table of devices with detected cards */
void discover_devices(void)
{
   unsigned short bus, slot, func, vendor, count = 0;
   
   for(bus = 0; bus < (1 << 8); bus++)
      for(slot = 0; slot < (1 << 5); slot++)
         for(func = 0; func < (1 << 3); func++)
         {            
            if(pci_read_config(bus, slot, func, PCI_HEADER_VENDORID, &vendor) == success)
            {
               unsigned short deviceid, class, phys_index, class_index, type;
               pci_device *new;
               pci_device *head;
               
               /* check the details - skip non-generic cards */
               pci_read_config(bus, slot, func, PCI_HEADER_TYPE, &type);
               if((type & PCI_HEADER_TYPEMASK) != PCI_HEADER_GENERIC) continue;
               
               pci_read_config(bus, slot, func, PCI_HEADER_DEVICEID, &deviceid);
               pci_read_config(bus, slot, func, PCI_HEADER_CLASS, &class);
               
               new = malloc(sizeof(pci_device));
               if(!new)
               {
                  printf("malloc() failed during device discovery\n");
                  return; /* bail out if memory is a problem */
               }
               
               new->bus = bus;
               new->slot = slot;
               new->func = func;
               new->deviceid = deviceid;
               new->class = class;
               new->pid = 0; /* no registered process yet */
               
               /* add into the hash tables */
               phys_index = PCI_PHYS_INDEX(bus, slot);
               class_index = PCI_CLASS_INDEX(class);
                           
               head = pci_phys_tbl[phys_index];
               if(head)
               {
                  head->phys_prev = new;
                  new->phys_next = head;
               }
               else
                  new->phys_next = NULL;
               pci_phys_tbl[phys_index] = new;
               new->phys_prev = NULL;
               
               head = pci_class_tbl[class_index];
               if(head)
               {
                  head->class_prev = new;
                  new->class_next = head;
               }
               else
                  new->class_next = NULL;
               pci_class_tbl[class_index] = new;
               new->class_prev = NULL;
               
               count++;
               
               /* break out of the loop if this is a single function card */
               if(!(type & PCI_HEADER_MULTIFUNC)) break;
            }
         }
   
   printf("found %i PCI device(s)\n", count);
}

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
void reply_to_request(diosix_msg_info *msg, kresult result, pci_reply_msg *reply)
{   
   /* sanity check */
   if(!msg || !reply) return;
   
   /* ping a reply back to unlock the requesting thread */
   reply->result = result;
   
   msg->send = reply;
   msg->send_size = sizeof(pci_reply_msg);
   
   diosix_msg_reply(msg);
}

void wait_for_request(void)
{
   diosix_msg_info msg;
   pci_request_msg request;
   pci_reply_msg reply;
   unsigned int owner = 0;
   
   memset(&msg, 0, sizeof(diosix_msg_info));
   memset(&reply, 0, sizeof(pci_reply_msg));
   
   /* wait for a generic message to come in */
   msg.flags = DIOSIX_MSG_GENERIC;
   msg.recv = &request;
   msg.recv_max_size = sizeof(pci_request_msg);

   if(diosix_msg_receive(&msg) == success)
   {      
      if(msg.recv_size < sizeof(pci_request_msg))
      {
         /* malformed request, it's too small to even hold a request type */
         reply_to_request(&msg, e_too_small, &reply);
         return;
      }
      
      /* sanity check the message header */
      if(request.magic != PCI_MSG_MAGIC)
      {
         /* malformed request, bad magic */
         reply_to_request(&msg, e_bad_magic, &reply);
         return;
      }
      
      /* we only respond to privileged processes */
      if(msg.uid != DIOSIX_SUPERUSER_ID)
      {
         reply_to_request(&msg, e_no_rights, &reply);
         return;
      }
      
      /* unless the request is an initial discovery message, 
         we only service requests to unclaimed devices
         or devices claimed by the sender */
      if(request.req != find_device)
      {
         owner = lookup_pid_from_phys(request.bus, request.slot, request.func);
         if(owner != msg.pid && owner != 0)
         {
            reply_to_request(&msg, e_no_rights, &reply);
            return;
         }
      }
      
      /* we only support vanilla PCI at the moment */
      if(request.bus_type != pci_bus)
      {
         reply_to_request(&msg, e_bad_arch, &reply);
         return;
      }
   
      /* decode the request type */
      switch(request.req)
      {
         case read_config:
         {
            unsigned short value;
            kresult err = pci_read_config(request.bus, request.slot, request.func, request.offset, &value);
            if(err)
               reply_to_request(&msg, err, &reply);
            else
            {
               reply.value = value;
               reply_to_request(&msg, success, &reply);
            }
         }
         break;
         
         case claim_device:
         {            
            /* already claimed? */
            if(owner)
               reply_to_request(&msg, e_exists, &reply);
            else
               reply_to_request(&msg, pci_claim_device(request.bus, request.slot, request.func, msg.pid), &reply);
         }
         break;
         
         case release_device:
         {            
            /* already claimed? */
            if(owner)
               reply_to_request(&msg, pci_claim_device(request.bus, request.slot, request.func, 0), &reply);
            else
               reply_to_request(&msg, e_bad_params, &reply);
         }
         break;
            
         case find_device:
         {
            kresult err = pci_find_device(request.class, request.count,
                                          &(reply.bus), &(reply.slot), &(reply.func), &(reply.pid));
            if(err)
               reply_to_request(&msg, err, &reply);
            else
               reply_to_request(&msg, success, &reply);
         }
         break;
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params, &reply);
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
   
   /* let's see what we've got on this system */
   discover_devices();
   
   /* split into two worker threads */
   diosix_thread_fork();
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
