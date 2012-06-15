/* user/sbin/drivers/i386/textconsole/main.c
 * Driver to control the VGA text-mode console
 * Author : Chris Williams
 * Date   : Sat,09 Jun 2012.20:29:00

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

#include "textconsole.h"
#include "../ps2kbd/ps2kbd.h"

#include <stdio.h>
#include <string.h>

/* the text-mode character buffer for the screen */
volatile unsigned short *screen_base = (volatile unsigned short *)SCREEN_LOG_BASE;

/* the text cursor - where the next character will be printed */
unsigned int txt_x = 0, txt_y = 0;

/* ----------------------------------------------------------------------
                    manage the video hardware
   ---------------------------------------------------------------------- */

/* initialise_screen
   Get the graphics card into a known usable state */
void initialise_screen(void)
{
   diosix_phys_request req;
   
   /* map in the video buffer */
   req.paddr = (void *)SCREEN_PHYS_BASE; /* screen character buffer */
   req.vaddr = (void *)SCREEN_LOG_BASE;
   req.size  = DIOSIX_PAGE_ROUNDUP(SCREEN_MAX_SIZE);
   req.flags = VMA_WRITEABLE | VMA_NOCACHE | VMA_SHARED;
   
   /* exit if there's a failure */
   if(diosix_driver_map_phys(&req)) diosix_exit(1);
   
   /* start off in the top left corner */
   txt_x = txt_y = 0;
}


/* ----------------------------------------------------------------------
                     basic graphics routines 
   ---------------------------------------------------------------------- */

/* plot_character
   Plot a character on the screen using the given colours at the given position
   => x, y = the x, y coordinates of the character to plot
      character = the ASCII code of the character to plot
      fg_colour, bg_colour = character's foreground and background colours.
                             -1 (COLOUR_UNCHANGED) indicates use the
                             existing fg/bg colour at that character
                             position, effectively implementing transparency
*/
void plot_character(unsigned char x, unsigned char y, unsigned char character,
                    char fg_colour, char bg_colour)
{
   unsigned short pos;
   
   /* keep everything sane */
   if(x >= SCREEN_WIDTH) x = SCREEN_WIDTH - 1;
   if(y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;
   
   /* calculate the character's position in memory */
   pos = x + (y * SCREEN_WIDTH);
   
   /* check to see if the colours need fixing up if -1 was passed for either */
   if(fg_colour == COLOUR_UNCHANGED || bg_colour == COLOUR_UNCHANGED)
   {
      unsigned short data = screen_base[pos];
      
      if(fg_colour == COLOUR_UNCHANGED) fg_colour = data & 0xf;
      if(bg_colour == COLOUR_UNCHANGED) bg_colour = (data >> 4) & 0xf;
   }
   else
   {
      fg_colour = fg_colour & 0xf;
      bg_colour = bg_colour & 0xf;
   }
   
   /* write to video memory, format is:
      bit 15            bit 7                  bit 0
      [ attribute byte ][ ascii character byte ]
        attribute bits 0-3 = foreground colour
                       4-7 = background colour */
   screen_base[pos] = character | (fg_colour << 8) | (bg_colour << 12);
}

/* plot_rectangle
   Fill a rectangle of characters on the screen with a colour
   => x = top left-hand corner x coordinate
      y = top left-hand corner y coordinate
      w = width in characters
      h = height in characters
      colour = colour to plot, or -1 to use existing attributes
*/
void plot_rectangle(unsigned char x, unsigned char y,
                    unsigned char w, unsigned char h, char colour)
{
   unsigned int w_loop, h_loop;
   
   /* keep everything sane */
   if(x > SCREEN_WIDTH) x = SCREEN_WIDTH;
   if(y > SCREEN_HEIGHT) y = SCREEN_HEIGHT;
   if((x + w) > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
   if((y + h) > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
   
   for(h_loop = 0; h_loop < h; h_loop++)
      for(w_loop = 0; w_loop < w; w_loop++)
         plot_character(h_loop, w_loop, ' ', COLOUR_UNCHANGED, colour);
}

/* line_break
   Insert a line break on the screen, scrolling the screen if necessary
*/
void line_break(void)
{
   unsigned char x, y;
   
   /* do the carriage return */
   txt_x = 0;
   
   /* and the line feed */
   if((txt_y + 1) >= SCREEN_WIDTH)
   {
      /* scroll the screen by one line if we've run out of space */
      for(y = 1; y < SCREEN_HEIGHT; y++)
         for(x = 0; x < SCREEN_WIDTH; x++)
            screen_base[x + ((y - 1) * SCREEN_WIDTH)] = screen_base[x + (y * SCREEN_WIDTH)];
      
      /* clear the bottom line */
      for(x = 0; x < SCREEN_WIDTH; x++)
         plot_character(x, SCREEN_WIDTH - 1, ' ', COLOUR_UNCHANGED, COLOUR_UNCHANGED);
   }
   else
      txt_y++;
}

/* backspace
   Move the text cursor to the left, moving it up to the end of the last
   line if necessary, and erase the last written character */
void backspace(void)
{   
   /* handle moving up a line */
   if(!txt_x)
   {
      if(txt_y)
      {
         txt_y--;
         txt_x = SCREEN_WIDTH - 1;
      }
      else return; /* nothing to delete! */
   }
   else
      txt_x--;
   
   /* erase it */
   plot_character(txt_x, txt_y, ' ', COLOUR_UNCHANGED, COLOUR_UNCHANGED);
}

/* write_string
   Draw a string of text on the screen, inserting a line break at the
   end and scrolling the screen if necessary. 
   => str = NULL terminated string to write
      fg_colour, bg_colour = text colours to use (-1 to use the existing
      colours in those character positions)
*/
void write_string(char *str, char fg_colour, char bg_colour)
{
   while(*str)
   {
      plot_character(txt_x, txt_y, *str, fg_colour, bg_colour);
      
      str++;
      
      /* line break automatically at the end of a line */
      txt_x++;
      if(txt_x >= SCREEN_WIDTH) line_break();
   }
   
   line_break();
}

/* clear_screen
   Wipe the screen clear using the given background colour */
void clear_screen(char bg_colour)
{
   plot_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bg_colour);
}

/* introduce us to the console user */
void boot_screen(void)
{
   clear_screen(COLOUR_DEFAULTBG);

   write_string("Hello, world! Now running diosix-hyatt", COLOUR_DEFAULTFG, COLOUR_DEFAULTBG);

   while(1)
   {
      write_string("tick", COLOUR_DEFAULTFG, COLOUR_DEFAULTBG);
      diosix_thread_sleep(100);
   }
}

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */

int main(void)
{
   /* move into driver layer (1) and get access to IO ports */
   diosix_priv_layer_up(1);
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */
   
   /* set up the graphics card */
   initialise_screen();

   /* welcome the user */
   boot_screen();
}
