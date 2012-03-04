/* kernel/core/payload.c
 * diosix portable code to unpack modules loaded with the kernel
 * Author : Chris Williams
 * Date   : Tue,03 Apr 2007.22:29:29

Copyright (c) Chris Williams and individual contributors

   ELF parsing code cribbed from code by John-Mark Bell <jmb202@ecs.soton.ac.uk> with permission granted for use

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

/* NOTE: No locking required as this code is read-only, stateless and only called by the boot cpu */

#include <portdefs.h>
#include <elf.h>

/* the number of modules in the kernel payload */
unsigned int payload_modulemax = 0;

/* private copy of the multiboot data pointer */
multiboot_info_t *mbd_ptr = NULL;

/* payload_exist_here
   Check to see if a physical address lies within a payload object. This is
   rather inefficient if there are lots of modules...
   => ptr = address to check
   <= 0 if no payload object at addr, or result code
*/
kresult payload_exist_here(unsigned int ptr)
{
   mb_module_t *module = NULL;
   unsigned int loop;

   for(loop = 0; loop < payload_modulemax; loop++)
   {
      module = payload_readmodule(loop);
      
      if(ptr >= module->mod_start && ptr < module->mod_end)
         return e_payload_obj_here;
   }

   return success; /* fall through */
}

/* payload_readmodule
   Look up a module loaded with the kernel and return details about it.
   => modulenum = module number, 0 for first module
   <= pointer to multiboot module data or NULL for no more
*/
mb_module_t *payload_readmodule(unsigned int modulenum)
{
   mb_module_t *module = (mb_module_t *)mbd_ptr->mods_addr;
   
   if(!module)
      debug_panic("trying to probe payloads before preinit");
   
   module = (mb_module_t *)((unsigned int)module + 
                            (sizeof(mb_module_t) * modulenum));
   
   return (mb_module_t *)KERNEL_PHYS2LOG(module);
}

/* payload_parsemodule
   Attempt to parse a module. if it's an ELF, describe its layout so that the process
   management code can understand it and convert it into a runnable process.
   Ideally, any sort of executable loaders should be well outside this
   microkernel, but we're up against a chicken and the egg scenario. This
   function can understand static-linked ELF binaries. A module can also be a
   kernel symbol file, where the format is:
   KSYM
   addrinhex[space]sizeindecimal[space]symbolname[newline] (repeat until EOF)
   => module = pointer to structure describing the payload module to parse
      payload = pointer to payload info structure to fill in
   <= payload type or 0 for failure
*/
payload_type payload_parsemodule(mb_module_t *module, payload_descr *payload)
{
   unsigned char *magic = (unsigned char *)KERNEL_PHYS2LOG(module->mod_start);
   Elf32_Ehdr *fheader;
   Elf32_Phdr *pheader;
   
   /* is this a symbol table for debugging? */
   if(magic[0] == 'K' &&
      magic[1] == 'S' &&
      magic[2] == 'Y' &&
      magic[3] == 'M')
   {
      BOOT_DEBUG("[payload:%i] found a kernel symbol table (%x - %x)\n",
                 CPU_ID, module->mod_start, module->mod_end);
      if(debug_init_sym_table((char *)((unsigned int)magic + (sizeof(char) * 5)),
                              (char *)KERNEL_PHYS2LOG(module->mod_end)) == success)
         return payload_sym;
      else
         return payload_bad;
   }
   
   /* is this an ELF to load as a user process? */
   fheader = (Elf32_Ehdr *)KERNEL_PHYS2LOG(module->mod_start);
   BOOT_DEBUG("[payload:%i] parsing binary: %s (%x)", CPU_ID, (char *)KERNEL_PHYS2LOG(module->string), fheader);   
   
   /* sanity check the ELF we're trying to load */
   if(fheader->e_ident[0] != 0x7f ||
      fheader->e_ident[1] != 'E'  ||
      fheader->e_ident[2] != 'L'  ||
      fheader->e_ident[3] != 'F')
   {
      BOOT_DEBUG(" ... is not a valid ELF\n");
      return payload_bad;
   }

   /* check it's a supported architecture */
   if(fheader->e_machine != EM_PORT_X86 &&
      fheader->e_machine != EM_PORT_ARM)
   {
      BOOT_DEBUG(" ... is incompatible with this machine (type: 0x%x allowed: 0x%x, 0x%x)\n",
                 fheader->e_machine,
                 EM_PORT_X86, EM_PORT_ARM);
      return payload_bad;
   }

   /* fill in some basic info about this module */
   vmm_memset(payload, 0, sizeof(payload_descr));
   payload->name = (char *)KERNEL_PHYS2LOG(module->string);
   payload->entry = (void *)fheader->e_entry;

   /* now inspect the various headers to pull out the code and data */
   if(fheader->e_phoff)
   {
      int p_loop;
      for(p_loop = 0; p_loop < fheader->e_phnum; p_loop++)
      {
         pheader = (Elf32_Phdr *)((unsigned int)fheader + fheader->e_phoff +
                                  (p_loop * sizeof(Elf32_Phdr)));

         if(pheader->p_type == PT_LOAD)
         {
            /* we expect two areas: code and then data, and nothing else */
            if(p_loop > PAYLOAD_DATA)
            {
               BOOT_DEBUG(" ... has unexpected areas!\n");
               return payload_bad;
            }

            payload->areas[p_loop].virtual = (void *)pheader->p_vaddr;
            payload->areas[p_loop].physical = (void *)(pheader->p_offset +
                                              (unsigned int)fheader);
            payload->areas[p_loop].size = pheader->p_filesz;
            payload->areas[p_loop].memsize = pheader->p_memsz;
            payload->areas[p_loop].flags = pheader->p_flags;
         }
      }
   }

   BOOT_DEBUG(" ... done\n");
   return payload_exe;
}

/* payload_preinit
   Check to make sure we have some modules loaded with us, or life is going
   to be rather dull with just the microkernel present.
   => mbd = pointer to multiboot info in physical memory
   <= 0 for success, or result code
*/
kresult payload_preinit(multiboot_info_t *mbd)
{   
   mbd = (multiboot_info_t *)KERNEL_PHYS2LOG(mbd);

   /* keep a copy of the multiboot info pointer */
   mbd_ptr = mbd;

   /* bit 3 indicates modules loaded, although we must also check mods_count */
   if(!((mbd->flags & MULTIBOOT_FLAGS_MODS) && mbd->mods_count))
   {
      BOOT_DEBUG("[payload:%i] No modules loaded with kernel.\n", CPU_ID);
      return e_payload_missing;
   }

   BOOT_DEBUG("[payload:%i] %i module(s) in payload\n", CPU_ID, mbd->mods_count);

   payload_modulemax = mbd->mods_count;
   
   return success;
}
