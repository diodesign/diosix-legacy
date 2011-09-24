/* user/sbin/drivers/i386/ata/main.c
 * Driver for ATA-connected storage devices 
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.22:54:00

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

#include "ata.h"
#include "lowlevel.h"
#include "pci.h"

/* table of controllers */
ata_controller controllers[ATA_MAX_CONTROLLERS];

/* discover_controllers
   Detect and setup ATA controllers and drives, and return number of drives */
unsigned char discover_controllers(void)
{
   unsigned char pci_loop = 0, ccount = 0, dcount = 0;
   kresult err;
   
   /* try to find mass storage IDE controllers on the PCI bus */
   unsigned short class = PCI_CLASS_CALC(PCI_CLASS_MASSSTORAGE, PCI_SUBCLASS_IDE);
   
   /* run through the available controllers on the PCI sub-system */
   do
   {
      unsigned short bus, slot;
      unsigned int pid;

      printf("trying to find bus %i slot %i class %x\n", bus, slot, class);
      err = pci_find_device(class, pci_loop, &bus, &slot, &pid);
      printf("err = %i\n", err);
      if(!err && pid == 0)
      {
         printf("trying to claim bus %i slot %i\n", bus, slot);
         err = pci_claim_device(bus, slot, 0);
         if(!err)
         {
            unsigned short header_type, func = 0;
            
            /* loop through possible functions */
            while(func < 8)
            {
               /* make sure the PCI card is valid */
               printf("trying to find bus %i slot %i func %i\n", bus, slot, func);
               pci_read_config(bus, slot, func, PCI_HEADER_TYPE, &header_type);
               if(header_type == PCI_HEADER_GENERIC)
               {                  
                  /* record the bus+slot */
                  controllers[ccount].bus = bus;
                  controllers[ccount].slot = slot;
                  controllers[ccount].flags = ATA_CONTROLLER_PCI;
                  
                  /* discover the controller's IO ports - note that if any
                     of the IO port values comes back as 0x00 or 0x01 then assume the 
                     default port values */
                  pci_read_config(bus, slot, func, PCI_HEADER_BAR0_LOW, &(controllers[ccount].channels[ATA_PRIMARY].ioport));
                  if(controllers[ccount].channels[ATA_PRIMARY].ioport < 0x02)
                     controllers[ccount].channels[ATA_PRIMARY].ioport = ATA_IOPORT_PRIMARY;
                  
                  pci_read_config(bus, slot, func, PCI_HEADER_BAR1_LOW,
                                  &(controllers[ccount].channels[ATA_PRIMARY].control_ioport));
                  if(controllers[ccount].channels[ATA_PRIMARY].control_ioport < 0x02)
                     controllers[ccount].channels[ATA_PRIMARY].control_ioport = ATA_IOPORT_PRIMARYCTRL;
                  
                  pci_read_config(bus, slot, func, PCI_HEADER_BAR2_LOW, &(controllers[ccount].channels[ATA_SECONDARY].ioport));
                  if(controllers[ccount].channels[ATA_SECONDARY].ioport < 0x02)
                     controllers[ccount].channels[ATA_SECONDARY].ioport = ATA_IOPORT_SECONDARY;
                  
                  pci_read_config(bus, slot, func, PCI_HEADER_BAR3_LOW,
                                  &(controllers[ccount].channels[ATA_SECONDARY].control_ioport));
                  if(controllers[ccount].channels[ATA_SECONDARY].control_ioport < 0x02)
                     controllers[ccount].channels[ATA_SECONDARY].control_ioport = ATA_IOPORT_SECONDARYCTRL;
                  
                  pci_read_config(bus, slot, func, PCI_HEADER_BAR4_LOW,
                                  &(controllers[ccount].channels[ATA_PRIMARY].busmaster_ioport));
                  controllers[ccount].channels[ATA_SECONDARY].busmaster_ioport = 
                     controllers[ccount].channels[ATA_PRIMARY].busmaster_ioport + 8;
                  
                  /* discover the IRQ lines */
                  pci_read_config(bus, slot, func, PCI_HEADER_INT_LINE, &(controllers[ccount].irq_line));
                  pci_read_config(bus, slot, func, PCI_HEADER_INT_PIN, &(controllers[ccount].irq_pin));
                  controllers[ccount].channels[ATA_PRIMARY].irq = ATA_IRQ_PRIMARY;
                  controllers[ccount].channels[ATA_SECONDARY].irq = ATA_IRQ_SECONDARY;
                  
                  printf("ata: claimed controller in PCI bus %x slot %x irq %x\n",
                         bus, slot, controllers[ccount].irq_line);
                  
                  /* discover connected storage drives */
                  dcount += ata_detect_drives(&(controllers[ccount]));
                  
                  /* move onto next controller.. */
                  ccount++;
               }
               else
               {
                  /* hand back the PCI device if it's not suitable */
                  pci_release_device(bus, slot);
               }
               
               func++;
            }
         }
      }
      
      pci_loop++;
   }
   while(!err && ccount < ATA_MAX_CONTROLLERS && pci_loop);
   
   if(dcount) return dcount; /* found our devices */
   
   /* try to find any other IDE controllers on the system and drives if
      there's no suitable hardware present on the PCI bus */
   controllers[ccount].flags = ATA_CONTROLLER_BUILTIN;
   controllers[ccount].channels[ATA_PRIMARY].irq = ATA_IRQ_PRIMARY;
   controllers[ccount].channels[ATA_PRIMARY].ioport = ATA_IOPORT_PRIMARY;
   controllers[ccount].channels[ATA_PRIMARY].control_ioport = ATA_IOPORT_PRIMARYCTRL;
   controllers[ccount].channels[ATA_SECONDARY].irq = ATA_IRQ_SECONDARY;
   controllers[ccount].channels[ATA_SECONDARY].ioport = ATA_IOPORT_SECONDARY;
   controllers[ccount].channels[ATA_SECONDARY].control_ioport = ATA_IOPORT_SECONDARYCTRL;
   
   printf("ata: falling back to motherboard IDE controller\n");
   dcount = ata_detect_drives(&(controllers[ccount]));
   
   return dcount;
}

int main(void)
{   
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */
   
   /* zero the controllers data structure and then populate it */
   memset(&controllers, 0, sizeof(controllers));
   if(discover_controllers() == 0) return 1; /* exit if there's no ATA drives */
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
