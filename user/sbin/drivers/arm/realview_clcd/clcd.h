/* user/sbin/drivers/arm/realview_clcd/clcd.h
 * Control the Realview CLCD graphics display
 * Author : Chris Williams
 * Date   : Fri,22 April 2011.17:51:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _CLCD_H
#define _CLCD_H 1

/* hardware constants */

/* default settings */
#define FB_WIDTH      (800)    /* pixels x max */
#define FB_HEIGHT     (600)    /* pixels y max */
#define FB_TXT_WIDTH  (100)    /* characters x max */
#define FB_TXT_HEIGHT (75)     /* characters y max */
#define FB_DEPTH      (32)     /* bits per pixel */
#define FB_MAX_SIZE   (FB_WIDTH * FB_HEIGHT * (FB_DEPTH >> 3))
#define FB_PHYS_BASE  (0)
#define FB_LOG_BASE   (0)
#define FB_BACKGROUND (0xff)
#define FB_FOREGROUND (0x22)

#define FB_WORDSPERPIXEL   (FB_DEPTH >> 5) /* 32bits per word */
#define FB_BYTESPERPIXEL   (FB_DEPTH >> 3) /* 8bits per byte */

#define COLOUR_GREY(a) ((((a) & 0xff) << 16) | (((a) & 0xff) << 8) | ((a) & 0xff))

/* access the graphics hardware */
void clcd_set_mode(unsigned short width, unsigned short height, unsigned char bpp);
void clcd_write(unsigned short index, unsigned short val);
unsigned short clcd_read(unsigned short index);

#endif
