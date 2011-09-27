/* user/sbin/drivers/i386/ata/ata.c
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
unsigned short ata_read_word(ata_controller *controller, unsigned char chan, unsigned char reg)
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
   
   write_port_byte(ioport, data);
}

/* ata_wait_for_ready
   Spin until a controller is ready to accept a new command or have one of its registers written to.
   => channel = channel to select (ATA_PRIMARY or ATA_SECONDARY)
      controller = pointer to drive's controller structure
*/
void ata_wait_for_ready(unsigned char channel, ata_controller *controller)
{      
   /* assumes sane parameters */
   while(!(ata_read_register(controller, channel, ATA_REG_STATUS) & ATA_SR_DRDY));
}

/* ata_print_string
   Dump a string of ASCII characters from an ATA identify data block to stdout, omitting
   a newline and any non-ASCII bytes
   => data = identify data block structure
      start = first 16bit word to read
      end = last 16bit word, inclusive, to read
*/
void ata_print_string(ata_identify_data *data, unsigned short start, unsigned short end)
{
   unsigned short count = start;
   
   while(count <= end)
   {
      unsigned char high = data->word[count] & 0xff;
      unsigned char low = (data->word[count] >> 8) & 0xff;

      if(low > 31 && low < 128) printf("%c", low);
      if(high > 31 && high < 128) printf("%c", high);
      
      count++;
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
   ata_write_register(controller, channel, ATA_REG_HDDEVSEL, (drive << ATA_SELECT_DRIVE_SHIFT));
   return success;
}

/* ata_identify_packet_device
   Send an identify packet command to an [P/S]ATAPI drive. Note that this function will not
   transfer the identify data from the drive.
   => drive = drive to select (ATA_MASTER or ATA_SLAVE)
      channel = channel to select (ATA_PRIMARY or ATA_SECONDARY)
      controller = pointer to drive's controller structure
   <= 0 for success, or an error code
*/
kresult ata_identify_packet_device(unsigned char drive, unsigned char channel, ata_controller *controller)
{
   unsigned char status;
   
   /* select the correct drive */
   if(ata_select_device(drive, channel, controller) != success) return e_bad_params;
   
   /* it's recommended to zero these registers, plus it introduces a delay */
   ata_write_register(controller, channel, ATA_REG_SECTORCOUNT, 0);
   ata_write_register(controller, channel, ATA_REG_LBA0, 0);
   ata_write_register(controller, channel, ATA_REG_LBA1, 0);
   ata_write_register(controller, channel, ATA_REG_LBA2, 0);
   
   /* send the packet command */
   ata_write_register(controller, channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
   
   /* only read the status reg once per transfer */
   status = ata_read_register(controller, channel, ATA_REG_STATUS);
   
   /* wait until we get a response from the device */
   while(1)
   {
      /* status isn't allowed to be zero, according to the specs,
         unless the device simply isn't present */
      if(!status) return e_not_found;
      
      /* record the fact that the drive rejected the identify packet command */
      if(status & ATA_SR_ERR) return e_failure;
      
      if(status & ATA_SR_DRQ) return success; /* device is ready */
      
      /* it's safe to read the alternative status register */
      status = ata_read_register(controller, channel, ATA_REG_ALTSTATUS);
   }
}

/* ata_identify_device
   Interrogate a drive and store its parameter information
   => drive = drive to select (ATA_MASTER or ATA_SLAVE)
      channel = channel to select (ATA_PRIMARY or ATA_SECONDARY)
      controller = pointer to drive's controller structure
      data = pointer to 256-byte block to save the identification data into
             plus any other information about the drive
   <= 0 for success, or an error code
*/
kresult ata_identify_device(unsigned char drive, unsigned char channel, ata_controller *controller, ata_identify_data *data)
{
   unsigned char status, low, high, identify_error = 0;
   unsigned short loop;

   /* sanity checks */
   if(!controller || !data) return e_bad_params;
   
   /* select the correct drive */
   if(ata_select_device(drive, channel, controller) != success) return e_bad_params;
   
   /* it's recommended to zero these registers, plus it introduces a delay */
   ata_write_register(controller, channel, ATA_REG_SECTORCOUNT, 0);
   ata_write_register(controller, channel, ATA_REG_LBA0, 0);
   ata_write_register(controller, channel, ATA_REG_LBA1, 0);
   ata_write_register(controller, channel, ATA_REG_LBA2, 0);
   
   /* send the identify command */
   ata_write_register(controller, channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
   
   /* read the signature bytes to determine the type of drive */
   low = ata_read_register(controller, channel, ATA_REG_LBA1);
   high = ata_read_register(controller, channel, ATA_REG_LBA2);
   
   /* only read the status reg once per transfer */
   status = ata_read_register(controller, channel, ATA_REG_STATUS);

   /* wait until we get a response from the device */
   while(1)
   {      
      /* status isn't allowed to be zero, according to the specs,
         unless the device simply isn't present */
      if(!status) return e_not_found;

      /* record the fact that the drive rejected the identify command
         this may be because it's an ATAPI drive that will only accept identify packet */
      if(status & ATA_SR_ERR)
      {
         identify_error = 1;
         break;
      }
      
      if(status & ATA_SR_DRQ) break; /* device is ready */
      
      /* it's safe to repeatedly read the alternative status register */
      status = ata_read_register(controller, channel, ATA_REG_ALTSTATUS);
   }
   
   /* resend the identify command as identify packet if the PATAPI drive complained */
   if(low == 0x14 && high == 0xeb)
   {
      printf("ata: drive %i in channel %i is a PATAPI device\n", drive, channel);
      if(identify_error)
         if(ata_identify_packet_device(drive, channel, controller) != success)
            return e_failure;
      
      data->type = ata_type_patapi;
      goto ata_identify_device_read;
   }
   
   /* resend the identify command as identify packet if the SATAPI drive complained */
   if(low == 0x69 && high == 0x96)
   {
      printf("ata: drive %i in channel %i is a SATAPI device\n", drive, channel);
      if(identify_error)
         if(ata_identify_packet_device(drive, channel, controller) != success)
            return e_failure;
      
      data->type = ata_type_satapi;
      goto ata_identify_device_read;
   }

   if(low == 0x00 && high == 0x00)
   {
      printf("ata: drive %i in channel %i is a PATA device (error flag %i)\n", drive, channel, identify_error);
      if(identify_error) return e_failure;
      
      data->type = ata_type_pata;
      goto ata_identify_device_read;
   }
   
	if(low == 0x3c && high == 0xc3)
   {
      printf("ata: drive %i in channel %i is a SATA device (error flag %i)\n", drive, channel, identify_error);
      if(identify_error) return e_failure;
      
      data->type = ata_type_sata;
      goto ata_identify_device_read;
   }

   /* fall through to reject drive */
   printf("ata: drive %i in channel %i is unrecognised [%x %x]\n", drive, channel, low, high);
   data->type = ata_type_unknown;
   return e_failure;
   
ata_identify_device_read:
   /* now read in the 256 16-bit words */
   for(loop = 0; loop < ATA_IDENT_MAXWORDS; loop++)
      data->word[loop] = ata_read_word(controller, channel, ATA_REG_DATA);
   
   /* dump some diagnostic info */
   printf("ata: serial: ");
   ata_print_string(data, ATA_IDENT_SERIALSTART, ATA_IDENT_SERIALEND);
   printf(" model: ");
   ata_print_string(data, ATA_IDENT_MODELSTART, ATA_IDENT_MODELEND);
   printf("\n");
   
   return success;
}


/* ata_detect_drives
   Discover the ATA/ATAPI drives attached to a controller's channels
   => controller = pointer to ata controller structure
   <= number of drives found connected to the controller
*/
unsigned char ata_detect_drives(ata_controller *controller)
{
   unsigned char drive, channel, found = 0;
   ata_identify_data data;
   
   /* iterate through the possible channels and drives */
   for(channel = 0; channel < ATA_MAX_CHANS_PER_CNTRLR; channel++)
   {
      /* clear the 48bit HOB bit, disable IRQs for the moment */
      ata_write_register(controller, channel, ATA_REG_CONTROL, ATA_DC_NIEN);
      
      for(drive = 0; drive < ATA_MAX_DEVS_PER_CHANNEL; drive++)
      {
         if(ata_identify_device(drive, channel, controller,
                                &(controller->channels[channel].drive[drive])) == success)
            found++;
      }
      
      /* reenable interrupts */
      ata_write_register(controller, channel, ATA_REG_CONTROL, 0);
   }
   
   return found;
}

