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

/* find_controllers
   Detect and setup ATAPI controllers and return number of devices */
unsigned int find_controllers(void)
{
   unsigned char dcount = 0, ccount = 0;
   kresult err;
   
   /* try to find mass storage IDE controllers */
   unsigned short class = PCI_CLASS_CALC(PCI_CLASS_MASSSTORAGE, PCI_SUBCLASS_IDE);
   
   /* run through the available controllers on the PCI sub-system */
   do
   {
      unsigned short bus, slot;
      unsigned int pid;
      
      err = pci_find_device(class, dcount, &bus, &slot, &pid);
      if(!err && pid == 0)
      {
         /* path includes 2 digits and NULL byte */
         unsigned int path_length = strlen(ATA_PATHNAME_BASE) + (3 * sizeof(char));
         char *pathname = malloc(path_length);
         if(!pathname)
         {
            printf("ata: malloc() failed during controller discovery\n");
            return ccount;
         }
         
         err = pci_claim_device(bus, slot, 0);
         if(!err)
         {
            /* register this device with the vfs */         
            snprintf(pathname, path_length, "%s%i", ATA_PATHNAME_BASE, ccount);
            if(diosix_vfs_register(pathname) == success)
            {
               /* record the bus+slot */
               controllers[ccount].bus = bus;
               controllers[ccount].slot = slot;
               controllers[ccount].pathname = pathname;
            
               printf("claimed controller in PCI bus %x slot %x (created %s)\n", bus, slot, pathname);
               ccount++;
            }
            else
               /* failed! */
               free(pathname);
         }
         else
            /* failed! */
            free(pathname);
      }
      
      dcount++;
   }
   while(!err && ccount < ATA_MAX_CONTROLLERS);
   
   return ccount;
}

int main(void)
{   
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   if(find_controllers() == 0) return 1; /* exit if there's no ATAPI controllers */
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
