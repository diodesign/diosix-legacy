/* user/sbin/init/init.c
 * First process to run; spawn system managers and boot system
 * Author : Chris Williams
 * Date   : Wed,21 Oct 2009.10:37:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <diosix.h>
#include <functions.h>
#include <signal.h>

/* values taken from http://wiki.osdev.org/Bochs_Graphics_Adaptor */

#define VBE_DISPI_IOPORT_INDEX (0x1ce)
#define VBE_DISPI_IOPORT_DATA  (0x1cf)

#define VBE_DISPI_INDEX_ID     (0)
#define VBE_DISPI_INDEX_XRES   (1)
#define VBE_DISPI_INDEX_YRES   (2)
#define VBE_DISPI_INDEX_BPP    (3)
#define VBE_DISPI_INDEX_ENABLE (4)
#define VBE_DISPI_INDEX_BANK   (5)

#define VBE_DISPI_DISABLED     (0x00)
#define VBE_DISPI_ENABLED      (0x01)
#define VBE_DISPI_LFB_ENABLED  (0x40)

#define VBE_DISPI_BPP_4    (0x04)
#define VBE_DISPI_BPP_8    (0x08)
#define VBE_DISPI_BPP_15   (0x0F)
#define VBE_DISPI_BPP_16   (0x10)
#define VBE_DISPI_BPP_24   (0x18)
#define VBE_DISPI_BPP_32   (0x20)

#define FB_WIDTH     (800)    /* pixels x max */
#define FB_HEIGHT    (600)    /* pixels y max */
#define FB_DEPTH     (VBE_DISPI_BPP_32) /* bits per pixel */
#define FB_MAX_SIZE  (FB_WIDTH * FB_HEIGHT * (FB_DEPTH >> 3))
#define FB_PHYS_BASE (0xe0000000)
#define FB_LOG_BASE  (0x400000)

#define KEYBOARD_IRQ  (33)   /* IRQ1 */
#define KEYBOARD_DATA (0x60) /* KBC data port */
#define KEYBOARD_CTRL (0x61) /* KBC control port */

unsigned char read_port_byte(unsigned short port)
{
   unsigned char ret_val;
   
   __asm__ __volatile__("inb %1,%0"
                        : "=a"(ret_val)
                        : "d"(port));
   return ret_val;
}

unsigned short read_port(unsigned short port)
{
   unsigned char ret_val;
   
   __asm__ __volatile__("in %1,%0"
                        : "=a"(ret_val)
                        : "d"(port));
   return ret_val;
}

void write_port_byte(unsigned short port, unsigned char val)
{
   __asm__ __volatile__("outb %0,%1"
                        :
                        : "a"(val), "d"(port));
}


void write_port(unsigned short port, unsigned short val)
{
   __asm__ __volatile__("out %0,%1"
                        :
                        : "a"(val), "d"(port));
}

unsigned short vbe_read(unsigned short index)
{
   write_port(VBE_DISPI_IOPORT_INDEX, index);
   return read_port(VBE_DISPI_IOPORT_DATA);
}

void vbe_write(unsigned short index, unsigned short val)
{
   write_port(VBE_DISPI_IOPORT_INDEX, index);
   write_port(VBE_DISPI_IOPORT_DATA, val);
}

void vbe_set_mode(unsigned short width, unsigned short height, unsigned char bpp)
{
   vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
   
   vbe_write(VBE_DISPI_INDEX_XRES, width);
   vbe_write(VBE_DISPI_INDEX_YRES, height);
   vbe_write(VBE_DISPI_INDEX_BPP, bpp);
   
   vbe_write(VBE_DISPI_INDEX_ENABLE,
             VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}

void do_listen(void)
{
   unsigned int buffer = 0;
   unsigned int px;
   diosix_msg_info msg;
   diosix_phys_request req;
   
   /* move into driver layer and get access to IO ports */
   diosix_priv_layer_up();
   diosix_driver_register();
   
   vbe_set_mode(FB_WIDTH, FB_HEIGHT, FB_DEPTH);
   
   /* map in the video buffer */
   req.paddr = (void *)FB_PHYS_BASE; /* VBE frame buffer */
   req.vaddr = (void *)FB_LOG_BASE;
   req.size  = DIOSIX_PAGE_ROUNDUP(FB_MAX_SIZE);
   req.flags = VMA_WRITEABLE | VMA_NOCACHE | VMA_SHARED;
   diosix_driver_map_phys(&req);
   
   /* accept any message from any thread/process */
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.flags = DIOSIX_MSG_GENERIC;
   msg.recv = &buffer;
   msg.recv_max_size = sizeof(unsigned int);

   if(diosix_msg_receive(&msg) != success) while(1); /* halt if failed */
   
   if(buffer == 0) buffer = 0xffffffff; /* prove we're swapping data */
   
   msg.flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_SHAREVMA | DIOSIX_MSG_REPLY;
   msg.mem_req.base = FB_LOG_BASE;
   msg.mem_req.size = DIOSIX_PAGE_ROUNDUP(FB_MAX_SIZE);
   msg.send = &buffer;
   msg.send_size = sizeof(unsigned int);
   
   /* reply to the share request */
   if(diosix_msg_reply(&msg) != success) while(1); /* halt if failed */
      
   while(1)
   {
      diosix_thread_sleep(100);
      
      for(px = 0; px < FB_MAX_SIZE >> 1; px += sizeof(unsigned int))
         *((volatile unsigned int *)(FB_LOG_BASE + px)) = (buffer & 0xff) << 16;
         
      buffer += 4;
   }
}

void main(void)
{
   diosix_msg_info msg;
   unsigned int child;
   unsigned int buffer = 0, message = 0;
   unsigned int px;

   /* create idle thread */
   child = diosix_thread_fork();
   if(!child)
   {
      diosix_priv_layer_up();
      diosix_priv_layer_up();
      while(1);
   }
   
   /* create new process to receive */
   child = diosix_fork();
   if(!child) do_listen(); /* child does the listening */

   /* ask to share some memory with the child */
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.pid = child;
   msg.flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_SENDASUSR | DIOSIX_MSG_SHAREVMA | DIOSIX_MSG_QUEUEME;
   msg.send = &message;
   msg.send_size = sizeof(unsigned int);
   msg.recv = &message;
   msg.recv_max_size = sizeof(unsigned int);
   msg.mem_req.base = 0x200000;
   msg.mem_req.size = DIOSIX_PAGE_ROUNDUP(FB_MAX_SIZE);
   
   /* send message any listening thread */
   if(diosix_msg_send(&msg) != success) while(1);

   /* if memory share worked then write to video memory */
   if(msg.flags & DIOSIX_MSG_SHAREVMA)
   {      
      while(1)
      {
         diosix_thread_sleep(50);
         
         for(px = FB_MAX_SIZE >> 1; px < FB_MAX_SIZE; px += sizeof(unsigned int))
            *((volatile unsigned int *)(0x200000 + px)) = buffer & 0xff;
      
         buffer += 4;
      }
   }
   while(1); /* halt */
}
