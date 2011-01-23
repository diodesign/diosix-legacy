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
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"

#include "atapi.h"
#include "lowlevel.h"
#include "pci.h"

/* find_controllers
   Detect and setup ATAPI controllers and return number of devices */
unsigned int find_controllers(void)
{
   unsigned char count = 0;
   unsigned short class = PCI_CLASS_CALC(PCI_CLASS_MASSSTORAGE, PCI_SUBCLASS_IDE);
   kresult err;
   
   /* run through the available IDE controllers on the PCI sub-system */
   do
   {
      pci_find_device(class, count, &bus, &slot, );
      
      count++;
   }
   while(err == success);
}

int main(void)
{   
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */

   if(find_controllers == 0) return; /* exit if there's no ATAPI controllers */
      
   /* register this device with the vfs */
   diosix_vfs_register(DEVICE_PATHNAME);
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
