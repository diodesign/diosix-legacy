/* kernel/ports/arm/versatilepb/eeprom.c
 * ARM-specific low-level routines to read EEPROMs on the Versatile PB dev board
 * Author : Chris Williams
 * Date   : Sun,10 April 2011.03:35:00
 
 Copyright (c) Chris Williams and individual contributors
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
 
 */

#include <portdefs.h>

/* ---------------------------------------------------
    Communicate with the EEPROM via the I2C bus
   --------------------------------------------------- */

#define REG_SB_CONTROL  KERNEL_PHYS2LOG(0x10002000) /* read control bits  */
#define REG_SB_CONTROLS KERNEL_PHYS2LOG(0x10002000) /* set control bits   */
#define REG_SB_CONTROLC KERNEL_PHYS2LOG(0x10002004) /* clear control bits */

#define BIT_SCL   (1 << 0)
#define BIT_SDA   (1 << 1)

/* i2c_delay
   Stall for a moment during an i2c operation */
void i2c_delay(void)
{
   volatile unsigned int i;
   
   for(i = 0; i < 100; i++);
}

/* i2c_read_scl
   Return the state of the serial clock line */
unsigned char i2c_read_scl(void)
{
   /* the scl state is in bit 0 of the control reg */
   return CHIPSET_REG32(REG_SB_CONTROL) & BIT_SCL;
}

/* i2c_read_sda
   Set the serial data line and return its state */
unsigned char i2c_read_sda(void)
{
   /* the sda state is in bit 1 of the control reg */
   CHIPSET_REG32(REG_SB_CONTROLS) = BIT_SDA;
   return CHIPSET_REG32(REG_SB_CONTROL) & BIT_SDA;
}

/* i2c_clear_scl
   Clear the serial clock line */
void i2c_clear_scl(void)
{
   CHIPSET_REG32(REG_SB_CONTROLC) = BIT_SCL;
}

/* i2c_clear_sda
 Clear the serial data line */
void i2c_clear_sda(void)
{
   CHIPSET_REG32(REG_SB_CONTROLC) = BIT_SDA;
}

/* set if a start bit has been sent */
unsigned char start_flag = 0;

void i2c_start_cond(void)
{
   if(start_flag)
   {
      volatile unsigned char loop = 100;
      
      /* restart the command by setting SDA to 1 */
      i2c_read_sda();
      i2c_delay();
      
      /* clock stretching */
      while(i2c_read_scl() == 0 && loop) loop--;
   }
   
   /* toggle SDA */
   i2c_read_sda();
   i2c_clear_sda();
   i2c_delay();
   
   i2c_clear_scl();
   start_flag = 1;
}

void i2c_stop_cond(void)
{
   volatile unsigned char loop = 100;
   
   /* set SDA to 0 */
   i2c_clear_sda();
   i2c_delay();
   
   /* clock stretching */
   while(i2c_read_scl() == 0 && loop) loop--;
   
   /* and set SDA back to 1 */
   i2c_read_sda();
   i2c_delay();
   start_flag = 0;
}

/* i2c_write_bit
   Write a bit to I2C bus */
void i2c_write_bit(unsigned char bit)
{
   volatile unsigned char loop = 100;
   
   /* do the write */
   if(bit) 
      i2c_read_sda();
   else 
      i2c_clear_sda();
   
   i2c_delay();
   while(i2c_read_scl() == 0 && loop) loop--;

   /* set SDA, if it's zero then someone else is trying to
      master the bus and we should cope with this rather
      than ignore it... */
   if(bit) i2c_read_sda();
   
   i2c_delay();
   i2c_clear_scl();
}

/* i2c_read_bit
   Read a bit from I2C bus */
unsigned i2c_read_bit(void)
{
   volatile unsigned char loop = 100;
   unsigned bit;

   i2c_read_sda();
   i2c_delay();
   while(i2c_read_scl() == 0 && loop) loop--;
   
   /* read the data */
   bit = i2c_read_sda();
   
   loop = 100;
   while(i2c_read_scl() == 0 && loop) loop--;
   i2c_clear_scl();
   
   return bit;
}

/* i2c_write_byte
   Write a byte to I2C bus
   => send_start = 1 to send a start bit
      send_stop = 1 to send a stop bit
      byte = data to write out
   <= return 0 if acknowleged by the slave
*/
unsigned char i2c_write_byte(unsigned char send_start, unsigned char send_stop, unsigned char byte)
{
   unsigned char bit;
   unsigned char ack;

   if(send_start) i2c_start_cond();

   for(bit = 0; bit < 8; bit++) 
   {
      /* send most significant bit first */
      i2c_write_bit(byte & 0x80);
      byte = byte << 1;
   }

   ack = i2c_read_bit();

   if (send_stop) i2c_stop_cond();
   
   return ack;
}

/* i2c_read_byte
   Read a byte from I2C bus
   => ack = acknowlege bit to send
      send_stop = send stop bit 
   <= returns data read from the bus
*/
unsigned char i2c_read_byte(int ack, int send_stop)
{
   unsigned char byte = 0;
   unsigned char bit;
   
   for (bit = 0; bit < 8; bit++) 
   {
      byte = byte << 1;            
      byte |= i2c_read_bit();             
   }
   
   i2c_write_bit(ack);
   if(send_stop) i2c_stop_cond();

   return byte;
}

/* ---------------------------------------------------
    Read data from the EEPROM
   --------------------------------------------------- */

/* eeprom_read
   Read a random byte from an expansion EEPROM
   => bus_addr = bus address of eeprom chip
      mem_addr = address of byte to read from chip
   <= returns the byte read
*/
unsigned char eeprom_read(unsigned char bus_addr, unsigned int mem_addr)
{   
   /* mask off the low bit - it's always 0 for write, 1 for read */
   bus_addr &= ~1;
   
   /* send the start signal and bus address */
   i2c_write_byte(1, 0, bus_addr);
   
   /* now send the memory address, high address first */
   i2c_write_byte(0, 0, (mem_addr >> 8) & 0xff);
   i2c_write_byte(0, 0, mem_addr & 0xff);

   /* resend the bus address with read bit set */
   i2c_write_byte(1, 0, bus_addr | 1);
   
   return i2c_read_byte(1, 1);
}
