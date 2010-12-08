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

#include <diosix.h>
#include <functions.h>
#include <signal.h>
#include <roles.h>
#include "vbe.h"

/* the frame buffer */
unsigned int *fb_base = (unsigned int *)FB_LOG_BASE;

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
   /* calculate the px position in memory */
   unsigned int px = x * FB_WORDSPERPIXEL + y * (FB_WIDTH * FB_WORDSPERPIXEL);
   
   /* write to video memory */
   fb_base[px] = colour;
}

/* plot_rectangle
   Fill a rectangle on the screen with a colour
   => x = bottom left-hand corner x coordinate
      y = bottom left-hand corner y coordinate
      w = width in pixels
      h = height in pixels
      colour = colour to plot
*/
void plot_rectangle(unsigned int x, unsigned int y,
                    unsigned int w, unsigned int h, unsigned int colour)
{
   unsigned int w_loop, h_loop;
   
   /* calculate the position of the bottom left-hand corner */
   unsigned int px = x * FB_WORDSPERPIXEL + y * (FB_WIDTH * FB_WORDSPERPIXEL);
   
   for(h_loop = 0; h_loop < h; h_loop++)
   {
      for(w_loop = 0; w_loop < w; w_loop++)
         fb_base[px + w_loop] = colour; /* write to video memory */
      
      /* advance a line */
      px += (FB_WIDTH * FB_WORDSPERPIXEL);
   }
}


/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
int main(void)
{   
   /* move into driver layer and get access to IO ports */
   diosix_priv_layer_up();
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */
   
   /* set up the graphics card */
   initialise_screen();
   
   /* name this process so others can find it */
   diosix_set_role(DIOSIX_ROLE_CONSOLEVIDEO);
   
   /* clear the screen */
   plot_rectangle(0, 0, FB_WIDTH, FB_HEIGHT, VBE_COLOUR_BLUE);
   
   /* begin polling for requests */
   while(1);
}
