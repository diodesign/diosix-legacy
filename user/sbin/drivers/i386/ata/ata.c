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


/* ata_read_register
   Read a register from the controller for a given channel
   => controller = pointer to structure defining the controller to probe
      chan = channel to access (ATA_PRIMARY or ATA_SECONDARY)
      reg = the ATA register to read
   <= returns the byte from the controller hardware
*/
unsigned char ata_read_register(ata_controller *controller, unsigned char chan, unsigned char reg)
{
   unsigned short ioport;

   ata_channel *channel = &(controller->channels[chan]);
   
   /* translate an internal ATA register number into an offset from the relevant x86 IO port base */
   switch(reg)
   {
      /* registers accessable from BAR0 (or BAR2 for secondary) aka the base IO port */
      case ATA_REG_DATA:
      case ATA_REG_ERROR:
      case ATA_REG_SECTORCOUNT:
      case ATA_REG_LBA0:
      case ATA_REG_LBA1:
      case ATA_REG_LBA2:
      case ATA_REG_HDDEVSEL:
      case ATA_REG_STATUS:
         ioport = reg + channel->ioport;
         break;
         
      /* registers accessable from BAR1 (or BAR3 for secondary) aka the base control IO port */
      case ATA_REG_ALTSTATUS:
         ioport = (reg - ATA_REG_OFFSET) + channel->control_ioport;
         break;
         
      default:
         printf("ata: ata_read_register(): bad register %x\n", reg);
         return 0xff;
   }
   
   return read_port_byte(ioport);
}

/* ata_read_word
   Read a 16-bit word from a register in a controller for a given channel
   => controller = pointer to structure defining the controller to probe
      chan = channel to access (ATA_PRIMARY or ATA_SECONDARY)
      reg = the ATA register to read
   <= returns the word from the controller hardware
*/
unsigned char ata_read_word(ata_controller *controller, unsigned char chan, unsigned char reg)
{
   unsigned short ioport;
   
   ata_channel *channel = &(controller->channels[chan]);
   
   /* translate an internal ATA register number into an offset from the relevant x86 IO port base */
   switch(reg)
   {
      /* registers accessable from BAR0 (or BAR2 for secondary) aka the base IO port */
      case ATA_REG_DATA:
      case ATA_REG_ERROR:
      case ATA_REG_SECTORCOUNT:
      case ATA_REG_LBA0:
      case ATA_REG_LBA1:
      case ATA_REG_LBA2:
      case ATA_REG_HDDEVSEL:
      case ATA_REG_STATUS:
         ioport = reg + channel->ioport;
         break;
         
         /* registers accessable from BAR1 (or BAR3 for secondary) aka the base control IO port */
      case ATA_REG_ALTSTATUS:
         ioport = (reg - ATA_REG_OFFSET) + channel->control_ioport;
         break;
         
      default:
         printf("ata: ata_read_word(): bad register %x\n", reg);
         return 0xff;
   }
   
   return read_port(ioport);
}

/* ata_write_register
   Write to a register in the controller for a given channel
   => controller = pointer to structure defining the controller to contact
      chan = channel to access (ATA_PRIMARY or ATA_SECONDARY)
      reg = the ATA register to write to
      data = byte to write
   <= returns the byte from the controller hardware
*/
void ata_write_register(ata_controller *controller, unsigned char chan, unsigned char reg, unsigned char data)
{
   unsigned short ioport;
   
   printf("ata_write_register: request to write %x to channel %i register %x\n", data, chan, reg);
   ata_channel *channel = &(controller->channels[chan]);
   
   /* translate an internal ATA register number into an offset from the relevant x86 IO port base */
   switch(reg)
   {
      /* registers accessable from BAR0 (or BAR2 for secondary) aka the base IO port */
      case ATA_REG_DATA:
      case ATA_REG_FEATURES:
      case ATA_REG_SECTORCOUNT:
      case ATA_REG_LBA0:
      case ATA_REG_LBA1:
      case ATA_REG_LBA2:
      case ATA_REG_HDDEVSEL:
      case ATA_REG_COMMAND:
         ioport = reg + channel->ioport;
         break;
         
      /* registers accessable from BAR1 (or BAR3 for secondary) aka the base control IO port */
      case ATA_REG_CONTROL:
         ioport = (reg - ATA_REG_OFFSET) + channel->control_ioport;
         break;
         
      default:
         printf("ata: ata_write_register(): bad register %x (data %x)\n", reg, data);
         return;
   }
   
   printf("ata_write_register: writing %x to ioport %x\n", data, ioport);
   write_port_byte(ioport, data);
   printf("ata_write_register: wrote %x to ioport %x\n", data, ioport);
}

/* ata_wait_for_ready
   Spin until a controller is ready to accept a new command or have one of its registers written to.
   In the status register, BSY and DRQ must both be cleared to zero and DMACK- is not asserted.
   => channel = channel to select (ATA_PRIMARY or ATA_SECONDARY)
      controller = pointer to drive's controller structure
*/
void ata_wait_for_ready(unsigned char channel, ata_controller *controller)
{
   unsigned char status = 0xff;
   
   printf("ata_wait_for_ready: channel = %i controller = %p data = %x\n",
          channel, controller, ata_read_register(controller, channel, ATA_REG_STATUS));
   
   /* assumes sane parameters */
   while(ata_read_register(controller, channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRDY))
   {
      unsigned char byte = ata_read_register(controller, channel, ATA_REG_STATUS);
      if(byte != status) printf("ata_wait_for_ready: status = %x\n", byte);
      status = byte;
   }
}

/* ata_select_device
   Select the given drive as the active drive for subsequent commands and accesses
    => drive = drive to select (ATA_MASTER or ATA_SLAVE)
       channel = channel to select (ATA_PRIMARY or ATA_SECONDARY)
       controller = pointer to drive's controller structure
   <= 0 for success, or an error code
*/
kresult ata_select_device(unsigned char drive, unsigned char channel, ata_controller *controller)
{
   /* sanity checks */
   if(!controller) return e_bad_params;
   if(drive != ATA_MASTER && drive != ATA_SLAVE) return e_bad_params;
   if(channel != ATA_PRIMARY && channel != ATA_SECONDARY) return e_bad_params;
   
   /* write the select bit in the hdd select register */
   ata_wait_for_ready(channel, controller);
   ata_write_register(controller, channel, ATA_REG_HDDEVSEL, drive << ATA_SELECT_DRIVE_SHIFT);
   return success;
}

/* ata_identify_device
   Interrogate a drive and store its parameter information
   => drive = drive to select (ATA_MASTER or ATA_SLAVE)
      channel = channel to select (ATA_PRIMARY or ATA_SECONDARY)
      controller = pointer to drive's controller structure
      data = pointer to 256-byte block to save the identification data into
   <= 0 for success, or an error code
*/
kresult ata_identify_device(unsigned char drive, unsigned char channel, ata_controller *controller, ata_identify_data *data)
{
   unsigned short loop;
   unsigned char check_for_atapi = 0;
   
   /* sanity checks */
   if(!controller || !data) return e_bad_params;
   
   /* select the correct drive */
   printf("ata_identify_device: selecting channel %i drive %i\n", channel, drive);
   if(ata_select_device(drive, channel, controller) != success) return e_bad_params;
   diosix_thread_sleep(1);
   
   /* send the identify command */
   ata_write_register(controller, channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
   diosix_thread_sleep(1);
   
   /* wait until we get a response from the device */
   printf("waiting...\n");
   while(1)
   {
      unsigned char status = ata_read_register(controller, channel, ATA_REG_STATUS);
      
      if(status & ATA_SR_ERR)
      {
         /* this may be an ATAPI device, so check it out */
         check_for_atapi = 1;
         break;
      }
      
      if(!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
   }
   
   /* check if it's an ATAPI drive, if necessary, and fire the correct
      identify packet at it */
   if(check_for_atapi)
   {
      unsigned char cl = ata_read_register(controller, channel, ATA_REG_LBA1);
      unsigned char ch = ata_read_register(controller, channel, ATA_REG_LBA2);
      
      if((cl == 0x14 && ch == 0xeb) || (cl == 0x69 && ch == 0x96))
      {
         printf("ata_identify_device: drive is an ATAPI device\n");
         ata_write_register(controller, channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
         diosix_thread_sleep(1);
      }
   }
   
   printf("ata_identify_device: reading identify data\n");
   
   /* now read in the 256 16-bit words */
   for(loop = 0; loop < ATA_IDENT_MAXWORDS; loop++)
   {
      data->word[loop] = ata_read_word(controller, channel, ATA_REG_DATA);
      printf("ata_identify_device: word %i = %x\n", loop, data->word[loop]);
   }
   
   return success;
}


/* ata_detect_drives
   Discover the ATA/ATAPI drives attached to a controller's channels
   => controller = pointer to ata controller structure
   <= number of drives found connected to the controller
*/
unsigned char ata_detect_drives(ata_controller *controller)
{
   unsigned char drive, channel;
   ata_identify_data data;
   
   /* iterate through the possible channels and drives */
   for(channel = 0; channel < ATA_MAX_CHANS_PER_CNTRLR; channel++)
   {
      /* disable IRQs for the moment */
      ata_write_register(controller, channel, ATA_REG_CONTROL, 2);
      
      for(drive = 0; drive < ATA_MAX_DEVS_PER_CHANNEL; drive++)
      {
         printf("ata_detect_drives: attempting to identify drive\n");
         if(ata_identify_device(drive, channel, controller, &data) == success)
         {
            printf("ata: identified device! sector count = %x %x\n",
                   data.word[ATA_IDENT_MAX_LBA1], data.word[ATA_IDENT_MAX_LBA0]);
         }
      }
   }
}

