/* kernel/ports/arm/startup.s
 * ARM-specific low-level start-up routines
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

/* ------------------------------------------------------------ */

_Reset:
/* the machine has been reset/powered-on, the kernel is loaded
   at the 64K mark in physical memory. we're in SVC with MMU and
   interrupts off and God knows what in the ARM exception vector table */

/* use r4 to convert virtual-to-physical addresses by subtracting
   it from the virtual addresses - use before we enable the MMU */
MOV   r4, #0xc0000000

/* install default emergency handlers at 0x0 */
MOV   r0, #0
LDR   r1, =KernelBootExceptionTable
SUB   r1, r1, r4
MOV   r3, #0
copyloop:
LDR   r2, [r1, r0, LSL #2]
STR   r2, [r3, r0, LSL #2]
ADD   r0, r0, #1
CMP   r0, #16                 /* 16 entries: 8 for branch tbl, 8 for vectors */
BNE   copyloop

/* zero space from where boot page table will be stored at 16K mark
   4096 table entries (16K) for 4096 x 1M virtual memory space */
MOV   r0, #4096
MOV   r1, #1024
LSL   r1, r1, #4              /* 2^4 * 1024 = 16K */

MOV   r2, #0x00000000         /* zero whole words at a time */
zeroloop:
SUB   r0, r0, #1
STR   r2, [r1, r0, LSL #2]
CMP   r0, #0
BNE   zeroloop

/* write the KernelBootPgTableEntry at the 16KB marker */
LDR   r0, =KernelBootPgTableEntry
SUB   r0, r0, r4
LDR   r0, [r0]
MOV   r1, #1024
LSL   r1, r1, #4              /* r1 = 16KB phys base */
STR   r0, [r1]

/* the kernel is going to be mapped from 0xc0000000 so
   write an entry at table offset 0xc00 to point to the
   lowest 1M of physical mem */
MOV   r2, #0xc00
STR   r0, [r1, r2, LSL #2]

/* don't forget to map the serial hardware at 0x10100000
   phys in at 0xc0000000 + 0x10100000 virtual */
MOV   r0, #0x100
ADD   r0, r0, #1              /* get 0x101 into r0 */
ADD   r2, r2, r0
LDR   r0, =KernelBootPgTableSerial
SUB   r0, r0, r4
LDR   r0, [r0]
STR   r0, [r1, r2, LSL #2]

/* clear the translation base control address so we
   only use TTB0 */
MOV   r0, #0
MCR   p15, 0, r0, c2, c0, 2

/* set the translation table base 0 (TTB0) to point to
   our boot page table at the 16K mark */
MOV   r0, #1024
LSL   r0, r0, #4              /* 2^4 * 1024 = 16K */
ORR   r0, r0, #0xf            /* cachable, sharable */
MCR   p15, 0, r0, c2, c0, 0

/* enable domain 0 by setting bit 1 for this domain */
MOV   r0, #1
MCR   p15, 0, r0, c3, c0, 0

/* disable the fast context switch extension system */
MOV   r0, #0
MCR   p15, 0, r0, c13, c0, 0
MCR   p15, 0, r0, c13, c0, 1

/* fingers crossed! flush entire TLB and enable the MMU */
MCR   p15, 0, r0, c8, c7, 0
LDR   r0, =KernelBootMMUFlags
SUB   r0, r0, r4
LDR   r0, [r0]
MCR   p15, 0, r0, c1, c0, 0

/* locate the stack base in virtual space and enter the C kernel */
LDR   sp, =KernelBootStackBase
BL    main

/* halt if we get to this point */
B .

/* ------------------------------------------------------------ */

/* emergency boot exception vector table */
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
.word _Reset - 0xc0000000
.word KernelBootExceptionUndefInst - 0xc0000000
.word KernelBootExceptionSWI - 0xc0000000
.word KernelBootExceptionPreAbt - 0xc0000000
.word KernelBootExceptionDataAbt - 0xc0000000
.word _Reset - 0xc0000000
.word KernelBootExceptionIRQ - 0xc0000000
.word KernelBootExceptionFIQ - 0xc0000000

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

/* craft a mapping for the serial hardware */
KernelBootPgTableSerial:
.word 0x10102402  /* point at 0x10100000 phys mem, non-shared device */

KernelBootMMUFlags:
.word 0x800009 /* enable, write buffer, no subpages */

/* ------------------------------------------------------------ */



