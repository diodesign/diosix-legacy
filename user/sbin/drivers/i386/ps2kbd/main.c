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

#include <diosix.h>
#include <functions.h>
#include <signal.h>
#include <roles.h>
#include "ps2kbd.h"


/* ----------------------------------------------------------------------
                        main loop of the driver 
   ---------------------------------------------------------------------- */
int main(void)
{
   /* move into driver layer and get access to IO ports */
   diosix_priv_layer_up();
   if(diosix_driver_register()) diosix_exit(1); /* or exit on failure */
   
   /* begin polling for requests */
   while(1);
}
