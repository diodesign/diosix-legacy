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

#define KEYBOARD_IRQ (33) /* IRQ1 */

unsigned short read_port(unsigned short port)
{
   unsigned char ret_val;
   
   __asm__ __volatile__("in %1,%0"
                        : "=a"(ret_val)
                        : "d"(port));
   return ret_val;
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

void do_idle(void)
{  
   /* always give the processor something to do */
   while(1) diosix_thread_yield();
}

void do_listen(void)
{
   unsigned int buffer = 0;
   unsigned int px;
   unsigned int state = 0;
   diosix_msg_info msg;
   diosix_phys_request req;
   unsigned int child;
   
   child = diosix_thread_fork();
   if(child == 0) do_idle();
   
   /* move into driver layer and get access to IO ports */
   diosix_priv_layer_up();
   diosix_driver_register();
   
   vbe_set_mode(FB_WIDTH, FB_HEIGHT, FB_DEPTH);
   
   /* map in the video buffer */
   req.paddr = (void *)FB_PHYS_BASE; /* VBE frame buffer */
   req.vaddr = (void *)FB_LOG_BASE;
   req.size  = DIOSIX_PAGE_ROUNDUP(FB_MAX_SIZE);
   req.flags = VMA_WRITEABLE | VMA_NOCACHE | VMA_FIXED;
   diosix_driver_map_phys(&req);
   
   /* listen and reply */
   while(1)
   {      
      /* accept any message from any thread/process */
      msg.tid = DIOSIX_MSG_ANY_THREAD;
      msg.pid = DIOSIX_MSG_ANY_PROCESS;
      msg.flags = DIOSIX_MSG_GENERIC;
      msg.recv = &buffer;
      msg.recv_max_size = sizeof(unsigned int);

      if(diosix_msg_receive(&msg) == success)
      {
         /* change screen colour */
         for(px = 0; px < FB_MAX_SIZE; px += sizeof(unsigned int))
            *((unsigned int *)(FB_LOG_BASE + px)) = buffer & 0xffffff;
         
         state++;
         if(state < 0x0ff)
            buffer += 0x00000001;
         if(state >= 0x0ff && state < 0x1ff)
            buffer += 0x00000100;
         if(state >= 0x1ff && state < 0x2ff)
            buffer += 0x00010000;
         if(state >= 0x2ff && state < 0x3ff)
            buffer -= 0x00000001;
         if(state >= 0x3ff && state < 0x4ff)
            buffer -= 0x00000100;
         if(state >= 0x4ff && state < 0x5ff)
            buffer -= 0x00010000;
         
         if(state > 0x5ff) state = buffer = 0;

         msg.flags = DIOSIX_MSG_GENERIC;
         msg.send = &buffer;
         msg.send_size = sizeof(unsigned int);
         diosix_msg_reply(&msg);
      }
   }
}

void main(void)
{
   diosix_msg_info msg, sig;
   unsigned int child;
   unsigned int message = 0;

   /* create a new thread that'll idle for us */
   child = diosix_fork();
   if(child == 0) do_idle(); /* child can idle */
   
   /* create new process to receive */
   child = diosix_fork();
   if(child == 0) do_listen(); /* child does the listening */
   
   /* move into driver layer and get access to the keyboard IRQ */
   diosix_priv_layer_up();
   diosix_driver_register();
   diosix_signals_kernel(SIG_ACCEPT_KERNEL(SIGXIRQ));
   // diosix_driver_register_irq(KEYBOARD_IRQ);
   diosix_driver_register_irq(32);
   
   /* wait for IRQ signal */
   sig.tid = DIOSIX_MSG_ANY_THREAD;
   sig.pid = DIOSIX_MSG_ANY_PROCESS;
   sig.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_KERNELONLY;
   sig.recv = NULL;
   sig.recv_max_size = 0;
   
   /* wait for keyboard IRQ */
   while(1)
      if(diosix_msg_receive(&sig) == success)
      {         
         /* set up message block to poke the child */
         msg.tid = DIOSIX_MSG_ANY_THREAD;
         msg.pid = child;
         msg.flags = DIOSIX_MSG_GENERIC | DIOSIX_MSG_SENDASUSR;
         msg.send = &message;
         msg.send_size = sizeof(unsigned int);
         msg.recv = &message;
         msg.recv_max_size = sizeof(unsigned int);
         
         /* send message any listening thread, block if successfully
            found a receiver */ 
         if(diosix_msg_send(&msg))
            diosix_thread_yield();
      }
}
