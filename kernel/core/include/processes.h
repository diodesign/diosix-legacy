/* kernel/core/include/processes.h
 * prototypes and structures for managing processes by the kernel
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

/* PID and TID 0 are reserved for the kernel, UID/GID 0 for the superuser */
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
   waitingformsg,     /* not in queue, not running, waiting for a non-reply msg */
   waitingaftersig,   /* not in queue, not running, waiting after a signal interrupted it */
   held,              /* not in queue, not running, forced to wait by a senior process */
   dead               /* not in queue, not running, not waiting, soon to be destroyed */
} thread_state;

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
typedef struct mp_thread_queue mp_thread_queue; /* in cpu.h */

/* a queued signal block held in the kernel */
typedef struct
{
   diosix_signal signal; /* the signal number and code */
   unsigned int sender_pid, sender_tid; /* process & thread IDs of the sender */
   unsigned int sender_uid, sender_gid; /* POSIX-conformant user and group ids of the sender */
} queued_signal;

/* a queued synchronous message sender */
typedef struct
{
   unsigned int pid, tid; /* process & thread IDs of the sender */
} queued_sender;

#define THREAD_FLAG_INUSERMODE       (1 << 0)
#define THREAD_FLAG_ISDRIVER         (1 << 1)
#define THREAD_FLAG_HASIOBITMAP      (1 << 2)

/* needed in the process and thread structs */
typedef struct kpool kpool;
typedef struct kpool_block kpool_block;
typedef struct vmm_tree vmm_tree;

typedef struct process process; /* keep the compiler nice and sweet */
typedef struct thread thread;
typedef struct tss_descr tss_descr;

/* describe each thread */
struct thread
{
   process *proc;             /* pointer to owner */
   unsigned int tid;          /* thread id, unique within process */
   unsigned int cpu;          /* actual cpu running this thread */
   
   unsigned char flags;          /* status flags for this thread */
   unsigned char timeslice;      /* preempt when it reaches zero */
   unsigned char priority;       /* priority level - see sched.h */
   unsigned char priority_granted; /* a priority level temporarily granted by another process */
   unsigned int priority_points; /* when a thread changes priority this variable is
                                    loaded with the value 2^priority. One point is deducted
                                    if the thread has to be preempted but gains a point if it
                                    blocks (such as sending a message). If it hits zero, it drops 
                                    a priority level and is rescheduled. If it hits 2*(2^priority)
                                    then it is bumped up a priority level and rescheduled. */
   
   thread_state state;  /* the running state of the thread */
   
   thread *replysource; /* thread awaiting reply from */
   diosix_msg_info msg; /* copy of the message block ptr submitted to syscall msg_send/recv */
   diosix_msg_info *msg_src; /* pointer to the user-supplied msg block ptr */
   
   /* simple thread locking mechanism - acquire a lock before modifying
    or reading the thread's structure */
   rw_gate lock;

   mp_thread_queue *queue; /* the run-queue the thread exists in */
   thread *queue_prev, *queue_next; /* the run-queue's double-linked list */
   thread *hash_prev, *hash_next; /* pointers through thread hash table */
   
   /* normally zero, but set to a role number if waiting for a process to
      appear with this particular role */
   unsigned char waiting_for_role;
   
   /* thread registers */
   unsigned int stackbase; /* where this thread's user stack should start */
   unsigned int kstackbase, kstackblk; /* kernel stack ptrs */
   
   /* port implementation specific stuff */
   int_registers_block regs; /* saved general purpose register state of a thread */
   void *fp; /* saved state of the thread's floating-point state */
   tss_descr *tss; /* x86 - pointer to the task-state block */
};

/* describe a linked list of threads waiting on a role to be assigned to a process */
typedef struct role_snoozer role_snoozer;
struct role_snoozer
{
   thread *snoozer; /* the thread waiting for the process to wake up */
   role_snoozer *previous, *next;
};

/* process status flags (scheduling) */
#define PROC_FLAG_RUNLOCKED   (1 << 0)

/* process status flags (rights) */
#define PROC_FLAG_CANMSGASUSR   (1 << 1) /* can send messages as a user process */
#define PROC_FLAG_CANBEDRIVER   (1 << 2) /* can register as a driver process */
#define PROC_FLAG_CANMAPPHYS    (1 << 3) /* is allowed to map physical memory into its virtual space */
#define PROC_FLAG_CANUNIXSIGNAL (1 << 4) /* is allowed to send POSIX-compatible signals (sig code 0-31) */
#define PROC_FLAG_CANPLAYAROLE  (1 << 5) /* can register a set role within the operating system */ 
#define PROC_FLAG_HASCALLEDEXEC (1 << 6) /* process has called exec() to replace its image */
#define PROC_RIGHTS_MASK  (PROC_FLAG_CANMSGASUSR | \
                           PROC_FLAG_CANBEDRIVER | \
                           PROC_FLAG_CANMAPPHYS | \
                           PROC_FLAG_CANUNIXSIGNAL | \
                           PROC_FLAG_CANPLAYAROLE)

/* needed in the process struct */
typedef struct irq_driver_entry irq_driver_entry;

/* defines a supplementary group entry */
typedef struct
{
   unsigned int gid;
} posix_supplementary_group;

/* physical memory allocations for privileged processes */
typedef struct process_phys_mem_block process_phys_mem_block;
struct process_phys_mem_block
{
   process_phys_mem_block *previous, *next;
   
   void *base;
   unsigned short pages;
};

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
   unsigned int priority_low, priority_high; /* minimum and maximum scheduling
                                                priority for this process's threads */ 
   
   /* memory management */
   unsigned int entry; /* where code execution should begin */
   vmm_tree *mem; /* tree of memory areas */
   process_phys_mem_block *phys_mem_head;
   
   /* rights management */
   /* processes are ordered by layers - these are not to be confused
      with x86-style ring levels. The microkernel leaves fine-grained
      access control to userland to fight it out, and only recognises
      processes by their layer. Messages can only
      be sent down through layers (replies can be sent up) to avoid
      deadlocks; processes can only manipulate those in layers above
      them or their own children. The layer list:
      0 = reserved for init (which becomes the sytem executive)
      1 = reserved for hardware-specific drivers (eg: hard disc driver)
      2 = reserved for hardware-independent drivers (eg: ext2 filesystem)
      3 = reserved for service managers (eg: the VFS)
      4 = reserved for lower services (eg: the page swapper/demand loader)
      5 = reserved for upper services (eg: executable loader)
      6 = reserved for the security layer (which checks permisisons to access trusted processes)
      7 = reserved for user programs */
   unsigned char layer; /* this process's layer */
   
   unsigned int role; /* the role the process plays in the system, if any */
   
   /* POSIX-conformant user and group ids for this process */
   posix_id_set uid, gid;
   
   /* POSIX-conformant process group and session ids */
   unsigned int proc_group_id, session_id;
   
   /* pool of POSIX-conformant supplementary group ids */
   kpool *supplementary_groups;

   /* x86 IO port access - a pointer to a 2^16-bit bitmap where each bit corresponds 
      to an IO port. A set bit indicates access is denied. A NULL pointer here means
      the process has no IO port access */
   unsigned int *ioport_bitmap;
   
   /* linked list of registered driver structures */
   irq_driver_entry *interrupts;
   
   /* signal management */
   unsigned int unix_signals_accepted; /* mask for the first 32 signals (UNIX-compatible ones) */
   unsigned int unix_signals_inprogress; /* bitfield for the first 32 signals (UNIX-compatible ones) */
   unsigned int kernel_signals_accepted; /* bitfield for the kernel's signals */
   kpool *system_signals; /* queue of UNIX-compatible and kernel signals */
   kpool *user_signals; /* queue of user defined signals */
   kpool *msg_queue; /* queue of synchronous messages waiting to be delivered to the process */
};

#define LAYER_MAX        (255)
#define LAYER_EXECUTIVE  (0)
#define LAYER_DRIVERS    (1)
#define FLAGS_EXECUTIVE  (PROC_RIGHTS_MASK) /* give executive all rights */

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
kresult proc_send_group_signal(unsigned int pgid, thread *sender, unsigned int signum, unsigned int sigcode);
kresult proc_is_valid_pgid(unsigned int pgid, unsigned int sid, process *exclude);
kresult proc_is_child(process *parent, process *child);
kresult proc_kill(unsigned int victimpid, process *slayer);
kresult proc_layer_up(process *proc);
kresult proc_clear_rights(process *proc, unsigned int bits);
thread *thread_new(process *proc);
kresult thread_new_hash(process *proc);
thread *thread_duplicate(process *proc, thread *source);
thread *thread_find_thread(process *proc, unsigned int tid);
thread *thread_find_any_thread(process *proc);
kresult thread_kill(process *owner, thread *victim);
process *proc_role_lookup(unsigned int role);
kresult proc_role_add(process *target, unsigned int role);
kresult proc_role_remove(process *target, unsigned int role);
void proc_role_wakeup(unsigned char role);
kresult proc_wait_for_role(thread *snoozer, unsigned char role);
kresult proc_remove_phys_mem_allocation(process *proc, void *addr);
kresult proc_add_phys_mem_allocation(process *proc, void *addr, unsigned short pages);

#endif
