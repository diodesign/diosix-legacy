/* kernel/ports/i386/include/locks.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _LOCKS_H
#define   _LOCKS_H

/* allow a lock_gate timeout (in cpu cycles) if LOCK_TIME_CHECK is defined */
#define LOCK_TIMEOUT         (0xffffffff)

/* lock settings */
#define LOCK_READ           (0)
#define LOCK_WRITE          (1 << 0)
#define LOCK_SELFDESTRUCT   (1 << 1)
#define LOCK_WRITEWAITING   (1 << 2)

/* locking primitives for processes and threads */
typedef struct
{
   /* lock value, owning thread, and status flags */
   volatile unsigned int spinlock, owner, flags, refcount;
} rw_gate;

/* low-level locking mechanisms */
void lock_spin(volatile unsigned int *spinlock);
void unlock_spin(volatile unsigned int *spinlock);
kresult lock_gate(rw_gate *gate, unsigned int flags);
kresult unlock_gate(rw_gate *gate, unsigned int flags);

#endif
