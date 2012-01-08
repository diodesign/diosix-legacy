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
#define ATA_IOPORT_PRIMARY        (0x1F0)
#define ATA_IOPORT_PRIMARYCTRL    (0x3F4)
#define ATA_IOPORT_SECONDARY      (0x170)
#define ATA_IOPORT_SECONDARYCTRL  (0x374)

/* default ISA IRQ lines */
#define ATA_IRQ_PRIMARY           (X86_PIC_IRQ_BASE + 14)
#define ATA_IRQ_SECONDARY         (X86_PIC_IRQ_BASE + 15)

/* bits for the IRQ pending flag in te driver */
#define ATA_IRQ_PRIMARY_PENDING   (1 << 0)
#define ATA_IRQ_SECONDARY_PENDING (1 << 1)

/* command/status bits */
#define ATA_SR_BSY                (1 << 7)
#define ATA_SR_DRDY               (1 << 6)
#define ATA_SR_DFAULT             (1 << 5)
#define ATA_SR_DSEEKCCOMPLETE     (1 << 4)
#define ATA_SR_DRQ                (1 << 3)
#define ATA_SR_CORR               (1 << 2)
#define ATA_SR_IDX                (1 << 1)
#define ATA_SR_ERR                (1 << 0)

/* features/errors bits */
#define ATA_ER_BBK                (0x80)
#define ATA_ER_UNC                (0x40)
#define ATA_ER_MC                 (0x20)
#define ATA_ER_IDNF               (0x10)
#define ATA_ER_MCR                (0x08)
#define ATA_ER_ABRT               (0x04)
#define ATA_ER_TK0NF              (0x02)
#define ATA_ER_AMNF               (0x01)

/* device control bits */
#define ATA_DC_NIEN               (1 << 1)
#define ATA_DC_SRST               (1 << 2)
#define ATA_DC_HOB                (1 << 7)

/* ATA command list */
#define ATA_CMD_READ_PIO          (0x20)
#define ATA_CMD_READ_PIO_EXT      (0x24)
#define ATA_CMD_READ_DMA          (0xc8)
#define ATA_CMD_READ_DMA_EXT      (0x25)
#define ATA_CMD_WRITE_PIO         (0x30)
#define ATA_CMD_WRITE_PIO_EXT     (0x34)
#define ATA_CMD_WRITE_DMA         (0xca)
#define ATA_CMD_WRITE_DMA_EXT     (0x35)
#define ATA_CMD_CACHE_FLUSH       (0xe7)
#define ATA_CMD_CACHE_FLUSH_EXT   (0xea)
#define ATA_CMD_PACKET            (0xa0)
#define ATA_CMD_IDENTIFY_PACKET   (0xa1)
#define ATA_CMD_IDENTIFY          (0xec)

/* ATAPI-specific commands */
#define ATAPI_CMD_READ            (0xA8)
#define ATAPI_CMD_EJECT           (0x1b)

/* ATA_CMD_IDENTIFY_PACKET and ATA_CMD_IDENTIFY return
   a 256 x 16-bit block of information; these are the
   16-bit word indices we're interested in */
#define ATA_IDENT_DEVICETYPE      (0)
#define ATA_IDENT_SERIALSTART     (10)
#define ATA_IDENT_SERIALEND       (19)
#define ATA_IDENT_MODELSTART      (27)
#define ATA_IDENT_MODELEND        (46)
#define ATA_IDENT_CAPABILITIES    (49)
#define ATA_IDENT_FIELDVALID      (53)
#define ATA_IDENT_MAX_LBA0        (60)
#define ATA_IDENT_MAX_LBA1        (61)
#define ATA_IDENT_COMMANDSET0     (82)
#define ATA_IDENT_COMMANDSET1     (83)
#define ATA_IDENT_COMMANDSET2     (84)
#define ATA_IDENT_COMMANDSET3     (85)
#define ATA_IDENT_COMMANDSET4     (86)
#define ATA_IDENT_COMMANDSET5     (87)
#define ATA_IDENT_ULTRADMAMODE    (88)
#define ATA_IDENT_MAX_LBA_EXT0    (100)
#define ATA_IDENT_MAX_LBA_EXT1    (101)
#define ATA_IDENT_MAX_LBA_EXT2    (102)
#define ATA_IDENT_MAX_LBA_EXT3    (103)
#define ATA_IDENT_SECTORSIZE      (106)
#define ATA_IDENT_LOGSECSIZELO    (117)
#define ATA_IDENT_LOGSECSIZEHI    (118)
#define ATA_IDENT_MAXWORDS        (256) /* number of 16-bit words in the data block */

/* each channel has a number of registers in IO space. these are the
   offsets from BAR0 and BAR1 (or BAR2 and BAR3 for secondary channels) */
#define ATA_REG_DATA              (0x00)   /* BAR0+0  RW */
#define ATA_REG_ERROR             (0x01)   /* BAR0+1  R  */
#define ATA_REG_FEATURES          (0x01)   /* BAR0+1  W  */
#define ATA_REG_SECTORCOUNT       (0x02)   /* BAR0+2  RW */
#define ATA_REG_LBA0              (0x03)   /* BAR0+3  RW */
#define ATA_REG_LBA1              (0x04)   /* BAR0+3  RW */
#define ATA_REG_LBA2              (0x05)   /* BAR0+5  RW */
#define ATA_REG_HDDEVSEL          (0x06)   /* BAR0+6  RW */
#define ATA_REG_COMMAND           (0x07)   /* BAR0+7  W  */
#define ATA_REG_STATUS            (0x07)   /* BAR0+7  R  */
#define ATA_REG_OFFSET            (0x10)   /* use this to differentiate command IO ports and control IO ports */
#define ATA_REG_CONTROL           (0x02 + ATA_REG_OFFSET)   /* BAR1+2  W  */
#define ATA_REG_ALTSTATUS         (0x02 + ATA_REG_OFFSET)   /* BAR1+2  R  */

/* select the correct interface, drive and channel */
#define SELECT_ATA                (0x00)
#define SELECT_ATAPI              (0x01)
#define ATA_MASTER                (0x00)
#define ATA_SLAVE                 (0x01)
#define ATA_MAX_DEVS_PER_CHANNEL  (2)
#define ATA_PRIMARY               (0x00)
#define ATA_SECONDARY             (0x01)
#define ATA_MAX_CHANS_PER_CNTRLR  (2)
#define ATA_SELECT_DRIVE_SHIFT    (4)

#define ATA_READ                  (0x00)
#define ATA_WRITE                 (0x01)

/* supported drives */
typedef enum
{
   ata_type_unknown,
   ata_type_patapi, ata_type_satapi,
   ata_type_pata, ata_type_sata
} ata_drive_type;

/* drive status flags */
#define ATA_DEVICE_PRESENT       (1 << 0)
#define ATA_DEVICE_IS_ATA        (1 << 1)
#define ATA_DEVICE_IS_ATAPI      (1 << 2)

/* how a device will describe itself when IDENTIFY'd */
typedef struct
{
   ata_drive_type type;
   unsigned short word[ATA_IDENT_MAXWORDS];
} ata_identify_data;

/* define a drive connected to a channel */
typedef struct
{
   unsigned char flags; /* set status */
   
   /* PID and file descriptor for the process holding the device open */
   unsigned int pid;
   int filedesc;
   
   ata_identify_data identity; /* identify data from the drive */
} ata_device;

/* define a controller channel */
typedef struct
{
   /* define the base IO port addresses for this channel */
   unsigned short ioport, control_ioport, busmaster_ioport;
   unsigned char irq, nIEN; /* nIEN is no interrupt */
   
   /* max of two devices per channel */
   ata_device drive[ATA_MAX_DEVS_PER_CHANNEL];
} ata_channel;

/* controller flags */
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
   
   /* hardware interfaces, max two per controller */
   ata_channel channels[ATA_MAX_CHANS_PER_CNTRLR];
} ata_controller;

extern ata_controller controllers[ATA_MAX_CONTROLLERS];
extern volatile unsigned char ata_lock;

/* prototypes for functions in ata.c */
unsigned char ata_detect_drives(ata_controller *controller);
unsigned char ata_read_register(ata_controller *controller, unsigned char channel, unsigned char reg);
void ata_write_register(ata_controller *controller, unsigned char chan, unsigned char reg, unsigned char data);

/* prototype functions in fs.c and irq.c */
void wait_for_request(void);
void wait_for_irq(void);
void clear_irq(unsigned char flags);

/* prototypes for the filesystem-facing code in fs.c */
void fs_init(void);

#endif
