/* kernel/ports/i386/buses.c
 * i386-specific management of system buses
 * Author : Chris Williams
 * Date   : Sat,13 Feb 2009.19:23:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* bus_register_name
   Register a named bus so that device drivers can attach themselves to
   interrupt lines on specific buses
   => id = bus ID number
       name = name to associate with bus
   <= 0 for success or a failure code
*/
kresult bus_register_name(unsigned int id, char *name)
{
   BUS_DEBUG("[bus:%i] registering bus %i as '%s'\n", CPU_ID, id, name);
   
   /* TODO: unimplemented */
   
   return e_failure;
}

/* bus_add_route
   Register a route between an interrupt on a bus and a particular pin on an
   IOAPIC chip. This allows the kernel to take an IOAPIC interrupt and identify
   the driver responsible for it. A driver thread will know about an interrupt
   on the end of a particular bus but can't assume how it is routed into an IOAPIC.
   This route database marries up driver threads to IOAPIC interrupts
   => flags = flags
      busid = ID number of bus
      busirq = interrupt pin number on the bus
      ioapicid = ID number of the IOAPIC the interrupt is routed to
      ioapicirq = interrupt pin on the IOAPIC
   <= 0 for success or a failure code
*/
kresult bus_add_route(unsigned int flags, unsigned int busid, unsigned int busirq,
                      unsigned int ioapicid, unsigned int ioapicirq)
{
   BUS_DEBUG("[bus:%i] adding route: bus %i irq %i ==> ioapic %i irq %i (flags %i)\n", CPU_ID,
             busid, busirq, ioapicid, ioapicirq, flags);
   
   /* TODO: unimplemented */
   
   return e_failure;
}

/* bus_find_irq
   Look up a cpu irq number from a bus name and bus irq line
   => cpuirq = pointer to a byte in which to store the cpu irq number
      busname = pointer to string containing bus name
      busirq = irq line on the bus
   <= 0 for success or failure code
*/
kresult bus_find_irq(unsigned char *cpuirq, char *busname, unsigned int busirq)
{
   return e_failure;
}

