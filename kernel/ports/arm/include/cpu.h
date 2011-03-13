/* kernel/ports/arm/include/cpu.h
 * prototypes and structures for the ARM port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _CPU_H
#define   _CPU_H
         
/* find this cpu's unique id */
#define CPU_ID                 (0)

/* keep track of available processor resources */
extern unsigned char mp_cpus;
extern unsigned char mp_boot_cpu;

/* the cpu's per-thread task descriptor */
struct __attribute__((packed)) tss_descr
{
   unsigned int esp0; /* kernel stack for the thread */
};

/* chip states */
typedef enum
{
   enabled = 0,  /* chip can be used */   
   disabled       /* chip has been disabled at boot */
} chip_state;

/* describe an mp core */
typedef struct
{
   chip_state state;
   
   thread *current;
} mp_core;

extern mp_core *cpu_table;

#endif
