/* kernel/ports/i386/cpu/fpu.c
 * i386-specific floating-point co-processor support
 * Author : Chris Williams
 * Date   : Sat,23 Jul 2011.14:53.00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#include <portdefs.h>

/* fpu_state_size
   Look up the number of bytes needed to store the FP state on this system
   <= number of bytes required, or 0 for no supported/no present FPU
*/
unsigned int fpu_state_size(void)
{
   switch((fpu_type)KernelSIMDPresent)
   {
      case fpu_type_basic:
      case fpu_type_mmx:
         return X86_FPU_STATE_SIZE_PRESSE;
         
      case fpu_type_sse:
      case fpu_type_sse2:
      case fpu_type_sse3:
         return X86_FPU_STATE_SIZE_SSE;
         
      default:
         /* unsupported FPU type */
         return 0;
   }
}

/* fpu_align_state
   Align a state block to the next suitable boundary within a kernel heap block.
   Note: there must be suitable padding space at the end of the heap block to
   accomodate this realignment.
   => addr = base address of heap block
   <= aligned address to use with FP store/restore state instructions
*/
unsigned int fpu_align_state(unsigned int addr)
{
   unsigned int aligned = addr & X86_FPU_STATE_ALIGNMASK;
   if(aligned == addr) return addr;
      
   return aligned + X86_FPU_STATE_PADDING;
}

/* fpu_restore_state
   Load the processor's FP registers with the state stored for a thread.
   If no FP state exists then allocate space for one and initialise it.
   => target = thread with FP state to restore
   <= 0 for success, or an error code
*/
kresult fpu_restore_state(thread *target)
{
   unsigned char *aligned_block;
   
   /* sanity check */
   if(!target) return e_bad_params;
   
   /* if there's no state to load then create a blank state */
   if(target->fp == NULL)
   {
      kresult err;
      unsigned int block_size = fpu_state_size() + X86_FPU_STATE_PADDING;

      err = vmm_malloc(&(target->fp), block_size);
      if(err) return err;
      
      /* zero it */
      vmm_memset(target->fp, 0, block_size);
   }
   
   aligned_block = (unsigned char *)fpu_align_state((unsigned int)target->fp);
   
   /* execute the right restore instruction for the supported FPU */
   switch((fpu_type)KernelSIMDPresent)
   {
      case fpu_type_basic:
      case fpu_type_mmx:
         __asm__ __volatile__ ("frstor %0" : : "m" (*aligned_block) : "memory");
         break;
         
      case fpu_type_sse:
      case fpu_type_sse2:
      case fpu_type_sse3:
         __asm__ __volatile__ ("fxrstor %0" : : "m" (*aligned_block) : "memory");
         break;
         
      default:
         /* unsupported FPU type */
         return e_failure;
   }
   
   /* reload the FPU configuration word with the kernel default */
   __asm__ __volatile__("fldcw %0" : : "m" (KernelFPConfig));
   
   return success;
}

/* fpu_restore_state
   Load the processor's FP registers with the state stored for a thread
   => target = thread with FP state to restore
   <= 0 for success, or an error code
*/
kresult fpu_save_state(thread *target)
{
   unsigned char *aligned_block;
   
   /* sanity check */
   if(!target) return e_bad_params;
   
   /* if there's no state to save into then something's gone very wrong */
   if(target->fp == NULL)
   {
      KOOPS_DEBUG("OMGWTF! fpu_save_state: thread %i of process %i has no space for state\n",
                  target->tid, target->proc->pid);
      return e_failure;
   }
   
   aligned_block = (unsigned char *)fpu_align_state((unsigned int)target->fp);

   /* execute the right restore instruction for the supported FPU */
   switch((fpu_type)KernelSIMDPresent)
   {
      case fpu_type_basic:
      case fpu_type_mmx:
         __asm__ __volatile__("fnsave %0" : "=m" (*aligned_block) : : "memory");
         break;
         
      case fpu_type_sse:
      case fpu_type_sse2:
      case fpu_type_sse3:
         __asm__ __volatile__("fxsave %0" : "=m" (*aligned_block) : : "memory");
         break;
         
      default:
         /* unsupported FPU type */
         return e_failure;
   }
   
   /* reload the FPU configuration word with the kernel default */
   __asm__ __volatile__("fldcw %0" : : "m" (KernelFPConfig));
   
   return success;
}

