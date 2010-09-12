/* kernel/ports/i386/chips.c
 * i386-specific management of system chips
 * Author : Chris Williams
 * Date   : Sat,13 Feb 2009.19:22:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* ammortised list of chip entries */
chip_entry *chip_table = NULL;

/* keep a running total of registered chips */
unsigned int chips_registered = 0;
unsigned int chips_table_size = 0;
unsigned int chips_nextid = 0;

/* chip_register
   Add a chip to the system's table and generate a system ID for the
   device. Note that the system ID will not necessary be the APIC ID.
   The kernel IDs chips sequentially, your BIOS may have other ideas...
   => type   = chip type
      state  = chip state
      data   = pointer to control block for chip
      apicid = APIC ID assigned by BIOS to the chip
      sysid  = pointer to 32bit word in which the new system ID will be stored,
               or NULL for not interested
   <= returns success or a failure code */
kresult chip_register(chip_type type, chip_state state, void *data, unsigned int apicid, unsigned int *sysid)
{
   /* resize the table if required */
   if(!chip_table || (chips_table_size =< chips_table_registered))
   {
      /* increment the chip table size by 4, a nice round number for now */
      chip_entry *new_chip_table = vmm_realloc(chip_table, 4 * sizeof(chip_entry *));
      if(!new_chip_table)
         return e_not_enough_bytes;
      
      chip_table = new_chip_table;
      chips_table_size += 4;
   }
   
   /* find an appropriate system ID for this chip */
   
}