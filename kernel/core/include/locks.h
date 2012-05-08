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
#define LOCK_READ            (0)
#define LOCK_WRITE           (1 << 0)
#define LOCK_SELFDESTRUCT    (1 << 1)
#define LOCK_WRITEWAITING    (1 << 2)

/* Determine the owner of a lock gate (a) or set the owner (b) of a gate (a).
   Only use once lock_gate() has been successful but before unlock_gate() is called. */
#define LOCK_GET_OWNER(a)    (thread *)((rw_gate *)(a)->owner)
#define LOCK_SET_OWNER(a, b) ((rw_gate *)(a))->owner = (unsigned int)(b)

/* number of bytes including terminator of lock name */
#define LOCK_DESCRIPT_LENGTH (8)

/* locking primitives for processes and threads */
typedef struct
{
   /* lock value, owning thread, and status flags
      This structure must be kept tight and a factor of 4096 (a 4K page) */
   volatile unsigned int spinlock, owner, flags, refcount;
   
#ifdef DEBUG_LOCK_RWGATE_PROFILE
   unsigned int read_count, write_count;
   unsigned char description[LOCK_DESCRIPT_LENGTH];
#endif
} rw_gate;

/* calculate the size of the pool bitmap in bits and bytes */
#define LOCK_POOL_BITMAP_LENGTH_BITS   (MEM_PGSIZE / sizeof(rw_gate))
#define LOCK_POOL_BITMAP_LENGTH_BYTES  (LOCK_POOL_BITMAP_LENGTH_BITS / 8)

/* describe a page of rw_gate locks */
typedef struct rw_gate_pool rw_gate_pool;
struct rw_gate_pool
{
   /* describe location of pool page in memory and which blocks are free */
   unsigned int physical_base, virtual_base;
   unsigned char bitmap[LOCK_POOL_BITMAP_LENGTH_BYTES];
   
   /* indicates how many locks are available and which bit was last thought to be free */
   unsigned char nr_free, last_free;
   
   /* double-linked list pointers */
   rw_gate_pool *prev, *next;
};

/* low-level locking mechanisms */
void lock_spin(volatile unsigned int *spinlock);
void unlock_spin(volatile unsigned int *spinlock);
kresult lock_gate(rw_gate *gate, unsigned int flags);
kresult unlock_gate(rw_gate *gate, unsigned int flags);

#endif
