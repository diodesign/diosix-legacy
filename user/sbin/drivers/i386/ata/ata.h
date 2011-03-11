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

/* where to find the controllers in the IO space */
#define ATA_IOPORT_PRIMARY       (0x1F0)
#define ATA_IOPORT_PRIMARYCTRL   (0x3F4)
#define ATA_IOPORT_SECONDARY     (0x170)
#define ATA_IOPORT_SECONDARYCTRL (0x374)

#define ATA_IRQ_PRIMARY          (14)
#define ATA_IRQ_SECONDARY        (15)

/* command/status bits */
#define ATA_SR_BSY               (0x80)
#define ATA_SR_DRDY              (0x40)
#define ATA_SR_DF                (0x20)
#define ATA_SR_DSC               (0x10)
#define ATA_SR_DRQ               (0x08)
#define ATA_SR_CORR              (0x04)
#define ATA_SR_IDX               (0x02)
#define ATA_SR_ERR               (0x01)

/* features/errors bits */
#define ATA_ER_BBK               (0x80)
#define ATA_ER_UNC               (0x40)
#define ATA_ER_MC                (0x20)
#define ATA_ER_IDNF              (0x10)
#define ATA_ER_MCR               (0x08)
#define ATA_ER_ABRT              (0x04)
#define ATA_ER_TK0NF             (0x02)
#define ATA_ER_AMNF              (0x01)

/* ATA command list */
#define ATA_CMD_READ_PIO         (0x20)
#define ATA_CMD_READ_PIO_EXT     (0x24)
#define ATA_CMD_READ_DMA         (0xc8)
#define ATA_CMD_READ_DMA_EXT     (0x25)
#define ATA_CMD_WRITE_PIO        (0x30)
#define ATA_CMD_WRITE_PIO_EXT    (0x34)
#define ATA_CMD_WRITE_DMA        (0xca)
#define ATA_CMD_WRITE_DMA_EXT    (0x35)
#define ATA_CMD_CACHE_FLUSH      (0xe7)
#define ATA_CMD_CACHE_FLUSH_EXT  (0xea)
#define ATA_CMD_PACKET           (0xa0)
#define ATA_CMD_IDENTIFY_PACKET  (0xa1)
#define ATA_CMD_IDENTIFY         (0xec)

/* ATAPI-specific commands */
#define ATAPI_CMD_READ           (0xA8)
#define ATAPI_CMD_EJECT          (0x1b)

/* ATA_CMD_IDENTIFY_PACKET and ATA_CMD_IDENTIFY return
   a 512-byte block of information; these are the
   offsets we're interested in */
#define ATA_IDENT_DEVICETYPE     (0)
#define ATA_IDENT_CYLINDERS      (2)
#define ATA_IDENT_HEADS          (6)
#define ATA_IDENT_SECTORS        (12)
#define ATA_IDENT_SERIAL         (20)
#define ATA_IDENT_MODEL          (54)
#define ATA_IDENT_CAPABILITIES   (98)
#define ATA_IDENT_FIELDVALID     (106)
#define ATA_IDENT_MAX_LBA        (120)
#define ATA_IDENT_COMMANDSETS    (164)
#define ATA_IDENT_MAX_LBA_EXT    (200)

/* each channel has a number of registers in IO space. these are the
   offsets from BAR0 and BAR1 (or BAR2 and BAR3) */
#define ATA_REG_DATA             (0x00)   /* BAR0+0  RW */
#define ATA_REG_ERROR            (0x01)   /* BAR0+1  R  */
#define ATA_REG_FEATURES         (0x01)   /* BAR0+1  W  */
#define ATA_REG_SECCOUNT0        (0x02)   /* BAR0+2  RW */
#define ATA_REG_LBA0             (0x03)   /* BAR0+3  RW */
#define ATA_REG_LBA1             (0x04)   /* BAR0+3  RW */
#define ATA_REG_LBA2             (0x05)   /* BAR0+5  RW */
#define ATA_REG_HDDEVSEL         (0x06)   /* BAR0+6  RW */
#define ATA_REG_COMMAND          (0x07)   /* BAR0+7  W  */
#define ATA_REG_STATUS           (0x07)   /* BAR0+7  R  */
#define ATA_REG_SECCOUNT1        (0x08)   /* BAR0+8  RW */
#define ATA_REG_LBA3             (0x09)   /* BAR0+9  RW */
#define ATA_REG_LBA4             (0x0a)   /* BAR0+10 RW */
#define ATA_REG_LBA5             (0x0b)   /* BAR0+11 RW */
#define ATA_REG_CONTROL          (0x0c)   /* BAR1+12 R  */
#define ATA_REG_ALTSTATUS        (0x0c)   /* BAR1+2  W  */

/* select the correct interface, drive and channel */
#define SELECT_ATA               (0x00)
#define SELECT_ATAPI             (0x01)
#define ATA_MASTER               (0x00)
#define ATA_SLAVE                (0x01)
#define ATA_PRIMARY              (0x00)
#define ATA_SECONDARY            (0x01)

#define ATA_READ                 (0x00)
#define ATA_WRITE                (0x01)

/* supported drives */
typedef enum
{
   ata_type_unknown,
   ata_type_patapi, ata_type_satapi,
   ata_type_pata, ata_type_sata;
} ata_drive_type;

/* define a drive connected to a channel */
typedef struct
{
   ata_drive_type type;
   unsigned char flags; /* set status */
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
   unsigned short irq_line, irq_pin;
   
   /* hardware interfaces */
   ata_channel primary;
   ata_channel secondary;
} ata_controller;

/* prototype functions in fs.c */
void wait_for_request(void);

#endif
