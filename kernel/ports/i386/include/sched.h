/* kernel/ports/i386/include/sched.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _SCHED_H
#define   _SCHED_H

/* rate at which thread timeslices are reduced by 1 (in Hz) */
#define SCHED_FREQUENCY   (100)

/* scheduling */
void sched_initialise(void);
void sched_pre_initalise(void);
void sched_add(unsigned char cpu, unsigned char priority, thread *torun);
void sched_remove(thread *victim, thread_state state);
void sched_tick(int_registers_block *regs);
void sched_pick(int_registers_block *regs);
void sched_move_to_end(unsigned char cpu, unsigned char priority, thread *toqueue);
kresult sched_lock_proc(process *proc);
kresult sched_unlock_proc(process *proc);
kresult sched_lock_thread(thread *victim);
kresult sched_unlock_thread(thread *towake);
unsigned char sched_pick_queue(unsigned char hint);

#endif
