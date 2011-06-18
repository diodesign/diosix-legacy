/* user/sbin/drivers/i386/rtl8139/main.c
 * Driver to send and receive ethernet frames from a rtl8139 card
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

#define RTL8139_VENDORID      (0x10ec)
#define RTL8139_DEVICEID      (0x8139)

#define RTL8139_PCI_IDR0      (0x00)
#define RTL8139_PCI_IOAR0     (0x10)
#define RTL8139_PCI_IOAR1     (0x12)
#define RTL8139_PCI_CR        (0x37)
#define RTL8139_PCI_CONFIG0   (0x50)
#define RTL8139_PCI_CONFIG1   (0x51)

#define RTL8139_PCI_CR_RESET  (1 << 4)

/* process_interrupts
   Loop, receiving interrupt signals
   => bus, slot, irq = PCI and IRQ line details
      for the card this thread is responsible for
   <= should never return
*/
void process_interrupts(unsigned short bus, unsigned short slot, unsigned short irq)
{
   diosix_msg_info msg;
   
   /* prepare to sleep until an interrupt comes in */
   msg.role = msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_KERNELONLY;
   
   while(1)
   {
      /* block until a signal comes in from the hardware */
      if(diosix_msg_receive(&msg) == success && msg.signal.extra == irq)
      {
         printf("rtl8139: IRQ received!\n");
      }
   }
}

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
void read_macaddress(unsigned short iobase, unsigned char *mac)
{
   unsigned char count;
   
   for(count = 0; count < 6; count++)
      mac[count] = read_port_byte(iobase + RTL8139_PCI_IDR0 + count);
}

int main(void)
{
   unsigned short bus, slot;
   unsigned int pid;
   unsigned char count = 0, claimed = 0;
   unsigned short vendorid, deviceid, irq, iobase;
   unsigned char mac[6];
   kresult err;
   
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   /* whoever heard of a machine with 256+ network cards? */
   while(count != 255)
   {
      /* attempt to find the rtl8139 PCI card */
      err = pci_find_device(PCI_CLASS_CALC(PCI_CLASS_NETWORKING, PCI_SUBCLASS_ETHERNET),
                            count, &bus, &slot, &pid);

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
                  claimed = 1;
                  break;
               }
            }
         }
      }
      
      count++;
   }
   
   /* count now becomes the card driver instance number.. */
   
   /* catch the card detection thread and kill it */
   if(!claimed) diosix_thread_exit(0);
   
   /* read the PIC IRQ line (lowest 8 bits) and channel IRQs from the network
      card to our process */
   pci_read_config(bus, slot, 0, PCI_HEADER_INT_LINE, &irq);
   irq = (irq & 0xff) + X86_PIC_IRQ_BASE;
   diosix_signals_kernel(SIGX_ENABLE(SIGXIRQ));
   diosix_driver_register_irq(irq);
   
   /* fork a worker thread to handle the interrupt */
   if(diosix_thread_fork() == 0) process_interrupts(bus, slot, irq);
   
   /* decode the IO space address bits into the PCI config */
   iobase = read_iobase(bus, slot);
   
   /* give up if we don't have an IO space base */
   if(!iobase)
   {
      printf("rtl8139.%i: no IO access to card's registers??\n", count);
      pci_release_device(bus, slot);
      diosix_thread_exit(1);
   }
   
   /* enable the RTL8139 chipset and reset it by..
       - clearing the config 0 word
       - setting bit 4 (software reset) in the
         command register
       - wait for the software reset bit to clear
   */
   write_port_byte(iobase + RTL8139_PCI_CONFIG1, 0x00);
   write_port_byte(iobase + RTL8139_PCI_CR, RTL8139_PCI_CR_RESET);
   while((read_port_byte(iobase + RTL8139_PCI_CR) & RTL8139_PCI_CR_RESET) != 0);
   
   /* get the card's MAC address */
   read_macaddress(iobase, mac);
   
   printf("rtl8139.%i: initialised 8139 PCI card %x:%x irq %x iobase %x "
          "[mac %x %x %x %x %x %x]\n",
          count, bus, slot, irq, iobase,
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

   
   
   while(1);
}
