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

/* detect_drive_type
   Detect the type of drive attached to a channel and convert the returned values into
   something sane
   => conroller = controller structure to work on
      slave = either ATA_SLAVE or ATA_MASTER to select primary or secondary device
   <= ata_drive_type number for the drive
*/
ata_drive_type detect_drive_type(ata_controller *controller, unsigned char slave)
{
	ata_soft_reset(ctrl->dev_ctl);		/* waits until master drive is ready again */

	outb(ctrl->base + REG_DEVSEL, 0xA0 | slavebit<<4);
	inb(ctrl->dev_ctl);			/* wait 400ns for drive select to work */
	inb(ctrl->dev_ctl);
	inb(ctrl->dev_ctl);
	inb(ctrl->dev_ctl);
	unsigned cl=inb(ctrl->base + REG_CYL_LO);	/* get the "signature bytes" */
	unsigned ch=inb(ctrl->base + REG_CYL_HI);
   
	/* differentiate ATA, ATAPI, SATA and SATAPI */
	if (cl==0x14 && ch==0xEB) return ata_type_patapi;
	if (cl==0x69 && ch==0x96) return ata_type_satapi;
	if (cl==0 && ch == 0) return ata_type_pata;
	if (cl==0x3c && ch==0xc3) return ata_type_sata;
   
	return ata_type_unknown;
}

/* detect_drives
   Discover the ATA/ATAPI drives attached to a controller's channels
   => controller = pointer to ata controller structure
   <= number of drives found connected to the controller
*/
unsigned char detect_drives(ata_controller *controller)
{
   
}

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
      
      err = pci_find_device(class, pci_loop, &bus, &slot, &pid);
      if(!err && pid == 0)
      {
         err = pci_claim_device(bus, slot, 0);
         if(!err)
         {
            unsigned short header_type;
            
            /* make sure the PCI card is valid */
            pci_read_config(bus, slot, 0, PCI_HEADER_TYPE, &header_type);
            if(header_type == PCI_HEADER_GENERIC)
            {               
               /* record the bus+slot */
               controllers[ccount].bus = bus;
               controllers[ccount].slot = slot;
               controllers[ccount].flags = ATA_CONTROLLER_PCI;
               
               /* discover the controller's IO ports - note that if any
                  of the IO port values comes back as 0x00 or 0x01 then assume the 
                  default port values */
               pci_read_config(bus, slot, 0, PCI_HEADER_BAR0_LOW, &(controllers[ccount].primary.ioport));
               if(controllers[ccount].primary.ioport < 0x02)
                  controllers[ccount].primary.ioport = ATA_IOPORT_PRIMARY;
               
               pci_read_config(bus, slot, 0, PCI_HEADER_BAR1_LOW, &(controllers[ccount].primary.control_ioport));
               if(controllers[ccount].primary.control_ioport < 0x02)
                  controllers[ccount].primary.control_ioport = ATA_IOPORT_PRIMARYCTRL;
               
               pci_read_config(bus, slot, 0, PCI_HEADER_BAR2_LOW, &(controllers[ccount].secondary.ioport));
               if(controllers[ccount].secondary.ioport < 0x02)
                  controllers[ccount].secondary.ioport = ATA_IOPORT_SECONDARY;
               
               pci_read_config(bus, slot, 0, PCI_HEADER_BAR3_LOW, &(controllers[ccount].secondary.control_ioport));
               if(controllers[ccount].secondary.control_ioport < 0x02)
                  controllers[ccount].secondary.control_ioport = ATA_IOPORT_SECONDARYCTRL;
               
               pci_read_config(bus, slot, 0, PCI_HEADER_BAR4_LOW, &(controllers[ccount].primary.busmaster_ioport));
               controllers[ccount].secondary.busmaster_ioport = controllers[ccount].primary.busmaster_ioport + 8;
               
               /* discover the IRQ lines */
               pci_read_config(bus, slot, 0, PCI_HEADER_INT_LINE, &(controllers[ccount].irq_line));
               pci_read_config(bus, slot, 0, PCI_HEADER_INT_PIN, &(controllers[ccount].irq_pin));
               controllers[ccount].primary.irq = ATA_IRQ_PRIMARY;
               controllers[ccount].secondary.irq = ATA_IRQ_SECONDARY;
               
               printf("ata: claimed controller in PCI bus %x slot %x irq %x\n",
                      bus, slot, controllers[ccount].irq_line);
               
               /* discover connected storage drives */
               dcount += detect_drives(&(controllers[ccount]));
               
               /* move onto next controller.. */
               ccount++;
            }
            else
            {
               /* hand back the PCI device if it's not suitable */
               printf("ata: unexpected header type %x in PCI controller (%x %x)\n",
                      header_type, bus, slot);
               pci_release_device(bus, slot);
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
   controllers[ccount].primary.irq = ATA_IRQ_PRIMARY;
   controllers[ccount].primary.ioport = ATA_IOPORT_PRIMARY;
   controllers[ccount].primary.control_ioport = ATA_IOPORT_PRIMARYCTRL;
   controllers[ccount].secondary.irq = ATA_IRQ_SECONDARY;
   controllers[ccount].secondary.ioport = ATA_IOPORT_SECONDARY;
   controllers[ccount].secondary.control_ioport = ATA_IOPORT_SECONDARYCTRL;
   
   printf("ata: falling back to motherboard IDE controller\n");
   dcount = detect_drives(&(controllers[ccount]));
   
   return dcount;
}

int main(void)
{   
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   if(discover_controllers() == 0) return 1; /* exit if there's no ATA drives */
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
