/* user/sbin/drivers/i386/vbe/main.c
 * Driver to output graphics to the VBE graphics card
 * Author : Chris Williams
 * Date   : Wed,21 Oct 2009.10:37:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "roles.h"
#include "io.h"
#include "vbe.h"
#include "font.h"

#include <stdio.h>
#include <string.h>

/* the frame buffer */
volatile unsigned int *fb_base = (volatile unsigned int *)FB_LOG_BASE;

/* the text cursor - where the next character will be printed */
unsigned int txt_x = 0, txt_y = 0;

/* default size of the initial receive buffer */
#define RECEIVE_BUFFER_SIZE   (2048)

/* the device name in the filesystem */
#define DEVICE_PATHNAME       "/dev/vbe"

/* record PID of process that has opened this device and the
   associated file handle generated by the VFS (exclusive access only) */
unsigned int connected_pid = 0;
unsigned int connected_filedesc = 0;

/* ----------------------------------------------------------------------
                    manage the video hardware
   ---------------------------------------------------------------------- */

/* initialise_screen
   Get the graphics card into a known usable state */
void initialise_screen(void)
{
   diosix_phys_request req;
   
   /* program the card to enter the default screen mode */
   vbe_set_mode(FB_WIDTH, FB_HEIGHT, FB_DEPTH);
   
   /* map in the video buffer */
   req.paddr = (void *)FB_PHYS_BASE; /* VBE frame buffer */
   req.vaddr = (void *)FB_LOG_BASE;
   req.size  = DIOSIX_PAGE_ROUNDUP(FB_MAX_SIZE);
   req.flags = VMA_WRITEABLE | VMA_NOCACHE | VMA_SHARED;
   
   /* exit if there's a failure */
   if(diosix_driver_map_phys(&req)) diosix_exit(1);
   
   /* start off in the top left corner */
   txt_x = txt_y = 0;
}


/* ----------------------------------------------------------------------
                     basic graphics routines 
   ---------------------------------------------------------------------- */

/* plot_pixel
   Fill a pixel on the screen with a colour
   => x = x coordinate
      y = y coordinate
      colour = pixel colour
*/
void plot_pixel(unsigned int x, unsigned int y, unsigned int colour)
{
   unsigned int px;
   
   /* keep everything sane */
   if(x >= FB_WIDTH) x = FB_WIDTH - 1;
   if(y >= FB_HEIGHT) y = FB_HEIGHT - 1;
   
   /* calculate the px position in memory */
   px = x * FB_WORDSPERPIXEL + y * (FB_WIDTH * FB_WORDSPERPIXEL);
   
   /* write to video memory */
   fb_base[px] = colour;
}

/* plot_rectangle
   Fill a rectangle on the screen with a colour
   => x = top left-hand corner x coordinate
      y = top left-hand corner y coordinate
      w = width in pixels
      h = height in pixels
      colour = colour to plot
*/
void plot_rectangle(unsigned int x, unsigned int y,
                    unsigned int w, unsigned int h, unsigned int colour)
{
   unsigned int w_loop, h_loop;
   
   /* keep everything sane */
   if(x > FB_WIDTH) x = FB_WIDTH;
   if(y > FB_HEIGHT) y = FB_HEIGHT;
   if((x + w) > FB_WIDTH) w = FB_WIDTH - x;
   if((y + h) > FB_HEIGHT) h = FB_HEIGHT - y;
   
   for(h_loop = 0; h_loop < h; h_loop++)
      for(w_loop = 0; w_loop < w; w_loop++)
         plot_pixel(x + w_loop, y + h_loop, colour);
}

/* line_break
   Insert a line break on the screen, scrolling the screen if necessary
*/
void line_break(void)
{
   /* do the carriage return */
   txt_x = 0;
   
   /* and the line feed */
   if((txt_y + 1) >= FB_TXT_WIDTH)
   {
      /* scroll the screen */
      unsigned int px = 0;
      unsigned char *fb_offset = (void *)fb_base;
      
      fb_offset += FONT_HEIGHT * (FONT_WIDTH * FB_BYTESPERPIXEL * FB_TXT_WIDTH);
      
      while(px < (FB_MAX_SIZE - (unsigned int)fb_offset))
      {
         fb_base[px] = fb_offset[px];
         px++;
      }
      
      /* clear the bottom line */
      plot_rectangle(0, FB_HEIGHT - FONT_HEIGHT, FB_WIDTH, FONT_HEIGHT, VBE_COLOUR_GREY(FB_BACKGROUND));
   }
   else
      txt_y++;
}

/* plot_character
   Draw a character on the screen
   => x, y = coordinates to plot the top left-hand corner of the character
      c = character code to plot
      colour = colour to use in the plot
*/
void plot_character(unsigned int x, unsigned int y, unsigned char c, unsigned int colour)
{
   unsigned int yloop, xloop;
   
   /* keep everything sane */
   if(x >= (FB_WIDTH - FONT_WIDTH)) x = FB_WIDTH - FONT_WIDTH - 1;
   if(y >= (FB_HEIGHT - FONT_HEIGHT)) y = FB_HEIGHT - FONT_HEIGHT - 1;
   if(c >= FONT_NR_CHARS) c = 0; 

   for(yloop = 0; yloop < FONT_HEIGHT; yloop++)
   {
      for(xloop = 0; xloop < FONT_WIDTH; xloop++)
         if(font_data[c][yloop] & (1 << xloop))
            plot_pixel(x + FONT_WIDTH - xloop, y + yloop, colour);
   }
}

/* backspace
   Move the text cursor to the left, moving it up to the end of the last
   line if necessary, and erase the last written character */
void backspace(void)
{
   unsigned int x, y;
   
   /* handle moving up a line */
   if(!txt_x)
   {
      if(txt_y)
      {
         txt_y--;
         txt_x = FB_TXT_WIDTH - 1;
      }
      else return; /* nothing to delete! */
   }
   else
      txt_x--;
 
   /* calculate coordinates of last known character */
   x = txt_x * FONT_WIDTH;
   y = txt_y * FONT_HEIGHT;
   
   /* erase it */
   plot_rectangle(x, y, FONT_WIDTH, FONT_HEIGHT, VBE_COLOUR_GREY(FB_BACKGROUND));
}

/* write_character
   Draw a character on the screen, scrolling the screen if necessary. 
   => c = character to write
      colour = text colour to use
 */
void write_character(char c, unsigned int colour)
{
   unsigned int x, y;
   
   x = txt_x * FONT_WIDTH;
   y = txt_y * FONT_HEIGHT;
   
   plot_character(x, y, c, VBE_COLOUR_GREY(colour));
      
   /* line break automatically at the end of a line */
   txt_x++;
   if(txt_x >= FB_TXT_WIDTH) line_break();
}

/* write_string
   Draw a string of text on the screen, inserting a line break at the
   end and scrolling the screen if necessary. 
   => str = NULL terminated string to write
      colour = text colour to use
*/
void write_string(char *str, unsigned int colour)
{
   unsigned int x, y;
   
   x = txt_x * FONT_WIDTH;
   y = txt_y * FONT_HEIGHT;
   
   while(*str)
   {
      plot_character(x, y, *str, VBE_COLOUR_GREY(colour));
      
      str++;
      x += FONT_WIDTH;
      
      /* line break automatically at the end of a line */
      txt_x++;
      if(txt_x >= FB_TXT_WIDTH) line_break();
   }
   
   line_break();
}


/* introduce us to the console user */
void boot_screen(void)
{
   unsigned int i;
   
   /* fade into white */
   for(i = 0; i < FB_BACKGROUND; i+= 8)
      plot_rectangle(0, 0, FB_WIDTH, FB_HEIGHT, VBE_COLOUR_GREY(i));
   
   /* plot some kind of welcome title bar */
   plot_rectangle(0, 0, FB_WIDTH, FONT_HEIGHT + 1, VBE_COLOUR_GREY(FB_FOREGROUND));
   write_string("now running diosix-hyatt", FB_BACKGROUND);
}

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
void reply_to_request(diosix_msg_info *msg, kresult result)
{
   diosix_vfs_reply reply;
   
   /* sanity check */
   if(!msg) return;
   
   /* ping a reply back to unlock the requesting thread */
   reply.result = result;
   msg->send = &reply;
   msg->send_size = sizeof(diosix_vfs_reply);
   
   diosix_msg_reply(msg);
}

void wait_for_request(void)
{
   diosix_msg_info msg;
   unsigned char recv_buffer[RECEIVE_BUFFER_SIZE];
   
   memset(&msg, 0, sizeof(diosix_msg_info));
   
   /* wait for a generic message to come in */
   msg.flags = DIOSIX_MSG_GENERIC;
   msg.recv = recv_buffer;
   msg.recv_max_size = RECEIVE_BUFFER_SIZE;
   
   if(diosix_msg_receive(&msg) == success)
   {
      diosix_vfs_request_head *req_head = (diosix_vfs_request_head *)recv_buffer;
      
      if(msg.recv_size < sizeof(diosix_vfs_request_head))
      {
         /* malformed request, it's too small to even hold a request type */
         reply_to_request(&msg, e_too_small);
         return;
      }
      
      /* sanity check the message header */
      if(req_head->magic != VFS_MSG_MAGIC)
      {
         /* malformed request, bad magic */
         reply_to_request(&msg, e_bad_magic);
         return;
      }
      
      /* decode the request type */
      switch(req_head->type)
      {
         /* discard requests that aren't appropriate */
         case link_req:
         case readlink_req:
         case unlink_req:
         case symlink_req:
         case lseek_req:
         case register_req:
         case deregister_req:
            
         /* discard requests that aren't implemented (yet) */
         case chown_req:
         case fstat_req:
         case stat_req:
            reply_to_request(&msg, e_no_handler);
            break;

         /* open device exclusively for root only */
         case open_req:
            /* discard if something other than the VFS is
               handling this request */
            if(msg.role != DIOSIX_ROLE_VFS)
            {
               diosix_vfs_request_open *req_info = VFS_MSG_EXTRACT(req_head, 0);
               connected_pid = msg.pid;
               reply_to_request(&msg, success);
               return;
            }
            
            /* fall through to sending back an error */
            reply_to_request(&msg, e_no_rights);
            break;

         case close_req:
         case read_req:
         case write_req:
            
         /* if the type is unknown then fail it */
         default:
            reply_to_request(&msg, e_bad_params);
      }
   }
}


int main(void)
{
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */
   
   /* set up the graphics card */
   initialise_screen();
   
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_CONSOLEVIDEO);

   /* welcome the user */
   boot_screen();

   /* register this device with the vfs */
   diosix_vfs_register(DEVICE_PATHNAME);
   
   /* wait for work to come in */
   while(1) wait_for_request();
}
