/* kernel/ports/arm/versatilepb/startup.s
 * ARM-specific low-level start-up routines for the Versatile PB dev board
 * Author : Chris Williams
 * Date   : Fri,11 Mar 2011.16:59:00
 
Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

.code 32
.balign 4

.global _Reset                /* entry point */
.global KernelBootStackBase   /* top of the boot stack */

/* kernel is expecting to run in the top 1GB of every process's
   virtual address space, so before the MMU is enabled with the
   kernel paged in from this high base address, we must subtract
   it from all references */
.set KernelVirtualBase, 0xc0000000

/* ------------------------------------------------------------ */

_Reset:
/* the machine has been reset/powered-on, the kernel is loaded
   at the 64K mark in physical memory. we're in SVC with MMU and
   interrupts off and God knows what in the ARM exception vector table.
   we've been booted as an ARM Linux kernel, so the following applies:
   http://www.arm.linux.org.uk/developer/booting.php
   http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html
 
   => r0 = 0
      r1 = Supported machine number
           387 = Versatile PB
           Full list is at: http://www.arm.linux.org.uk/developer/machines/download.php
      r2 = physical address of environment table
*/

/* use r4 to convert virtual-to-physical addresses by subtracting
   it from the virtual addresses - use before we enable the MMU */
MOV   r4, #KernelVirtualBase

/* install default emergency handlers at 0x0 */
MOV   r5, #0
LDR   r6, =KernelBootExceptionTable
SUB   r6, r6, r4
MOV   r3, #0
copyloop:
LDR   r7, [r6, r5, LSL #2]
STR   r7, [r3, r5, LSL #2]
ADD   r5, r5, #1
CMP   r5, #16                 /* 16 entries: 8 for branch tbl, 8 for vectors */
BNE   copyloop

/* zero space from where boot page table will be stored at 48K mark, allowing
   for 4096 table entries (16K) to represent 4096 x 1M of virtual memory space */
MOV   r5, #4096               /* 4096 entries in the table */
MOV   r6, #0xc000             /* 48K mark */

MOV   r7, #0                  /* zero whole words at a time */
zeroloop:
SUB   r5, r5, #1
STR   r7, [r6, r5, LSL #2]
CMP   r5, #0
BNE   zeroloop

/* write the KernelBootPgTableEntry at the 64KB marker */
LDR   r5, =KernelBootPgTableEntry
SUB   r5, r5, r4
LDR   r5, [r5]
MOV   r6, #1024
LSL   r6, r6, #4              /* r6 = 48KB phys base */
STR   r5, [r6]

/* the kernel is going to be mapped from 0xc0000000 so
   write an entry at table offset 0xc00 to point to the
   lowest 1M of physical mem */
MOV   r3, #0xc00
STR   r5, [r6, r3, LSL #2]

/* map 1M of the system registers at 0x10000000 physical
   into the kernel's space at 0xc0000000 + 0x10000000 */
MOV   r5, #0x100
ADD   r7, r3, r5
LDR   r5, =KernelBootPgTableSysRegs
SUB   r5, r5, r4
LDR   r5, [r5]
STR   r5, [r6, r7, LSL #2]

/* don't forget to map the serial hardware at 0x10100000
   phys in at 0xc0000000 + 0x10100000 virtual */
MOV   r5, #0x100
ADD   r5, r5, #1              /* get 0x101 into r5 */
ADD   r7, r3, r5
LDR   r5, =KernelBootPgTableSerial
SUB   r5, r5, r4
LDR   r5, [r5]
STR   r5, [r6, r7, LSL #2]

/* clear the translation base control address so we
   only use TTB0 */
MOV   r5, #0
MCR   p15, 0, r5, c2, c0, 2

/* set the translation table base 0 (TTB0) to point to
   our boot page table */
MOV   r5, #1024
LSL   r5, r5, #4              /* 2^4 * 1024 = 16K */
ORR   r5, r5, #0xf            /* cachable, sharable */
MCR   p15, 0, r5, c2, c0, 0

/* enable domain 0 by setting bit 1 for this domain */
MOV   r5, #1
MCR   p15, 0, r5, c3, c0, 0

/* disable the fast context switch extension system */
MOV   r5, #0
MCR   p15, 0, r5, c13, c0, 0
MCR   p15, 0, r5, c13, c0, 1

/* fingers crossed! flush entire TLB and enable the MMU */
MCR   p15, 0, r5, c8, c7, 0
LDR   r5, =KernelBootMMUFlags
SUB   r5, r5, r4
LDR   r5, [r5]
MCR   p15, 0, r5, c1, c0, 0

/* there may be pipeline issues so do nothing for a moment */
NOP
NOP

/* locate the stack base in virtual space, restore the bootloader's
   parameters and enter the C kernel */
LDR   sp, =KernelBootStackBase
BL    preboot

/* if r0 is non-zero then we have the green light to run the
   core kernel */
CMP   r0, #0
BEQ   parked

LDR   r1, =KernelMultibootMagic
LDR   r1, [r1]
BL    _main

/* halt if we get to this point */
parked:
B .

/* ------------------------------------------------------------ */

/* boot exception vector table */
KernelBootExceptionTable:
LDR   pc, [pc, #24]     /* reset */
LDR   pc, [pc, #24]     /* undefined instruction */
LDR   pc, [pc, #24]     /* software interrupt */
LDR   pc, [pc, #24]     /* prefetch abort */
LDR   pc, [pc, #24]     /* data abort */
LDR   pc, [pc, #24]     /* reserved */
LDR   pc, [pc, #24]     /* IRQ */
LDR   pc, [pc, #24]     /* FIQ */

/* this table must follow the above branch table */
KernelBootExceptionVectors:
.word _Reset                       - KernelVirtualBase
.word KernelBootExceptionUndefInst - KernelVirtualBase
.word KernelBootExceptionSWI       - KernelVirtualBase
.word KernelBootExceptionPreAbt    - KernelVirtualBase
.word KernelBootExceptionDataAbt   - KernelVirtualBase
.word _Reset                       - KernelVirtualBase
.word KernelBootExceptionIRQ       - KernelVirtualBase
.word KernelBootExceptionFIQ       - KernelVirtualBase

/* each emergency handler will write out a panic string and halt */
KernelBootExceptionUndefInst:
LDR   r0, =KernelBootStrUndefInst
B     KernelBootExceptionPanic

KernelBootExceptionSWI:
LDR   r0, =KernelBootStrSWI
B     KernelBootExceptionPanic

KernelBootExceptionPreAbt:
LDR   r0, =KernelBootStrPreAbt
B     KernelBootExceptionPanic

KernelBootExceptionDataAbt:
LDR   r0, =KernelBootStrDataAbt
B     KernelBootExceptionPanic

KernelBootExceptionIRQ:
LDR   r0, =KernelBootStrIRQ
B     KernelBootExceptionPanic

KernelBootExceptionFIQ:
LDR   r0, =KernelBootStrFIQ

KernelBootExceptionPanic:
LDR   sp, =KernelBootStackBase
BL    debug_panic
B     .

KernelBootStrUndefInst:
.asciz "Unexpected failure: undefined instruction"
KernelBootStrSWI:
.asciz "Unexpected failure: software interrupt"
KernelBootStrPreAbt:
.asciz "Unexpected failure: pre-fetch abort"
KernelBootStrDataAbt:
.asciz "Unexpected failure: abort on data transfer"
KernelBootStrIRQ:
.asciz "Unexpected failure: hardware interrupt"
KernelBootStrFIQ:
.asciz "Unexpected failure: fast hardware interrupt"
.balign 4

/* ------------------------------------------------------------ */

/* identity map the lowest 1M into the kernel's high memory:
   bits  31-20 = 1M section base addr
         18    = 0
         17    = nG: 0 for global
         16    = S: 1 for shared
         15    = APX: 0 to disable extra protections
         14-12 = TEX: 0 for strongly ordered, shareable
         11-10 = AP: 1 for privileged access only
         8-5   = Domain: 0 for booting kernel
         4     = XN: 0 for executable
         3     = C: 0 for strongly ordered, shareable
         2     = B: 0 for strongly ordered, shareable
         1-0   = 2 for 1M section
*/
KernelBootPgTableEntry:
.word 0x00010402  /* point at 0x0 phys mem */

/* craft a mapping for the system's chipset registers */
KernelBootPgTableSysRegs:
.word 0x10002402  /* point at 0x10000000 phys mem, non-shared device */

/* craft a mapping for the serial hardware */
KernelBootPgTableSerial:
.word 0x10102402  /* point at 0x10100000 phys mem, non-shared device */

KernelBootMMUFlags:
.word 0x800009 /* enable, write buffer, no subpages */

/* ------------------------------------------------------------ */

KernelMultibootMagic:
.word 0x2badb002 /* see kernel/core/include/multiboot.h */

/* ------------------------------------------------------------ */



