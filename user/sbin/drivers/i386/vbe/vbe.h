/* user/sbin/drivers/i386/vbe/vbe.h
 * Defines for the userspace VBE driver
 * Author : Chris Williams
 * Date   : Tue,7 Dec 2010.06:55:00

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

#ifndef _VBE_H
#define _VBE_H 1

/* hardware constants */
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

/* colours */
#define VBE_COLOUR_BLACK   (0x000000)
#define VBE_COLOUR_BLUE    (0x0000ff)
#define VBE_COLOUR_GREEN   (0x00ff00)
#define VBE_COLOUR_RED     (0xff0000)
#define VBE_COLOUR_WHITE   (0xffffff)
#define VBE_COLOUR_GREY(a) ((((a) & 0xff) << 16) | (((a) & 0xff) << 8) | ((a) & 0xff))

/* default settings */
#define FB_WIDTH      (800)    /* pixels x max */
#define FB_HEIGHT     (600)    /* pixels y max */
#define FB_TXT_WIDTH  (100)    /* characters x max */
#define FB_TXT_HEIGHT (75)     /* characters y max */
#define FB_DEPTH      (VBE_DISPI_BPP_32) /* bits per pixel */
#define FB_MAX_SIZE   (FB_WIDTH * FB_HEIGHT * (FB_DEPTH >> 3))
#define FB_PHYS_BASE  (0xe0000000)
#define FB_LOG_BASE   (0x400000)
#define FB_BACKGROUND (0xff)
#define FB_FOREGROUND (0x22)

#define FB_WORDSPERPIXEL   (FB_DEPTH >> 5) /* 32bits per word */
#define FB_BYTESPERPIXEL   (FB_DEPTH >> 3) /* 8bits per byte */

/* x86-specific stuff */
unsigned char read_port_byte(unsigned short port);
unsigned short read_port(unsigned short port);
void write_port_byte(unsigned short port, unsigned char val);
void write_port(unsigned short port, unsigned short val);

/* access the graphics hardware */
void vbe_set_mode(unsigned short width, unsigned short height, unsigned char bpp);
void vbe_write(unsigned short index, unsigned short val);
unsigned short vbe_read(unsigned short index);

#endif
