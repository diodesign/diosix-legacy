/* user/lib/libdiosix/include/io.h
 * structures and defines for input and output via the vfs
 * Author : Chris Williams
 * Date   : Thu,09 Dec 2010.05:09:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _IO_H
#define   _IO_H

/* agree on a way to represent non-ASCII key codes.
   8-bit ascii in the low byte with flags in the upper 24 bits */
#define KEY_TAB      (9)   /* tab control code */
#define KEY_ENTER    (13)  /* carriage return control code */
#define KEY_ESC      (27)
#define KEY_DELETE   (127)
#define KEY_F1       (1 | KEY_FKEY)
#define KEY_F2       (2 | KEY_FKEY)
#define KEY_F3       (3 | KEY_FKEY)
#define KEY_F4       (4 | KEY_FKEY)
#define KEY_F5       (5 | KEY_FKEY)
#define KEY_F6       (6 | KEY_FKEY)
#define KEY_F7       (7 | KEY_FKEY)
#define KEY_F8       (8 | KEY_FKEY)
#define KEY_F9       (9 | KEY_FKEY)
#define KEY_F10      (10 | KEY_FKEY)

#define KEY_SHIFT    (1 << 8)
#define KEY_BSPACE   (1 << 9)
#define KEY_CONTROL  (1 << 10)
#define KEY_LCTRL    (KEY_CONTROL)
#define KEY_RCTRL    (KEY_CONTROL)
#define KEY_LSHIFT   (KEY_SHIFT)
#define KEY_RSHIFT   (KEY_SHIFT)
#define KEY_ALT      (1 << 11)
#define KEY_LALT     (KEY_ALT)
#define KEY_RALT     (KEY_ALT)
#define KEY_CLOCK    (1 << 12)
#define KEY_FKEY     (1 << 13)
#define KEY_HOME     (1 << 14)
#define KEY_UP       (1 << 15)
#define KEY_DOWN     (1 << 16)
#define KEY_LEFT     (1 << 17)
#define KEY_RIGHT    (1 << 18)
#define KEY_PGUP     (1 << 19)
#define KEY_PGDOWN   (1 << 20)
#define KEY_INSERT   (1 << 21)
#define KEY_END      (1 << 22)

/* reserved at this time */
#define KEY_PSCREEN  (0)
#define KEY_SLOCK    (0)
#define KEY_NLOCK    (0)
#define KEY_BREAK    (0)


/* define the interface between clients and servers */
typedef struct
{
   unsigned int flags, status;
   char *node;
} diosix_server;

typedef struct
{
   unsigned int flags, mode, status;
   char *path;
} diosix_server_request;

#endif
