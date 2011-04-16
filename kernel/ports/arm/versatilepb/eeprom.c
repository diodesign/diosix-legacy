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

/* I2C bus addresses of memory expansion EEPROMs */
#define EEPROM_DRAM      (0xa0)  /* dynamic memory */
#define EEPROM_SRAM      (0xa2)  /* static memory */

#define REG_SB_CONTROL  KERNEL_PHYS2LOG(0x10002000) /* read control bits  */
#define REG_SB_CONTROLS KERNEL_PHYS2LOG(0x10002000) /* set control bits   */
#define REG_SB_CONTROLC KERNEL_PHYS2LOG(0x10002004) /* clear control bits */

#define BIT_SCL      (1 << 0)
#define BIT_SDA      (1 << 1)

#define I2C_TIMEOUT  (0xff)

/* i2c_delay
   Stall for a moment during an i2c operation */
void i2c_delay(void)
{
   volatile unsigned char timeout;
   
   for(timeout = 0; timeout < I2C_TIMEOUT; timeout++);
}

/* i2c_set_sda
   Set the serial data */
void i2c_set_sda(void)
{   
   /* the sda state is in bit 1 of the control reg */
   CHIPSET_REG32(REG_SB_CONTROLS) = BIT_SDA;
}

/* i2c_set_scl
 Bring the clock line high */
void i2c_set_scl(void)
{
   CHIPSET_REG32(REG_SB_CONTROLS) = BIT_SCL;
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

/* i2c_read_scl
 Return the state of the serial clock line */
unsigned char i2c_read_scl(void)
{   
   /* the scl state is in bit 0 of the control reg */
   return !!(CHIPSET_REG32(REG_SB_CONTROL) & BIT_SCL);
}

/* i2c_read_sda
 Return data line's state */
unsigned char i2c_read_sda(void)
{   
   i2c_set_sda();
   /* the sda state is in bit 1 of the control reg */
   return !!(CHIPSET_REG32(REG_SB_CONTROL) & BIT_SDA);
}

void i2c_send_start(void)
{   
   /* take data and clock high, then toggle */
   i2c_set_sda();
   i2c_delay();
   
   i2c_set_scl();
   i2c_delay();
   
   i2c_clear_sda();
   i2c_delay();
   
   i2c_clear_scl();
   i2c_delay();
}

void i2c_send_stop(void)
{   
   /* set SDA to 0 */
   i2c_clear_sda();
   i2c_delay();
   
   /* bring the clock line high */
   i2c_set_scl();
   i2c_delay();
   
   /* and set SDA back to 1 */
   i2c_set_sda();
   i2c_delay();
}

/* i2c_write_byte
   Write a byte to I2C bus
   => byte = data to write out
   <= return 0 if acknowleged by the slave
*/
unsigned char i2c_write_byte(unsigned char byte)
{
   unsigned char loop, ack, original_byte = byte;
   
   /* send data, msb first */
   for(loop = 0; loop < 8; loop++)
   {
      if(byte & 0x80)
         i2c_set_sda();
      else
         i2c_clear_sda();
      i2c_set_scl();
      i2c_delay();
      i2c_clear_scl();
      byte = byte << 1;
   }
   
   /* read ack bit from slave */
   i2c_set_sda();
   i2c_set_scl();
   i2c_delay();
   ack = i2c_read_sda();
   i2c_clear_scl();
   
   return ack;
}

/* i2c_read_byte
   Read a byte from I2C bus
   => ack = acknowlege bit to send after read
   <= returns data read from the bus
*/
unsigned char i2c_read_byte(unsigned char ack)
{
   unsigned char loop, byte;
   
   i2c_set_sda();
   
   /* read data msb first */
   for(loop = 0; loop < 8; loop++)
   {
      byte = byte << 1;
      i2c_set_scl();
      i2c_delay();
      if(i2c_read_sda()) byte = byte | 1;
      i2c_clear_scl();
   }
   
   /* send ack bit */
   if(ack)
      i2c_set_sda();
   else
      i2c_clear_sda();
   i2c_set_scl();
   i2c_delay();
   i2c_clear_scl();
   i2c_set_sda();
   
   return byte;
}

/* ---------------------------------------------------
    Read data from the EEPROM
   --------------------------------------------------- */

/* eeprom_read
   Read a random byte from an expansion EEPROM
   => bus_addr = bus address of eeprom chip
      mem_addr = address of byte to read from chip
   <= returns the byte read, or 0xff for no chip
*/
unsigned char eeprom_read(unsigned char bus_addr, unsigned int mem_addr)
{
   unsigned char data;
   
   /* send the start signal, the 7-bit bus address and a zero bit to indicate a write */
   i2c_send_start();
   if(i2c_write_byte(bus_addr & 0xfe)) return 0xff;
   
   /* now send the memory address, high address first */
   i2c_write_byte((mem_addr >> 8) & 0xff);
   i2c_write_byte(mem_addr & 0xff);

   /* resend the bus address with read bit set (bit 0) */
   i2c_write_byte(bus_addr | 1);
   
   /* get the data and return */
   data = i2c_read_byte(1);
   i2c_send_stop();
   
   return data;
}
