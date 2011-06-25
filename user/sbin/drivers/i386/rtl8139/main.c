/* user/sbin/drivers/i386/rtl8139/main.c
 * Driver to send and receive ethernet frames to and from a rtl8139 card
 * Author : Chris Williams
 * Date   : Sun,29 May 2011.06:11:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "lowlevel.h"
#include "pci.h"
#include "nic.h"

#include "rtl8139.h"

/* --------------------------------------------------------------
                        interrupt handling
   -------------------------------------------------------------- */

/* process_interrupts
   Loop, receiving interrupt signals and processing them
   => bus, slot, irq = PCI and IRQ line details
      for the card this thread is responsible for
      iobase = IO ports used by this card
      id = driver thread's ID
   <= should never return
*/
void process_interrupts(nic_info *nic)
{
   diosix_msg_info msg;

   /* sanity check */
   if(!nic) return;
   
   /* prepare to sleep until an interrupt comes in 
      from the kernel as a signal */
   msg.role = msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_KERNELONLY;
   
   while(1)
   {
      /* block until a signal comes in from the hardware via microkernel */
      if(diosix_msg_receive(&msg) == success && msg.signal.extra == nic->irq)
      {
         unsigned short isr;
         
         /* check to see what's happening by reading the intr status register */
         isr = read_port(nic->iobase + RTL8139_REG_ISR);
         printf("rtl8139.%i: IRQ received! (%x)\n", nic->id, isr);
         
         if(isr & RTL8139_REG_ISR_RECVOK)
         {
            lock_spin(&(nic->recv_lock));
            
            /* mark this buffer as being filled and awaiting reading */
            set_buffer_inuse(&(nic->recv_filled[0]), nic->current_recv);
                        
            /* find another receive buffer */
            change_recv_buffer(nic);
            
            unlock_spin(&(nic->recv_lock));

            /* reset the read pointer */
            write_port(nic->iobase + RTL8139_REG_CAPR, 0);            
         }
         
         /* we shouldn't accept error packets but in case one is flagged up,
            skip over it */
         if(isr & RTL8139_REG_ISR_RECVERR)
            write_port(nic->iobase + RTL8139_REG_CAPR, 0);

         /* turnaround a send buffer so it can be reused */
         if(isr & (RTL8139_REG_ISR_SENDOK | RTL8139_REG_ISR_SENDERR))
         {
            unsigned char loop;
            
            /* find the buffers with TOK and OWN set and release them */
            for(loop = 0; loop < SEND_BUFFERS; loop++)
            {
               unsigned int status = read_port_word(nic->iobase + RTL8139_REG_TSR(loop));
               if(status & (RTL8139_REG_TSR_TOK | RTL8139_REG_TSR_OWN))
               {
                  lock_spin(&(nic->send_lock));
                  
                  clear_buffer_inuse(nic->send_locks, loop);
                  clear_buffer_inuse(nic->send_filled, loop);
                  
                  unlock_spin(&(nic->send_lock));
               }
            }
         }
         
         /* ignore the other interrupts, they should be masked out anyway */
      }
   }
}


/* --------------------------------------------------------------
                        misc support functions
   -------------------------------------------------------------- */

/* read_iobase 
   => bus, slot = PCI card location
   <= returns IO space base address for the card's registers,
       or 0 for none configured
*/
unsigned short read_iobase(unsigned short bus, unsigned short slot)
{
   unsigned short ioaddr_low;
   
   pci_read_config(bus, slot, 0, RTL8139_PCI_IOAR0, &ioaddr_low);

   /* bit 0 of IOAR0 must be set for IO space to be usable */
   if(!(ioaddr_low & 1)) return 0;
   
   /* mask off the lowest 8bits and return the IO base */
   return ioaddr_low & ~(0x0ff);
}

/* read_macaddress
   Read the six-byte MAC address for the card
   => iobase = PCI card's IO space base
      mac = pointer to six-byte array to store the address in
*/
void read_macaddress(unsigned short iobase, char *mac)
{
   unsigned char count;
   
   for(count = 0; count < NIC_MAC_LENGTH; count++)
      mac[count] = read_port_byte(iobase + RTL8139_PCI_IDR0 + count);
}

/* set_buffer_inuse
   Set a bit in a buffer bitmap, marking it in use or full, etc
   => bitmap = buffer status bitmap to use
      nr = the buffer number to look up and set
   <= 0 for success, or a failure code
*/
unsigned char set_buffer_inuse(volatile unsigned char *bitmap, unsigned char nr)
{
   /* assumes nr is a sane value! do some other checks just in case */
   if(!bitmap) return e_bad_params;
   
   /* convert buffer number into byte and bit offsets into bitmaps */
   unsigned char byte = nr >> 3;
   unsigned char bit = nr - (byte << 3);
   
   bitmap[byte] |= (1 << bit);
   
   return 0;
}

/* clear_buffer_inuse
   Clear a bit in a buffer bitmap, marking it not in use or not full, etc
   => bitmap = buffer status bitmap to use
      nr = the buffer number to look up and clear
   <= 0 for success, or a failure code
*/
unsigned char clear_buffer_inuse(volatile unsigned char *bitmap, unsigned char nr)
{
   /* assumes nr is a sane value! do some other checks just in case */
   if(!bitmap) return e_bad_params;
   
   /* convert buffer number into byte and bit offsets into bitmaps */
   unsigned char byte = nr >> 3;
   unsigned char bit = nr - (byte << 3);
   
   bitmap[byte] &= ~(1 << bit);
   
   return 0;
}

/* is_buffer_inuse
   Check to see if a buffer is either locked or filled
   => locks = bitmap to use for checking locks
      filled = bitmap to use for checking empty status
      nr = the buffer number to look up
   <= 1 for in use or 0 for not in use
*/
unsigned char is_buffer_inuse(volatile unsigned char *locks, volatile unsigned char *filled, unsigned char nr)
{
   /* assumes nr is a sane value! do some other checks just in case */
   if(!locks || !filled) return e_bad_params;
   
   /* convert buffer number into byte and bit offsets into bitmaps */
   unsigned char byte = nr >> 3;
   unsigned char bit = nr - (byte << 3);
 
   if(locks[byte] & (1 << bit)) return 1;
   if(filled[byte] & (1 << bit)) return 1;
   
   return 0;
}

/* change_recv_buffer
   Program the NIC to use the next available recv buffer
   => nic = network card structure
   <= 0 for success, or an error code
*/
kresult change_recv_buffer(nic_info *nic)
{   
   unsigned char start = nic->next_recv;
   
   /* quick sanity check */
   if(!nic) return e_bad_params;
   
   /* find a suitable recv buffer, used next_recv as a suggested starting point  */
   while(is_buffer_inuse(nic->recv_locks, nic->recv_filled, nic->next_recv))
   {
      nic->next_recv++;
      if(nic->next_recv >= RECV_BUFFERS) nic->next_recv = 0;
      if(start == nic->next_recv) return e_not_found; /* no free buffers */
   }
   
   /* tell the NIC about this buffer in physical memory */
   write_port_word(nic->iobase + RTL8139_REG_RBSTART, (unsigned int)nic->recv_phys[nic->next_recv]);
   nic->current_recv = nic->next_recv;

   /* move onto the next buffer, we'll check next time if it's available */
   nic->next_recv++;
   if(nic->next_recv >= RECV_BUFFERS) nic->next_recv = 0;
   
   return success;
}

/* transmit_packet
   Send a packet to the wire 
   => nic = network card to use
      nr = send buffer number to use
      len = number of bytes to send
*/
void transmit_packet(nic_info *nic, unsigned char nr, unsigned short len)
{
   unsigned int status;
   
   /* sanity check */
   if(!nic || nr >= SEND_BUFFERS) return;
   
   /* write the physical start address */
   write_port_word(nic->iobase + RTL8139_REG_TSA(nr), (unsigned int)nic->send_phys[nr]);
   
   /* write the number of bytes to read and clear the OWN bit to kick off the write */
   status = read_port_word(nic->iobase + RTL8139_REG_TSR(nr));
   status &= ~(RTL8139_REG_TSR_OWN | RTL8139_REG_TSR_SIZEMASK);
   status |= len;
   write_port_word(nic->iobase + RTL8139_REG_TSR(nr), status);
}


/* --------------------------------------------------------------
                   diosix specific stuff
   -------------------------------------------------------------- */
/* reply to a request from another process */
void reply_to_request(diosix_msg_info *msg, kresult result, unsigned char buffer_nr)
{
   diosix_nic_reply reply;
   
   /* sanity check */
   if(!msg) return;
   
   /* ping a reply back to unlock the requesting thread */
   reply.result = result;
   reply.buffer_nr = buffer_nr;
   msg->send = &reply;
   msg->send_size = sizeof(diosix_nic_reply);
   
   diosix_msg_reply(msg);
}

/* contact the TCP/IP stack and tell it about us */
void announce_to_stack(nic_info *nic)
{
   diosix_nic_announce_req req;
   diosix_nic_reply reply;
   
   req.id = nic->id;
   snprintf(&(req.name[0]), NIC_NAME_LENGTH, NIC_IF_NAME, nic->id);
   req.send_buffers = SEND_BUFFERS;
   req.recv_buffers = RECV_BUFFERS;
   req.send_buffer_size = SEND_BUFFER_PAGES * DIOSIX_MEMORY_PAGESIZE;
   req.recv_buffer_size = RECV_BUFFER_PAGES * DIOSIX_MEMORY_PAGESIZE;
   
   nic_send_to_stack(nic_announce_device, &req, &reply);
}

/* --------------------------------------------------------------
                     driver main program
   -------------------------------------------------------------- */

int main(void)
{
   unsigned char loop = 0, claimed = 0;
   unsigned short vendorid, deviceid;
   unsigned int interrupt_thread;
   kresult err;

   /* each thread will have one of these structures, there will
      be one thread per RTL8139 card found */
   nic_info *nic = NULL;
   
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   /* whoever heard of a machine with 256+ network cards? */
   while(loop != 255)
   {
      unsigned short bus, slot;
      unsigned int pid;
      
      /* attempt to find the rtl8139 PCI card */
      err = pci_find_device(PCI_CLASS_CALC(PCI_CLASS_NETWORKING, PCI_SUBCLASS_ETHERNET),
                            loop, &bus, &slot, &pid);

      /* give up now if we hit an error */
      if(err) break;
      
      if(pid == 0)
      {
         /* check this is a supported device */
         pci_read_config(bus, slot, 0, PCI_HEADER_VENDORID, &vendorid);
         pci_read_config(bus, slot, 0, PCI_HEADER_DEVICEID, &deviceid);
         
         if(vendorid == RTL8139_VENDORID && deviceid == RTL8139_DEVICEID)
         {
            if(pci_claim_device(bus, slot, 0) == success)
            {
               /* we've managed to claim a card so fork a worker thread */
               if(diosix_thread_fork() == 0)
               {
                  /* now running in a worker thread, so do some admin */
                  nic = malloc(sizeof(nic_info));
                  if(!nic)
                  {
                     printf("rtl8139.%i: failed to allocate memory for internal structures\n", loop);
                     diosix_thread_exit(1);
                  }
                  
                  /* clear the structure and fill in the basics */
                  memset(nic, 0, sizeof(nic_info));
                  nic->bus = bus;
                  nic->slot = slot;
                  nic->id = loop;
                  
                  claimed = 1;
                  break;
               }
            }
         }
      }
      
      loop++;
   }
      
   /* catch the card detection thread (usually thread 1) and kill it */
   if(!claimed) diosix_thread_exit(0);
      
   /* decode the IO space address bits into the PCI config */
   nic->iobase = read_iobase(nic->bus, nic->slot);
   
   /* read the PIC IRQ line (lowest 8 bits) and channel IRQs from the network
      card to our process */
   pci_read_config(nic->bus, nic->slot, 0, PCI_HEADER_INT_LINE, &(nic->irq));
   nic->irq = (nic->irq & 0xff) + X86_PIC_IRQ_BASE;
   diosix_signals_kernel(SIGX_ENABLE(SIGXIRQ));
   diosix_driver_register_irq(nic->irq);
   
   /* fork a worker thread to handle the interrupt */
   interrupt_thread = diosix_thread_fork();
   if(interrupt_thread == 0) process_interrupts(nic);
   
   /* give up if we don't have an IO space base */
   if(!nic->iobase)
   {
      printf("rtl8139.%i: no IO access to card's registers??\n", nic->id);
      pci_release_device(nic->bus, nic->slot);
      diosix_thread_kill(interrupt_thread);
      free(nic);
      diosix_thread_exit(1);
   }
   
   /* enable the RTL8139 chipset and reset it by..
       - clearing the config 0 word
       - setting bit 4 (software reset) in the
         command register
       - wait for the software reset bit to clear
   */
   write_port_byte(nic->iobase + RTL8139_REG_CONFIG1, 0x00);
   write_port_byte(nic->iobase + RTL8139_REG_CR, RTL8139_REG_CR_RESET);
   while((read_port_byte(nic->iobase + RTL8139_REG_CR) & RTL8139_REG_CR_RESET) != 0);

   /* grab some phys mem for the card's send buffers and also map them into virtual space */
   for(loop = 0; loop < RECV_BUFFERS; loop++)
   {
      diosix_phys_request phys_mem;
      
      err = diosix_driver_req_phys(RECV_BUFFER_PAGES, (unsigned int *)&(nic->recv_phys[loop]));
      if(err)
      {
         printf("rtl8139.%i: unable to claim physical memory for receive buffers\n", nic->id);
         pci_release_device(nic->bus, nic->slot);
         diosix_thread_kill(interrupt_thread);
         free(nic);
         diosix_thread_exit(1);
      }
   
      /* now make sure we can access this physical memory in virtual space */
      phys_mem.size = RECV_BUFFER_PAGES * DIOSIX_MEMORY_PAGESIZE;
      phys_mem.flags = VMA_WRITEABLE | VMA_NOCACHE;
      phys_mem.paddr = nic->recv_phys[loop];
      phys_mem.vaddr = (void *)THREAD_RECV_BUFFER(nic->id, loop);
      
      printf("*** recv[%i] size = %x phys = %x virt = %x\n",
             loop, (unsigned int)phys_mem.size, (unsigned int)phys_mem.paddr, (unsigned int)phys_mem.vaddr);
      
      err = diosix_driver_map_phys(&phys_mem);
      if(err)
      {
         printf("rtl8139.%i: unable to map physical memory into virtual space\n", nic->id);
         pci_release_device(nic->bus, nic->slot);
         diosix_thread_kill(interrupt_thread);
         free(nic);
         diosix_thread_exit(1);
      }
      
      nic->recv_virt[loop] = phys_mem.vaddr; /* keep a copy of the calculated virtual addr */
   }
   
   /* grab some phys mem for the card's send buffers and also map them into virtual space */
   for(loop = 0; loop < SEND_BUFFERS; loop++)
   {
      diosix_phys_request phys_mem;
      
      err = diosix_driver_req_phys(SEND_BUFFER_PAGES, (unsigned int *)&(nic->send_phys[loop]));
      if(err)
      {
         printf("rtl8139.%i: unable to claim physical memory for send buffer %i\n", nic->id, loop);
         pci_release_device(nic->bus, nic->slot);
         diosix_thread_kill(interrupt_thread);
         diosix_thread_exit(1); /* physical memory claimed will be released by the kernel */
      }
      
      /* now make sure we can access this physical memory in virtual space */
      phys_mem.size = SEND_BUFFER_PAGES * DIOSIX_MEMORY_PAGESIZE;
      phys_mem.flags = VMA_WRITEABLE | VMA_NOCACHE;
      phys_mem.paddr = nic->recv_phys[loop];
      phys_mem.vaddr = (void *)THREAD_SEND_BUFFER(nic->id, loop);
      
      printf("*** recv[%i] size = %x phys = %x virt = %x\n",
             loop, (unsigned int)phys_mem.size, (unsigned int)phys_mem.paddr, (unsigned int)phys_mem.vaddr);
      
      err = diosix_driver_map_phys(&phys_mem);
      if(err)
      {
         printf("rtl8139.%i: unable to map physical memory for send buffer %i into virtual space\n", nic->id, loop);
         pci_release_device(nic->bus, nic->slot);
         diosix_thread_kill(interrupt_thread);
         free(nic);
         diosix_thread_exit(1); /* physical memory claimed will be released by the kernel */
      }

      nic->recv_virt[loop] = phys_mem.vaddr; /* keep a copy of the calculated virtual addr */
   }

   /* tell the PCI card where to find this physical memory for receiving */
   change_recv_buffer(nic);
      
   /* tell the network stack about us */
   announce_to_stack(nic);
   
   /* enable the transmit OK and receive OK interrupts */
   write_port_word(nic->iobase + RTL8139_REG_IMR,
                   RTL8139_REG_IMR_TOK | RTL8139_REG_IMR_ROK);
   
   /* set the acceptable packet bits and the wrap bit, disable early rx,
      8K + overspill space for buffer size and 1024 byte max DMA burst size */
   write_port_word(nic->iobase + RTL8139_REG_RCR,
                   RTL8139_REG_RXTX_MAXDMA_1024 | 
                   RTL8139_REG_RCR_ACCEPT_BROAD |
                   RTL8139_REG_RCR_ACCEPT_MATCH |
                   RTL8139_REG_RCR_WRAP);

   /* program the card to use 1024 byte max DMA burst size */
   write_port_word(nic->iobase + RTL8139_REG_TCR,
                   read_port_word(nic->iobase + RTL8139_REG_TCR) | RTL8139_REG_RXTX_MAXDMA_1024);
   
   /* enable transmission and receiving */
   write_port_byte(nic->iobase + RTL8139_REG_CR,
                   RTL8139_REG_CR_RECVR_EN | RTL8139_REG_CR_TRANS_EN);

   /* get the card's MAC address */
   read_macaddress(nic->iobase, &(nic->mac[0]));
   
   printf("rtl8139.%i: initialised 8139 PCI card %x:%x irq %x iobase %x "
          "[mac %x %x %x %x %x %x]\n",
          nic->id, nic->bus, nic->slot, nic->irq, nic->iobase,
          nic->mac[0], nic->mac[1], nic->mac[2], nic->mac[3], nic->mac[4], nic->mac[5]);
      
   /* wait for a generic message to come in */
   memset(&(nic->msg), 0, sizeof(diosix_msg_info));
   nic->msg.flags = DIOSIX_MSG_GENERIC;
   nic->msg.recv = &(nic->msg_buffer);
   nic->msg.recv_max_size = MSG_RECEIVE_BUFFER_SIZE;
   
   while(1)
   {
      unsigned char failed = 0;
      
      /* strictly keep communications to the network stack */
      nic->msg.role = DIOSIX_ROLE_NETWORKSTACK;
      
      if(diosix_msg_receive(&(nic->msg)) == success)
      {
         diosix_nic_req *req = (diosix_nic_req *)&(nic->msg_buffer);
         
         if(nic->msg.recv_size < sizeof(diosix_nic_req))
         {
            /* malformed request, wrong size */
            reply_to_request(&(nic->msg), e_too_small, 0);
            failed = 1;
         }
         
         /* sanity check the message header */
         if(req->magic != NIC_MSG_MAGIC)
         {
            /* malformed request, bad magic */
            reply_to_request(&(nic->msg), e_bad_magic, 0);
            failed = 1;
         }
                  
         if(!failed && nic->msg.role == DIOSIX_ROLE_NETWORKSTACK)
         {
            /* decode the request type */
            switch(req->type)
            {
               /* the stack wants access to our buffers, so share 'em */
               case nic_share_buffer:
               {
                  if(!(nic->msg.flags & DIOSIX_MSG_SHAREVMA))
                  {
                     reply_to_request(&(nic->msg), e_bad_params, 0);
                     break;
                  }
                  
                  if(req->data.buffer_req.type == nic_send_buffer)
                  {
                     /* make sure the share request is sane */
                     if(req->data.buffer_req.buffer_nr > SEND_BUFFERS ||
                        nic->msg.mem_req.size != SEND_BUFFER_PAGES * DIOSIX_MEMORY_PAGESIZE)
                        reply_to_request(&(nic->msg), e_bad_params, 0);
                     else
                     {
                        /* ok, let's do this - pass back our virtual address */
                        nic->msg.mem_req.base = (unsigned int)nic->send_virt[req->data.buffer_req.buffer_nr];
                        reply_to_request(&(nic->msg), success, req->data.buffer_req.buffer_nr);
                     }
                     
                     break;
                  }
                  
                  if(req->data.buffer_req.type == nic_recv_buffer)
                  {
                     /* make sure the share request is sane */
                     if(req->data.buffer_req.buffer_nr > RECV_BUFFERS ||
                        nic->msg.mem_req.size != RECV_BUFFER_PAGES * DIOSIX_MEMORY_PAGESIZE)
                        reply_to_request(&(nic->msg), e_bad_params, 0);
                     else
                     {
                        /* ok, let's do this - pass back our virtual address */
                        nic->msg.mem_req.base = (unsigned int)nic->recv_virt[req->data.buffer_req.buffer_nr];
                        reply_to_request(&(nic->msg), success, req->data.buffer_req.buffer_nr);
                     }
                     
                     break;
                  }
                  
                  /* still here? then the type was bad */
                  reply_to_request(&(nic->msg), e_bad_params, 0);
               }
               break;
               
               /* the stack wants to lock a buffer to write into */
               case nic_lock_buffer:
               {
                  unsigned char start = nic->next_send, found = 1;
                  
                  /* sanity check the requst - only write buffers 
                     can be locked, read buffers are auto-locked by the driver */
                  if(req->data.buffer_req.type != nic_send_buffer)
                  {
                     reply_to_request(&(nic->msg), e_bad_params, 0);
                     break;
                  }
                  
                  lock_spin(&(nic->send_lock));
                  
                  /* find a suitable send buffer, use next_send as a suggested starting point  */
                  while(is_buffer_inuse(nic->send_locks, nic->send_filled, nic->next_send))
                  {
                     nic->next_send++;
                     if(nic->next_send >= RECV_BUFFERS) nic->next_send = 0;
                     if(start == nic->next_send)
                     {
                        /* looped - no free buffers */
                        unlock_spin(&(nic->send_lock));
                        reply_to_request(&(nic->msg), e_no_phys_pgs, 0);
                        found = 0;
                        break;
                     }
                  }

                  if(!found) break;
                  
                  set_buffer_inuse(&(nic->send_locks[0]), nic->next_send);
                  reply_to_request(&(nic->msg), success, nic->next_send);

                  /* suggest another buffer to use next time, we'll check its in use status
                     then */
                  nic->next_send++;
                  if(nic->next_send >= SEND_BUFFERS) nic->next_send = 0;
                  
                  unlock_spin(&(nic->send_lock));
               }
               break;
                  
               /* the stack wants to unlock a write buffer (and send the packet and reuse it) or
                  unlock a read buffer (and thus release it for reuse) */
               case nic_unlock_buffer:
               {
                  lock_spin(&(nic->send_lock));
                  if(req->data.buffer_req.type == nic_send_buffer &&
                     req->data.buffer_req.buffer_nr < SEND_BUFFERS &&
                     is_buffer_inuse(nic->send_locks, nic->send_filled, req->data.buffer_req.buffer_nr))
                  {
                     /* unlock the send buffer */
                     clear_buffer_inuse(nic->send_locks, req->data.buffer_req.buffer_nr);
                     
                     /* set the filled bit */
                     set_buffer_inuse(nic->send_filled, req->data.buffer_req.buffer_nr);
                     
                     /* and transmit the packet */
                     transmit_packet(nic, req->data.buffer_req.buffer_nr, req->data.buffer_req.buffer_size);
                     
                     nic->next_send = req->data.buffer_req.buffer_nr;
                     unlock_spin(&(nic->send_lock));
                     reply_to_request(&(nic->msg), success, 0);
                     break;
                  }
                  unlock_spin(&(nic->send_lock));
                  
                  lock_spin(&(nic->recv_lock));
                  if(req->data.buffer_req.type == nic_recv_buffer &&
                     req->data.buffer_req.buffer_nr < RECV_BUFFERS &&
                     is_buffer_inuse(nic->recv_locks, nic->recv_filled, req->data.buffer_req.buffer_nr))
                  {
                     /* unlock the buffer for reuse */
                     clear_buffer_inuse(nic->recv_locks, req->data.buffer_req.buffer_nr);
                     clear_buffer_inuse(nic->recv_filled, req->data.buffer_req.buffer_nr);
                     
                     nic->next_recv = req->data.buffer_req.buffer_nr;
                     
                     unlock_spin(&(nic->recv_lock));
                     reply_to_request(&(nic->msg), success, 0);
                     break;
                  }
                  unlock_spin(&(nic->recv_lock));
                  
                  /* still here? fail the caller */
                  reply_to_request(&(nic->msg), e_bad_params, 0);
               }
               break;
                  
               /* if the type is unknown then fail it */
               default:
               reply_to_request(&(nic->msg), e_bad_params, 0);
            }
         }
      }
   }
}
