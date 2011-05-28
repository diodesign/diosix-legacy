/* kernel/ports/arm/include/portdefs.h
 * master header for the ARM port of the kernel 
 * Author : Chris Williams
 * Date   : Sat, 12 Mar 2011.00:33:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _PORTDEFS_H
#define   _PORTDEFS_H

/* declare stuff shared with the userspace libraries */
#include "diosix.h"
#include "async.h"
#include "roles.h"

/* declare stuff exclusive to the microkernel */
#include <debug.h>
#include <multiboot.h>
#include <locks.h>
#include <registers.h>
#include <processes.h>
#include <sched.h>
#include <ipc.h>
#include <elf.h>

/* the order of these should not be important */
#include <atag.h>
#include <boot.h>
#include <memory.h>
#include <cpu.h>
#include <lowlevel.h>
#include <armboot.h>
#include <interrupts.h>
#include <syscalls.h>

/* access a 32bit system register */
#define CHIPSET_REG32(a) (*(volatile unsigned int *)(a))

#endif
