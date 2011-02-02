/* user/sbin/drivers/i386/ata/ata.h
 * Defines for the userspace ATAPI driver
 * Author : Chris Williams
 * Date   : Sun,16 Jan 2011.23:21:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _ATA_H
#define _ATA_H 1

#define ATA_PATHNAME_BASE "/dev/ata"

#define ATA_MAX_CONTROLLERS      (4)

#define ATA_IOPORT_PRIMARY       (0x1F0)
#define ATA_IOPORT_PRIMARYCTRL   (0x3F4)
#define ATA_IOPORT_SECONDARY     (0x170)
#define ATA_IOPORT_SECONDARYCTRL (0x374)

#define ATA_IRQ_PRIMARY          (14)
#define ATA_IRQ_SECONDARY        (15)

#define 

/* define a drive connected to a channel */
typedef struct
{
   unsigned char flags; /* set the type and status */
} ata_device;

/* define a controller channel */
typedef struct
{
   unsigned short ioport, control_ioport, busmaster_ioport;
   unsigned char irq;
   ata_device drive;
} ata_channel;

#define ATA_CONTROLLER_PCI       (1 << 0)  /* controller is a PCI device */
#define ATA_CONTROLLER_BUILTIN   (1 << 1)  /* controller is an ISA/chipset device */

/* define an ATA controller */
typedef struct
{
   unsigned char flags; /* set the type and status */

   /* PCI location */
   unsigned short bus, slot;
   
   /* PCI interrupt handling */
   unsigned char irq_line, irq_pin;
   
   /* hardware interfaces */
   ata_channel primary;
   ata_channel secondary;
} ata_controller;

/* prototype functions in fs.c */
void wait_for_request(void);

#endif
