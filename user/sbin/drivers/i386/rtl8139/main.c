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

#define RTL8139_VENDORID   (0x10ec)
#define RTL8139_DEVICEID   (0x8139)

int main(void)
{
   unsigned short bus, slot;
   unsigned int pid;
   unsigned char count = 0, claimed = 0;
   unsigned short vendorid, deviceid;
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
   
   /* catch the detection thread and kill it */
   if(!claimed) diosix_thread_exit(0);
   
   printf("rtl8139: claimed 8139 PCI card %x %x\n", bus, slot);
   
   while(1);
}
