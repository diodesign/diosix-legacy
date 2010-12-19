/* user/sbin/drivers/i386/vbe/vbe.c
 * Control the VBE graphics card
 * Author : Chris Williams
 * Date   : Tue,7 Dec 2010.06:55:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include "vbe.h"

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
