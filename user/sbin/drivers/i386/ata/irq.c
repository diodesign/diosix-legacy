/* user/sbin/drivers/i386/ata/irq.c
 * Driver for ATA-connected storage devices 
 * Author : Chris Williams
 * Date   : Tues,27 Sep 2011.23:42:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <stdio.h>
#include <string.h>

#include "diosix.h"
#include "functions.h"
#include "async.h"
#include "lowlevel.h"

#include "ata.h"

volatile unsigned char irqs_pending = 0;
volatile unsigned char irqs_lock = 0;


/* clear_irq
   Clear pending IRQs
   => flags = bitmask to apply to pending IRQs byte; a set bit will clear the corresponding IRQ
*/
void clear_irq(unsigned char flags)
{
   lock_spin(&irqs_lock);
   
   irqs_pending &= ~flags;
   
   unlock_spin(&irqs_lock);
}

/* sleep_on_irq
   Block the thread until the selected IRQ fires, clearing the IRQ before returning
*/
void sleep_on_irq(unsigned char flags)
{
   while(1)
   {
      lock_spin(&irqs_lock);
      
      if(irqs_pending & flags)
      {
         /* clear the pending flag and return */
         irqs_pending &= ~flags;
         unlock_spin(&irqs_lock);
         return;
      }
      
      /* sleep for a bit */
      unlock_spin(&irqs_lock);
      diosix_thread_yield();
   }
}

void wait_for_irq(void)
{
   diosix_msg_info msg;

   /* prepare to sleep until an interrupt comes in */
   msg.role = msg.pid = DIOSIX_MSG_ANY_PROCESS;
   msg.tid = DIOSIX_MSG_ANY_THREAD;
   msg.flags = DIOSIX_MSG_SIGNAL | DIOSIX_MSG_KERNELONLY;
   
   for(;;)
   {
      /* block until a signal comes in from the hardware */
      if(diosix_msg_receive(&msg) == success)
      {
         switch(msg.signal.extra)
         {
            case ATA_IRQ_PRIMARY:
               lock_spin(&irqs_lock);
               
               irqs_pending |= ATA_IRQ_PRIMARY_PENDING;
               
               unlock_spin(&irqs_lock);
               break;
          
            case ATA_IRQ_SECONDARY:
               lock_spin(&irqs_lock);

               irqs_pending |= ATA_IRQ_SECONDARY_PENDING;
               
               unlock_spin(&irqs_lock);               
               break;
         }
      }
   }
}
