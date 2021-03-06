/* kernel/ports/arm/versatilepb/boot.c
 * ARM-specific low-level start-up routines for the Versatile PB dev board
 * Author : Chris Williams
 * Date   : Fri,11 Mar 2011.16:54:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* configuration registers */
#define REG_SYS_ID       KERNEL_PHYS2LOG(0x10000000)
#define REG_SYS_LOCK     KERNEL_PHYS2LOG(0x10000020)

/* set this bit in REG_SYS_LOCK to lock all control registers */
#define SYS_LOCK_BIT     (1 << 16)

/* preboot
   Set up a multiboot environment for the portable kernel
   => magic = must be zero
      machine_id = id of machine we're being booted on
      atag_list = pointer to physical address of ATAG list from bootloader
   <= phys address pointer to multiboot structure for the core kernel, or NULL for failure
*/
multiboot_info_t *preboot(unsigned int magic, arm_machine_id machine_id, void *atag_list)
{
   volatile unsigned int lockval;
   multiboot_info_t *mb;
   
   /* sanity checks - bail out if magic is non-zero or if
      trying to boot on a non-supported board */
   if(magic || machine_id != versatilepb) return NULL;
   
   /* process the atag list for the core kernel, which expects multiboot */
   mb = atag_process(KERNEL_PHYS2LOG(atag_list));
   BOOT_DEBUG("[arm] machine id: %i multiboot: 0x%p\n", machine_id, mb);
   
   /* lock the control registers by setting bit 16 */
   lockval = CHIPSET_REG32(REG_SYS_LOCK);
   CHIPSET_REG32(REG_SYS_LOCK) = lockval | SYS_LOCK_BIT;
   
   return mb;
}
