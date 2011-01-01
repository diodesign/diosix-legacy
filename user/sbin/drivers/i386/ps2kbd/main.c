/* user/sbin/drivers/i386/ps2kbd/main.c
 * Driver to get information out of the ps2 keyboard controller
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
#include "ps2kbd.h"

/* lookup_code
   Convert a keyboard controller code into ASCII
   => code = hardware scan code
      shift = set non-zero to apply automatic shift
      escaped = set non-zero to look up alterative code
   <= 32bit word with ASCII code in the low byte and flags in the upper 24bits,
      or 0 for no translation
*/
unsigned int lookup_code(unsigned char code, unsigned char shift, unsigned escaped)
{
   /* sanity check */
   if(code >= PS2KBD_KEYS) return 0;
   
   if(escaped) return ps2kbd_scancodes[code][2];
   if(shift) return ps2kbd_scancodes[code][1];
   return ps2kbd_scancodes[code][0];
}

/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
int main(void)
{
   diosix_msg_info msg;
   unsigned char shift = 0, control = 0, escaped = 0;
   unsigned char code;
   unsigned int finalcode;

   /* move into driver layer and get access to IO ports */
   diosix_priv_layer_up();
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */
   
   /* allow other processes to find this one */
   diosix_set_role(DIOSIX_ROLE_CONSOLEKEYBOARD);

   /* prepare to sleep until an interrupt comes in */
   msg.role = msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_KERNELONLY;
   
   /* accept the keyboard irq */
   diosix_signals_kernel(SIGX_ENABLE(SIGXIRQ));
   diosix_driver_register_irq(PS2KBD_IRQ);
   
   /* register this device with the vfs */
   diosix_vfs_register("/dev/ps2kbd");     
   
   for(;;)
   {
      /* block until a signal comes in from the hardware */
      if(diosix_msg_receive(&msg) == success && msg.signal.extra == PS2KBD_IRQ)
      {
         /* it's the keyboard controller */
         code = read_port_byte(PS2KBD_DATA);
         if(code == PS2KBD_ESCAPED)
            escaped = 1;
         else
         {
            /* mask out the break bit and translate it into something ASCII */
            finalcode = lookup_code(code & ~PS2KBD_BREAK, shift, escaped);
            escaped = 0; /* end escaped sequence now */
            
            /* decode the scan code */
            switch(finalcode)
            {
               case 0:
                  /* zero indicates an error or something we can't handle */
                  break;
                  
               /* a shift key moved */
               case KEY_SHIFT:
                  if(code & PS2KBD_BREAK) shift = 0;
                  else shift = 1;
                  break;
                  
               /* a control key moved */
               case KEY_CONTROL:
                  if(code & PS2KBD_BREAK) control = 0;
                  else control = 1;
                  break;
                  
               default:
                  /* only accept press-downs on keys */
                  if(!(code & PS2KBD_BREAK))
                  {
                     /* set the control key flag if necessary */
                     if(control) finalcode |= KEY_CONTROL;
                  }
            }
         }
      }
   }
}
