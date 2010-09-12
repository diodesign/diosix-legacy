/* kernel/core/boot.c
 * diosix portable start up routines, including kernel's main() function
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* main
   The portable side of the kernel starts here. It is called by some bootstrap
   code, initialises the system and begins running programs. We'll assume the
   following:
   1. The kernel is mapped into 0xC0400000 (4MB into top 1GB) logical,
      and 0x00400000 physical.
   2. The lowest 2x4MB pages of physical RAM are mapped from 0xC0000000 logical.
   3. We're running on an architecture with 4KB page sizes, and the multiboot
      data will be stored below the lowest 1MB boundary. It's 4.30am, and I'll
      try to worry about portability later. A multiboot bootloader is also
      essential.
   => mbd = ptr to multiboot data about the system we're running in
      magic = special word passed by the bootloader to prove it is multiboot capable
*/
void _main(multiboot_info_t *mbd, unsigned int magic)
{
   if(DEBUG) debug_initialise();
   dprintf("[core] diosix-hyatt rev %s" " " __TIME__ " " __DATE__ " (built with GCC " __VERSION__ ")\n", SVN_REV);
   
   if(magic != MULTIBOOT_MAGIC) /* as defined in the multiboot spec */
      dprintf("*** warning: bootloader magic was %x (expecting %x).\n", magic, MULTIBOOT_MAGIC);
   
   /* ---- multiboot + SMP data must be preserved during these calls ------- */
   /* initialise interrupt handling and discover processor(s) */
   if(mp_initialise()) goto goforhalt;
   
   /* parse modules payloaded by the boot loader, best halt if there are none? */
   if(payload_preinit(mbd)) goto goforhalt;

   /* initialise the memory manager or halt if it fails */
   if(vmm_initialise(mbd)) goto goforhalt;

   /* initialise process and thread management, prepare first processes */
   sched_pre_initalise();
   
   /* bring up the processor(s) */
   mp_post_initialise();
   
   if(proc_initialise()) goto goforhalt;
   /* ---- multiboot + SMP data is no longer required by this point ------- */

   /* hocus pocus, mumbo jumbo, black magic */
   sched_initialise(); /* enable interrupts and start the execution of
                          userspace processes */
   /* shouldn't fall through... but halt if so */
   
goforhalt:
   /* halt here - there is nothing to return to */
   debug_panic("unexpected end-of-boot");
}
