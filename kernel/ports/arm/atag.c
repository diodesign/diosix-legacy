/* kernel/ports/arm/atag.c
 * ARM-specific ATAG bootloader support
 * Author : Chris Williams
 * Date   : Sun,12 Mar 2011.03:39.00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* atag_fixup_initrd
   Fixup a loaded blob of multiboot payload modules so that their
   offsets are added to their load address to calculate final physical
   address pointers for the core portable kernel
   => addr = physical address of the payload blob start, its structure
             is defined in release/arm_versatilepb/mkpayload.c
      size = size of the payload blob in bytes
   <= returns the number of modules present
*/
unsigned int atag_fixup_initrd(unsigned int phys_addr, unsigned int size)
{
   mb_module_t *module;
   unsigned int mods_found = 0;
   unsigned int mods_end = (unsigned int)KERNEL_PHYS2LOG(phys_addr + size);
   unsigned int virt_addr = (unsigned int)KERNEL_PHYS2LOG(phys_addr);

   payload_blob_header *hdr = (payload_blob_header *)virt_addr;
   
   /* bail out if we've been given an empty blob */
   if(hdr->present == 0) return 0;
      
   /* move addr beyond the header metadata */
   virt_addr += sizeof(payload_blob_header);

   do
   {
      /* get pointer to this module */
      module = (mb_module_t *)virt_addr;
         
      /* use the initial phys addr as a base for the offsets */
      module->mod_start += phys_addr;
      module->mod_end   += phys_addr;
      module->string    += phys_addr;
            
      /* move to the end of the module */
      virt_addr = (unsigned int)KERNEL_PHYS2LOG(module->mod_end);
      mods_found++;
   }
   while(virt_addr < mods_end && mods_found < hdr->present);
      
   BOOT_DEBUG("[atag] initrd said to contain %i module(s), %i found\n",
              hdr->present, mods_found);
   
   return mods_found;
}
 
/* pointer to current free phys RAM addr in multiboot area,
   which immediately follows ATAG area */
unsigned char *mb_alloc_ptr = NULL;

/* atag_mb_alloc
   Allocate a block of physical RAM to store some multiboot data
   This function will keep allocations contiguous in memory and
   zero memory areas before returning the pointer
   => size = number of bytes to allocate
   <= pointer to start of block in physical memory, or NULL
      for failure
*/
void *atag_mb_alloc(unsigned int size)
{
   /* bail out if we try to use this func before mb_alloc_ptr is set */
   if(!mb_alloc_ptr)
   {
      KOOPS_DEBUG("atag_mb_alloc: allocation attempted before mb_alloc_ptr is defined\n");
      return NULL;
   }
   
   /* can't allocate zero-sized areas */
   if(!size) return NULL;
   
   /* return the current alloc ptr after moving it forward size bytes */
   void *retval = mb_alloc_ptr;
   mb_alloc_ptr = (unsigned char *)((unsigned int)mb_alloc_ptr + size);
   
   /* zero the memory area */
   vmm_memset(KERNEL_PHYS2LOG(retval), 0, size);
   
   return retval;
}

/* atag_process
   Process an ATAG list supplied by a bootloader into a multiboot structure
   => item = virtual address of ATAG list to grok
   <= phys address of the multiboot structure or 0 for failure
*/
multiboot_info_t *atag_process(atag_item *item)
{
   unsigned int atag_length = 0;
   atag_item *ptr = item;
   multiboot_info_t *mb_base;
   unsigned int mem_areas = 0, mem_area_count = 0;
   mb_memory_map_t *mem_array;
   
   /* sanity check */
   if(!item || (unsigned int)item < KERNEL_SPACE_BASE) return NULL;
   
   /* count up length of the tag list, we'll place the multiboot straight after */
   while(ptr->type != atag_none)
   {
      /* also count number of memory areas */
      if(ptr->type == atag_mem) mem_areas++;
      
      atag_length += ptr->size;
      ptr = ATAG_NEXT(ptr);
   }
   
   /* convert word count into number of bytes */
   mb_base = (multiboot_info_t *)((unsigned int)item + (atag_length * sizeof(unsigned int)));
   mb_alloc_ptr = KERNEL_LOG2PHYS((unsigned int)mb_base + sizeof(multiboot_info_t));
   
   /* start constructing the multiboot structure by zero'ing the structure */
   vmm_memset(mb_base, 0, sizeof(multiboot_info_t));
   
   /* allocate table of memory areas */
   if(mem_areas)
   {
      mb_base->mmap_length = sizeof(mb_memory_map_t) * mem_areas;
      mb_base->mmap_addr = (unsigned int)atag_mb_alloc(mb_base->mmap_length);
      mem_array = KERNEL_PHYS2LOG(mb_base->mmap_addr);
      
      if(!mb_base->mmap_addr)
      {
         KOOPS_DEBUG("atag_process: failed to allocate multiboot memory area array\n");
         return NULL;
      }
   }
   
   /* run through the list of environment information passed to us by the bootloader */
   while(item->type != atag_none)
   {
      switch(item->type)
      {
         case atag_core:
            if(item->size > 2)
            {
               BOOT_DEBUG("[atag] flags: 0x%x, %i bytes per page\n",
                          item->data.core.flags, item->data.core.page_size);
               
               if(item->data.core.page_size != MEM_PGSIZE)
                  debug_panic("processor page size incompatible with kernel");
            }
            break;
            
         case atag_mem:
            BOOT_DEBUG("[atag] RAM area: 0x%x - 0x%x size: %iMB (%i bytes)\n",
                       item->data.mem.physaddr,
                       item->data.mem.physaddr + item->data.mem.size - 1,
                       item->data.mem.size >> 20, item->data.mem.size);
            
            /* calc length of the data structure in bytes minus the size word */
            mem_array[mem_area_count].size = sizeof(mb_memory_map_t) - sizeof(unsigned long);
            mem_array[mem_area_count].base_addr_low = item->data.mem.physaddr;
            mem_array[mem_area_count].length_low = item->data.mem.size;
            mem_array[mem_area_count].type = MULTIBOOT_MEMTYPE_RAM;
            mem_area_count++;
            
            mb_base->flags |= MULTIBOOT_FLAGS_MEMMAP;
            break;
            
         case atag_initrd2:
            BOOT_DEBUG("[atag] initrd: 0x%x size: %i bytes\n",
                       item->data.initrd2.physaddr,
                       item->data.initrd2.size);
            
            /* store the details of the initrd in phys mem - the core kernel doesn't care for
               the header data we prepended to the modules but does expect the pointers to
               be valid, hence the fixup proc call */
            mb_base->mods_addr = (unsigned int)item->data.initrd2.physaddr + sizeof(payload_blob_header);
            mb_base->mods_count = atag_fixup_initrd(item->data.initrd2.physaddr, item->data.initrd2.size);
            
            mb_base->flags |= MULTIBOOT_FLAGS_MODS;
            break;
            
         default:
            BOOT_DEBUG("[atag] unsupported entry %x\n", item->type);
      }
      
      item = ATAG_NEXT(item);
   }

   return KERNEL_LOG2PHYS(mb_base);
}


