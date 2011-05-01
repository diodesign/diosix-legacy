/* kernel/ports/arm/include/armboot.h
 * prototypes and structures for the ARM port of the kernel 
 * Author : Chris Williams
 * Date   : Sat,16 Apr 2011.20:45:00
 
 Copyright (c) Chris Williams and individual contributors
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/
 
 */

#ifndef _ARMBOOT_H
#define   _ARMBOOT_H

/* define machine types */
typedef enum
{
   versatilepb = 387
} arm_machine_id;

/* define the ATAG structures passed to the kernel after boot, full details here:
   http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html */

typedef enum
{
   atag_none      = 0x00000000,
   atag_core      = 0x54410001,
   atag_mem       = 0x54410002,
   atag_videotext	= 0x54410003,
   atag_ramdisk	= 0x54410004,
   atag_initrd2	= 0x54420005,
   atag_serial    = 0x54410006,
   atag_revision	= 0x54410007,
   atag_videolfb	= 0x54410008,
   atag_cmdline	= 0x54410009
} atag_type;

typedef struct
{
   unsigned int size;
   atag_type type;
   
   union
   {
      /* essential details from the bootloader */
      struct
      {
         unsigned int flags, page_size, root_device;
      } core;
      
      /* describe a section of physical memory */
      struct
      {
         unsigned int size, physaddr;
      } mem;
      
      /* describe a ram disk (initrd) */
      struct
      {
         unsigned int physaddr, size;
      } initrd2;
   } data;

} atag_item;

/* get a pointer to the next atag item */
#define ATAG_NEXT(p) ((atag_item *)((unsigned int)p + (p->size << 2)))

/* prototypes */
multiboot_info_t *atag_process(atag_item *list);

#endif
