/* user/sbin/drivers/i386/vbe/vbe.h
 * Defines for the userspace VBE driver
 * Author : Chris Williams
 * Date   : Tue,7 Dec 2010.06:55:00

Copyright, (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files, (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <diosix.h>
#include <functions.h>
#include <signal.h>


#ifndef _PS2KBD_H
#define _PS2KBD_H 1

/* hardware constants */
#define PS2KBD_IRQ     (31)      /* ISA IRQ line of the kbd controller */
#define PS2KBD_DATA    (0x60)    /* data port of the kbd controller */
#define PS2KBD_BREAK   (1 << 7)  /* bit set to indicated key release */
#define PS2KBD_ESCAPED (0xe0)    /* byte to indicate a special sequence to follow */
#define PS2KBD_KEYS    (0x55)    /* number of keys in the structure below */

/* ps2 set 2 scan codes for US keyboard.
   format per scan code is:
      ascii code + flags,
      ascii code + flags with shift,
      escaped ascii code + flags */

unsigned int ps2kbd_scancodes[PS2KBD_KEYS][3] = 
{  /* normally an error code */
   /* 0 */ { 0, 0, 0},

   /* main keyboard keys */
   /* 0x01 */ { KEY_ESC, KEY_ESC | KEY_SHIFT, 0 },
   /* 0x02 */ { '1', '!', 0 },
   /* 0x03 */ { '2', '@', 0 },
   /* 0x04 */ { '3', '#', 0 },
   /* 0x05 */ { '4', '$', 0 },
   /* 0x06 */ { '5', '%', 0 },
   /* 0x07 */ { '6', '^', 0 },
   /* 0x08 */ { '7', '&', 0 },
   /* 0x09 */ { '8', '*', 0 },
   /* 0x0a */ { '9', '(', 0 },
   /* 0x0b */ { '0', ')', 0 },
   /* 0x0c */ { '-', '_', 0 },
   /* 0x0d */ { '=', '+', 0 },
   /* 0x0e */ { KEY_BSPACE, KEY_BSPACE | KEY_SHIFT, 0 },
   /* 0x0f */ { KEY_TAB, KEY_TAB | KEY_SHIFT, 0 },
   /* 0x10 */ { 'q', 'Q', 0 },
   /* 0x11 */ { 'w', 'W', 0 },
   /* 0x12 */ { 'e', 'E', 0 },
   /* 0x13 */ { 'r', 'R', 0 },
   /* 0x14 */ { 't', 'T', 0 },
   /* 0x15 */ { 'y', 'Y', 0 },
   /* 0x16 */ { 'u', 'U', 0 },
   /* 0x17 */ { 'i', 'I', 0 },
   /* 0x18 */ { 'o', 'O', 0 },
   /* 0x19 */ { 'p', 'P', 0 },
   /* 0x1a */ { '[', '{', 0 },
   /* 0x1b */ { ']', '}', 0 },
   /* 0x1c */ { KEY_ENTER, KEY_ENTER | KEY_SHIFT, KEY_ENTER },
   /* 0x1d */ { KEY_LCTRL, KEY_LCTRL | KEY_SHIFT, KEY_RCTRL },
   /* 0x1e */ { 'a', 'A', 0 },
   /* 0x1f */ { 's', 'S', 0 },
   /* 0x20 */ { 'd', 'D', 0 },
   /* 0x21 */ { 'f', 'F', 0 },
   /* 0x22 */ { 'g', 'G', 0 },
   /* 0x23 */ { 'h', 'H', 0 },
   /* 0x24 */ { 'j', 'J', 0 },
   /* 0x25 */ { 'k', 'K', 0 },
   /* 0x26 */ { 'l', 'L', 0 },
   /* 0x27 */ { ';', ':', 0 },
   /* 0x28 */ { '\'', '"', 0 },
   /* 0x29 */ { '`', '~', 0 },
   /* 0x2a */ { KEY_LSHIFT, KEY_LSHIFT, KEY_LSHIFT },
   /* 0x2b */ { '\\', '|', 0 },
   /* 0x2c */ { 'z', 'Z', 0 },
   /* 0x2d */ { 'x', 'X', 0 },
   /* 0x2e */ { 'c', 'C', 0 },
   /* 0x2f */ { 'v', 'V', 0 },
   /* 0x30 */ { 'b', 'B', 0 },
   /* 0x31 */ { 'n', 'N', 0 },
   /* 0x32 */ { 'm', 'M', 0 },
   /* 0x33 */ { ',', '<', 0 },
   /* 0x34 */ { '.', '>', 0 },
   /* 0x35 */ { '/', '?', '/' },
   /* 0x36 */ { KEY_RSHIFT, KEY_RSHIFT, KEY_RSHIFT },
   /* 0x37 */ { '*', '*' | KEY_SHIFT, KEY_PSCREEN | KEY_CONTROL },
   /* 0x38 */ { KEY_LALT, KEY_LALT | KEY_SHIFT, KEY_RALT },
   /* 0x39 */ { ' ', ' ' | KEY_SHIFT, 0 },
   /* 0x3a */ { KEY_CLOCK, KEY_CLOCK | KEY_SHIFT, 0 },
   /* 0x3b */ { KEY_F1, KEY_F1 | KEY_SHIFT, 0 },
   /* 0x3c */ { KEY_F2, KEY_F2 | KEY_SHIFT, 0 },
   /* 0x3d */ { KEY_F3, KEY_F3 | KEY_SHIFT, 0 },
   /* 0x3e */ { KEY_F4, KEY_F4 | KEY_SHIFT, 0 },
   /* 0x3f */ { KEY_F5, KEY_F5 | KEY_SHIFT, 0 },
   /* 0x40 */ { KEY_F6, KEY_F6 | KEY_SHIFT, 0 },
   /* 0x41 */ { KEY_F7, KEY_F7 | KEY_SHIFT, 0 },
   /* 0x42 */ { KEY_F8, KEY_F8 | KEY_SHIFT, 0 },
   /* 0x43 */ { KEY_F9, KEY_F9 | KEY_SHIFT, 0 },
   /* 0x44 */ { KEY_F10, KEY_F10 | KEY_SHIFT, 0 },
   /* 0x45 */ { KEY_NLOCK, KEY_NLOCK, 0 },
   /* 0x46 */ { KEY_SLOCK, KEY_SLOCK, KEY_BREAK | KEY_CONTROL },

   /* keypad */
   /* 0x47 */ { '7', '7', KEY_HOME },
   /* 0x48 */ { '8', '8', KEY_UP },
   /* 0x49 */ { '9', '9', KEY_PGUP },
   /* 0x4a */ { '-', '-', 0 },
   /* 0x4b */ { '4', '4', KEY_LEFT },
   /* 0x4c */ { '5', '5', 0 },
   /* 0x4d */ { '6', '6', KEY_RIGHT },
   /* 0x4e */ { '+', '+', 0 },
   /* 0x4f */ { '1', '1', KEY_END },
   /* 0x50 */ { '2', '2', KEY_DOWN },
   /* 0x51 */ { '3', '3', KEY_PGDOWN },
   /* 0x52 */ { '0', '0', KEY_INSERT },
   /* 0x53 */ { '.', '.', KEY_DELETE },

   /* Alt-SysRq */
   /* 0x54 */ { 0, 0, 0 }
};

/* x86-specific stuff */
unsigned char read_port_byte(unsigned short port);
unsigned short read_port(unsigned short port);
void write_port_byte(unsigned short port, unsigned char val);
void write_port(unsigned short port, unsigned short val);

#endif
