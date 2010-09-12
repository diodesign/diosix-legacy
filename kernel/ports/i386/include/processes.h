/* kernel/ports/i386/include/processes.h
 * prototypes and structures for managing processes by the i386 kernel
 * Author : Chris Williams
 * Date   : Wed,04 Apr 2007.22:09:57

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _PROCESSES_H
#define   _PROCESSES_H

/* PID and TID 0 are reserved */
#define RESERVED_PID (0)
#define FIRST_PID    (RESERVED_PID + 1)
#define FIRST_TID    (FIRST_PID)

/* thread states */
typedef enum
{
   sleeping = 0,      /* not in queue, not running, waiting for event */
   inrunqueue,        /* in queue, not running, waiting for cpu time */
   running,           /* in queue, is running, not waiting */
   waitingforreply,   /* not in queue, not running, waiting for a msg reply */
   waitingformsg,       /* not in queue, not running, waiting for a non-reply msg */
   held,              /* not in queue, not running, forced to wait by a senior process */
   dead               /* not in queue, not running, not waiting, soon to be destroyed */
} thread_state;

/* scheduler */
/* priority levels explained...
   lvl 0     = interrupt handlers
   lvl 1     = interactive processes
   lvl 2 & 3 = slower processes
*/
#define SCHED_PRIORITY_LEVELS     (4)
#define SCHED_TIMESLICE           (5) /* 50ms timeslice */
#define SCHED_FREQUENCY           (100) /* tick every 10ms */
#define SCHED_CARETAKER           (1000) /* run maintainance every 1000 ticks */

/* describe how an area of the payload is going to appear in memory */
typedef struct
{
   void *virtual;    /* virtual address it expects to be loaded */
   void *physical;   /* physical address it was loaded at */
   unsigned int size;       /* size in bytes of the area */
   unsigned int memsize;    /* total amount of mem required for area */
   unsigned int flags; /* access control to this area */
} payload_area;

#define PAYLOAD_CODE    (0)
#define PAYLOAD_DATA    (1)
#define PAYLOAD_EXECUTE (0x01)
#define PAYLOAD_READ    (0x02)
#define PAYLOAD_WRITE   (0x04)

typedef struct
{
   payload_area areas[2];   /* define main blocks of the program */
   void *entry;             /* where to begin execution */
   char *name;              /* ptr to module name in multiboot area */
} payload_descr;

typedef enum
{
   payload_bad = 0,
   payload_exe = 1,
   payload_sym = 2
} payload_type;

/* these are needed below and defined further down */
typedef struct process process;
typedef struct thread thread;

/* vmm_area flags */
#define VMA_READABLE    (0 << 0)
#define VMA_WRITEABLE   (1 << 0)
#define VMA_MEMSOURCE   (1 << 1) /* on fault, map in a physical page if set, or bump the userspace manager if not */
                                 /* this is overridden by page bit 9 */
#define VMA_HASPHYS     (1 << 7) /* hint to the vmm that a page has physical memory assigned to it */

/* a process memory area - arranged as a tree */
typedef struct
{
   unsigned int flags;       /* control aspects of this memory area */
   unsigned int refcount;  /* how many processes share this area? */
   unsigned int base, size; /* virtual addr base and size of the area in bytes */   

   unsigned int token; /* a cookie for the userspace page manager's reference */
   
   rw_gate lock; /* per-vma locking mechanism */
   
   /* the users here are processes */
   unsigned int users_max, users_ptr;
   process **users;
} vmm_area;

typedef struct vmm_tree vmm_tree;
struct vmm_tree
{
   /* pointer to the potentially shared area */
   vmm_area *area;
   
   /* metadata to place this area in a per-process tree */
   vmm_tree *left, *right;
   unsigned char colour;
};

/* the x86 cpu's TSS, one needed per thread */
typedef struct
{
   unsigned int prev_tss;
   unsigned int esp0;       /* kernel stack pointer for current thread */
   unsigned int ss0;        /* kernel stack segment for current thread */
   unsigned int esp1;       /* not used */
   unsigned int ss1;
   unsigned int esp2;
   unsigned int ss2;
   unsigned int cr3;
   unsigned int eip;
   unsigned int eflags;
   unsigned int eax;
   unsigned int ecx;
   unsigned int edx;
   unsigned int ebx;
   unsigned int esp;
   unsigned int ebp;
   unsigned int esi;
   unsigned int edi;
   unsigned int es;         
   unsigned int cs;        
   unsigned int ss;         
   unsigned int ds;         
   unsigned int fs;         
   unsigned int gs;         
   unsigned int ldt;        /* not used */
   unsigned short trap;
   unsigned short iomap_base;
} __attribute__((packed)) tss_descr;

/* low level gdt entry definition */
typedef struct
{
   unsigned short limit_low;          // lower 16 bits of the limit
   unsigned short base_low;           // lower 16 bits of the base
   unsigned char base_middle;         // 8 bits of the base
   unsigned char access;              // access flags, determine what ring this segment can be used in
   unsigned char granularity;
   unsigned char base_high;           // last 8 bits of the base
} __attribute__((packed)) gdt_entry;

/* 10-byte structure that describes a processor's GDT */
typedef struct
{
   unsigned short size;
   unsigned int ptr;
}  __attribute__((packed)) gdtptr_descr;

/* block of registers after an int/excep occurred */
typedef struct
{
   /* pushed by our interrupt code */
   unsigned int ds, edi, esi, ebp, esp, ebx, edx, ecx, eax;
   unsigned int intnum;
   
   /* pushed by the processor */
   unsigned int errcode, eip, cs, eflags, useresp, ss;
} int_registers_block;

#define THREAD_INUSERMODE   (1)

/* describe each thread */
struct thread
{
   process *proc;             /* pointer to owner */
   unsigned int tid;          /* thread id, unique within process */
   unsigned int cpu;          /* actual cpu running this thread */
   
   unsigned char flags;       /* status flags for this thread */
   unsigned char timeslice; /* preempt when it reaches zero */
   unsigned char priority;  /* priority level */
   unsigned char prev_priority; /* a previous priority level for this thread */
   
   thread_state state;      /* the running state of the thread */
   thread *replysource;       /* thread awaiting reply from */
   diosix_msg_info *msg;    /* copy of the message block ptr submitted to syscall msg_send/recv */
   
   /* simple thread locking mechanism - acquire a lock before modifying
    or reading the thread's structure */
   rw_gate lock;

   /* thread registers */
   unsigned int stackbase; /* where this thread's user stack should start */
   unsigned int kstackbase, kstackblk; /* kernel stack ptrs */
   tss_descr tss; /* the x86 task-state block */
   int_registers_block regs; /* saved state of a thread */

   thread *queue_prev, *queue_next; /* run-queue double-linked list */
   thread *hash_prev, *hash_next; /* pointers through thread hash table */
};

#define PROC_FLAG_RUNLOCKED   (1)

/* describe each process */
struct process
{
   unsigned int pid;           /* unique ID for this process */
   unsigned int parentpid;     /* ID for the current parent */
   unsigned int prevparentpid; /* ID of original parent */

   unsigned char cpu;       /* the id of the prefered cpu running the process - 
                               we try to keep threads together on the same cpu */
   unsigned char flags;     /* process status flags */
   
   /* simple process locking mechanism - acquire a lock before modifying
      or reading the process's structure */
   rw_gate lock;
   
   /* kernel logical address... but contains references
      to physical adresses */
   unsigned int **pgdir;   /* pointer to page directory */
   
   process *hash_prev, *hash_next; /* pid hash double-linked list */
   
   process **children; /* keep track of family in a growing list */
   unsigned int child_list_size, child_count;
   
   /* thread management */
   thread **threads; /* hash table of threads */
   unsigned int thread_count, next_tid;
   
   /* memory management */
   unsigned int entry; /* where code execution should begin */
   vmm_tree *mem; /* tree of memory areas */
   
   /* rights management */
   /* processes are ordered by layers - these are not to be confused
      with x86-style ring levels. The microkernel leaves fine-grained
      access control to userland to fight it out, and only recognises
      processes by their layer and syscall rights. Messages can only
      be sent down through layers (replies can be sent up) to avoid
      deadlocks; processes can only manipulate those in layers above
      them or their own children. The layer list:
      0 = reserved for the kernel (which has no processes/threads)
      1 = reserved for init (which becomes the sytem executive)
      2 = reserved for hardware-specific drivers (trusted)
      3 = reserved for hardware-independent managers (trusted)
      4 = user processes (untrusted) */
   unsigned int rights; /* bitmap of allowed restricted syscalls */
   unsigned char layer; /* this process's layer */
   
   /* signal management */
   unsigned int signalsaccepted, signalsinprogress;
   thread *signalhandler; /* direct signals to this thread */
};

#define LAYER_EXECUTIVE  (1)
#define RIGHTS_EXECUTIVE (0)

/* processes, threads and scheduling */
extern process *proc_sys_executive;
extern rw_gate proc_lock;

/* system limits */
#define PROC_MAX_NR         (1024)
#define PROC_HASH_ORDER     (3)
#define PROC_HASH_BUCKETS   (PROC_MAX_NR >> PROC_HASH_ORDER)
#define THREAD_MAX_NR       (1024)
#define THREAD_HASH_ORDER   (4)
#define THREAD_HASH_BUCKETS (THREAD_MAX_NR >> THREAD_HASH_ORDER)
#define THREAD_MAX_STACK    (4)

/* functions */
kresult proc_initialise(void);
process *proc_new(process *current, thread *caller);
process *proc_find_proc(unsigned int pid);
kresult proc_kill(unsigned int victimpid, process *slayer);
thread *thread_new(process *proc);
kresult thread_new_hash(process *proc);
thread *thread_duplicate(process *proc, thread *source);
thread *thread_find_thread(process *proc, unsigned int tid);
kresult thread_kill(process *owner, thread *victim);

#endif
