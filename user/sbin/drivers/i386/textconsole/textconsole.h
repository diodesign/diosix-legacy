/* user/sbin/drivers/i386/textconsole/textconsole.h
 * Defines for the userspace ps2 keyboard driver
 * Author : Chris Williams
 * Date   : Tue,7 Dec 2010.06:55:00

Copyright, (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files, (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/


#ifndef _TEXTCONSOLE_H
#define _TEXTCONSOLE_H 1

#define SCREEN_LOG_BASE  (0x400000)
#define SCREEN_PHYS_BASE (0xb8000)
#define SCREEN_WIDTH     (80)
#define SCREEN_HEIGHT    (25)
#define SCREEN_MAX_SIZE  (SCREEN_WIDTH * SCREEN_HEIGHT)

#define COLOUR_UNCHANGED (-1)

#endif
