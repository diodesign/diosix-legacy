/*
 * JS/Linux Linux starter
 *
 * Copyright (c) 2011 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* imported: Sat 28 May 2011 13:14:06 BST by Chris Williams
   Used under licence. Heavily modified to suit diosix's needs */

#include "linuxstart.h"

/* from plex86 (BSD license) */
struct  __attribute__ ((packed)) linux_params {
  unsigned long long gdt_table[256];
  unsigned long long idt_table[48];
};

void *memset(void *d1, int val, unsigned int len)
{
   char *d = d1;
   
   while (len--) {
      *d++ = val;
   }
   return d1;
}

void setup(int phys_ram_size, int initrd_size)
{
    struct linux_params *params;
   
    struct __attribute__((packed)) {
        short limit;
        void *addr;
    } gdt_data;

    params = (struct linux_params *)KERNEL_PARAMS_ADDR;
    memset(params, 0, sizeof(struct linux_params));
   
    params->gdt_table[2] = 0x00cf9a000000ffffLL; /* KERNEL_CS */
    params->gdt_table[3] = 0x00cf92000000ffffLL; /* KERNEL_DS */

    gdt_data.limit = sizeof(params->gdt_table);
    gdt_data.addr = params->gdt_table;
    
    /* init gdt */
    asm volatile ("lgdt (%0)" : : "r" (&gdt_data) : "memory");
}
