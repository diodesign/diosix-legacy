/* user/lib/newlib/libgloss/libnosys/sbrk.c
 * Change the amount of heap memory allocated to the process
 * Author : Chris Williams
 * Date   : Sat,25 Dec 2010.00:08:44

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

/* platform independent definitions */
#include "config.h"
#include <_syslist.h>
#include <errno.h>
#undef errno
extern int errno;

/* diosix-specific definitions */
#include "diosix.h"
#include "functions.h"

/* describe this process's heap area */
char *heap_start = 0, *heap_end = 0;
unsigned int heap_inuse, heap_size_max, heap_size_min;

/* spinlock to protect critical values */
volatile unsigned int heap_lock;

void *
_sbrk (incr)
     int incr;
{
   extern char end; /* set by the linker  */ 
   char *ptr;

   diosix_debug_write("sbrk() called");
   
   /* critical section - keep other threads out */
   DIOSIX_SPINLOCK_ACQUIRE(&heap_lock);
   
   if(heap_start == 0)
   {
      /* need to create a heap area, but what if
         incr is zero? return where the heap 
         will start if so */
      if(incr == 0)
      {
         diosix_debug_write("sbrk() incr == 0 for uninitialised heap");
         DIOSIX_SPINLOCK_RELEASE(&heap_lock);
         return (void *)DIOSIX_PAGE_ROUNDUP(&end);
      }
   
      /* can't shrink a non-existent heap */
      if(incr < 0)
      {
         errno = ENOMEM;
         diosix_debug_write("sbrk() failed at incr < 0 check");
         DIOSIX_SPINLOCK_RELEASE(&heap_lock);
         return (void *)-1;
      }
   
      /* create the initial memory area */
      if(diosix_memory_create((void *)DIOSIX_PAGE_ROUNDUP(&end), incr))
      {
         errno = ENOMEM;
         diosix_debug_write("sbrk() failed at heap creation syscall");
         DIOSIX_SPINLOCK_RELEASE(&heap_lock);
         return (void *)-1;
      }
      
      /* set the access rights */
      diosix_memory_access((void *)DIOSIX_PAGE_ROUNDUP(&end), VMA_WRITEABLE);
      
      /* initialise the heap's statistics and return */
      heap_start = heap_end = (void *)DIOSIX_PAGE_ROUNDUP(&end);
      heap_inuse = incr;
      heap_size_max = DIOSIX_PAGE_ROUNDUP(incr);
      heap_size_min = DIOSIX_PAGE_ROUNDDOWN(incr);
      
      DIOSIX_SPINLOCK_RELEASE(&heap_lock);
      return (void *)heap_start;
   }
 
   /* sanity check - don't shrink beyond the heap's size */
   if(incr < 0 && heap_inuse < abs(incr))
   {
      errno = ENOMEM;
      diosix_debug_write("sbrk() failed at heap shrink check");
      DIOSIX_SPINLOCK_RELEASE(&heap_lock);
      return (void *)-1;
   }
 
   /* the kernel is only interested in whole page resizes
      so if we can resize within page boundaries then do so
      and do not trouble the kernel */
   if(((heap_inuse + incr) >= heap_size_max) ||
      ((heap_inuse + incr) < heap_size_min))
   {
      /* we'll cross a page boundary so try to resize the heap area */
      if(diosix_memory_resize(heap_start, incr))
      {
         errno = ENOMEM;
         diosix_debug_write("sbrk() failed at heap resize syscall");
         DIOSIX_SPINLOCK_RELEASE(&heap_lock);
         return (void *)-1;
      }
   }

   /* update statistics */
   heap_inuse += incr;
   heap_size_max = DIOSIX_PAGE_ROUNDUP(heap_inuse);
   heap_size_min = DIOSIX_PAGE_ROUNDDOWN(heap_inuse);
   
   /* calculate the pointer to return, which should
      be the base of the extended or shrunken heap */
   if(incr > 0)
   {
      ptr = heap_end;
      heap_end += incr;
   }
   else
   {
      heap_end += incr;
      ptr = heap_end;
   }

   /* end critical section */
   DIOSIX_SPINLOCK_RELEASE(&heap_lock);
   return (void *)ptr;
}
