/* user/lib/newlib/libc/sys/diosix-i386/diosix.h
 * prototypes, defines and structures in the kernel relevant to userspace
 * Author : Chris Williams
 * Date   : Sat,14 Nov 2009.17:17:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _DIOSIX_H
#define   _DIOSIX_H

#ifndef NULL
#define NULL      (0)
#endif

/* standard result codes */
typedef enum
{
   success              = 0,
   e_failure            = 1, /* generic fault */
   e_not_found          = 2, /* search failed */
   e_notimplemented     = 3, /* function not supported */
   e_missing_mb_data,
   e_no_phys_pgs,
   e_no_handler,
   e_no_rights,
   e_no_receiver,
   e_signal_pending,
   e_not_pg_aligned,
   e_not_enough_pgs,
   e_not_contiguous,
   e_not_enough_bytes,
   e_too_big,
   e_too_small,
   e_phys_stk_overflow,
   e_payload_obj_here,
   e_payload_missing,
   e_payload_unexpected,
   e_bad_address,
   e_bad_source_address,
   e_bad_target_address,
   e_bad_magic,
   e_bad_arch,
   e_bad_exec,
   e_bad_params,
   e_vma_exists,
   e_exists,
   e_max_layer
} kresult;

#define POSIX_GENERIC_FAILURE   ((unsigned int)(-1))

/* software interrupts list */
#define SYSCALL_EXIT          (0)
#define SYSCALL_FORK          (1)
#define SYSCALL_KILL          (2)
#define SYSCALL_THREAD_YIELD  (3)
#define SYSCALL_THREAD_EXIT   (4)
#define SYSCALL_THREAD_FORK   (5)
#define SYSCALL_THREAD_KILL   (6)
#define SYSCALL_MSG_SEND      (7)
#define SYSCALL_MSG_RECV      (8)
#define SYSCALL_PRIVS         (9)
#define SYSCALL_INFO          (10)
#define SYSCALL_DRIVER        (11)
#define SYSCALL_MEMORY        (12)
#define SYSCALL_THREAD_SLEEP  (13)
#define SYSCALL_ALARM         (14)
#define SYSCALL_SET_ID        (15)
#define SYSCALL_USRDEBUG      (16)

/* manage a process's POSIX-conformant ids */
#define DIOSIX_SETPGID   (1) /* set process group id */
#define DIOSIX_SETSID    (2) /* set session id */
#define DIOSIX_SETEUID   (3) /* set effective user id */
#define DIOSIX_SETREUID  (4) /* set real user id */
#define DIOSIX_SETRESUID (5) /* set effective, real and saved user id */
#define DIOSIX_SETEGID   (6) /* set effective group id */
#define DIOSIX_SETREGID  (7) /* set real group id */
#define DIOSIX_SETRESGID (8) /* set effective, real and saved group id */

#define DIOSIX_SET_GROUP (0) /* alter a group id */
#define DIOSIX_SET_USER  (1) /* alter a user id */

/* diosix-specific process credential management */
#define DIOSIX_SET_ROLE  (9) /* register a role in the system */

/* the root uid/gid */
#define DIOSIX_SUPERUSER_ID (0)

/* contains the POSIX-defined real, effective and saved-set ids for processes */
typedef struct
{
   unsigned int real, effective, saved;
} posix_id_set;

/* define the rate at which the scheduler is interrupted, the system-wide scheduling tick */
#define DIOSIX_SCHED_TICK     (100) /* 100 times a second, one tick is 10ms */
/* calculate how many scheduling ticks there are per second */
#define DIOSIX_TICKS_PER_SECOND(a) ((a) * DIOSIX_SCHED_TICK)
#define DIOSIX_MSEC_PER_TICK       (1000 / DIOSIX_SCHED_TICK)
/* define the number of scheduler ticks to sleep for inbetween trying to resend
   a DIOSIX_MSG_QUEUEME message to a process that isn't revceiving yet */
#define DIOSIX_SCHED_MSGWAIT  (10)

/* message passing - control flags bits high */
#define DIOSIX_MSG_REPLY       (1 << 31) /* message is a reply */
#define DIOSIX_MSG_MULTIPART   (1 << 30) /* message split into multiple blocks */
#define DIOSIX_MSG_RECVONREPLY (1 << 29) /* block on recv after replying */
#define DIOSIX_MSG_SENDASUSR   (1 << 28) /* send message as an unpriv'd user process */
#define DIOSIX_MSG_KERNELONLY  (1 << 27) /* accept signals from the kernel only */
#define DIOSIX_MSG_SHAREVMA    (1 << 26) /* share a VMA in reply message */
#define DIOSIX_MSG_QUEUEME     (1 << 25) /* queue a non-reply non-signal message and block until received */
#define DIOSIX_MSG_INMYPROCGRP (1 << 24) /* send the signal to all processes in sender's process group */
#define DIOSIX_MSG_INAPROCGRP  (1 << 23) /* send the signal to all processes in process group selected by pid */
/* simple type bits low (bits 0-3) */
#define DIOSIX_MSG_GENERIC     (1)
#define DIOSIX_MSG_SIGNAL      (2)
#define DIOSIX_MSG_TYPEMASK    (DIOSIX_MSG_GENERIC | DIOSIX_MSG_SIGNAL)
/* zero is a reserved tid/pid */
#define DIOSIX_MSG_ANY_THREAD  (0)
#define DIOSIX_MSG_ANY_PROCESS (0)

/* the kernel will refuse to deliver individual messages greater than this size in bytes* */
#define DIOSIX_MSG_MAX_SIZE    (4096 * 4)

/* describe a queued asynchronous message */
typedef struct
{
   unsigned int number;   /* the signal number */
   unsigned int extra;    /* an extra word of information */
} diosix_signal;

/* describe a memory share request */
typedef struct
{
   unsigned int base, size;
} diosix_share_request;

/* message passing - describe an outgoing multipart message */
typedef struct
{
   unsigned int size;
   void *data;
} diosix_msg_multipart;

/* update multipart array (a) element (e) with data (d) and size (s) */
#define DIOSIX_WRITE_MULTIPART(a, e, d, s) { (a)[(e)].data = (void *)(d); (a)[(e)].size = (unsigned int)(s); }

/* message passing - describe an outgoing message and params for the reply */
typedef struct
{
   unsigned int role;       /* name the receiving process by role, or 0 for use pid+tid */
   unsigned int pid;        /* process pid sending to/receiving from, or 0 for any */
   unsigned int tid;        /* threasd tid sending to/receiving from, or 0 for any */
   unsigned int uid;        /* POSIX-conformant effective user id for the message sender */
   unsigned int gid;        /* POSIX-conformant effective group id for the message sender */
   unsigned int flags;      /* status flags and type for this message */

   unsigned int send_size;  /* size in bytes of the send message data, or number of multipart entries to send */
   void *send;              /* pointer to the send message data as a block or multipart */
   
   diosix_signal signal;    /* signal info block */

   diosix_share_request mem_req; /* memory sharing request */

   unsigned int recv_max_size; /* max size in bytes the reply/recv can be, or 0 for sending a reply */
   unsigned int recv_size; /* actual number of bytes in the reply/recv, or 0 for sending a reply */
   void *recv;             /* pointer to buffer for the reply/recv data */
} diosix_msg_info;

/* reason codes for priv/rights management */
#define DIOSIX_PRIV_LAYER_UP   (0)
#define DIOSIX_RIGHTS_CLEAR    (1)
#define DIOSIX_IORIGHTS_REMOVE (2)
#define DIOSIX_IORIGHTS_CLEAR  (3)
#define DIOSIX_UNIX_SIGNALS    (4)
#define DIOSIX_KERNEL_SIGNALS  (5)

/* reason codes for debugging with the kernel */
#define DIOSIX_DEBUG_WRITE     (0)

/* reason codes for requesting info from the kernel */
#define DIOSIX_THREAD_INFO       (0)
#define DIOSIX_PROCESS_INFO      (1)
#define DIOSIX_KERNEL_INFO       (2)
#define DIOSIX_KERNEL_STATISTICS (3)

/* reason codes for driver management */
#define DIOSIX_DRIVER_REGISTER       (0)
#define DIOSIX_DRIVER_DEREGISTER     (1)
#define DIOSIX_DRIVER_MAP_PHYS       (2)
#define DIOSIX_DRIVER_UNMAP_PHYS     (3)
#define DIOSIX_DRIVER_REGISTER_IRQ   (4)
#define DIOSIX_DRIVER_DEREGISTER_IRQ (5)
#define DIOSIX_DRIVER_IOREQUEST      (6)
#define DIOSIX_DRIVER_REQ_PHYS       (7)
#define DIOSIX_DRIVER_RET_PHYS       (8)

/* define an IO request via a hardware IO port, if available */
typedef enum
{
   ioport_read, ioport_write
} diosix_ioport_request_type;

typedef struct
{
   diosix_ioport_request_type type;
   char size; /* number of bytes to write: 1, 2 or 4 */
   unsigned short port;
   unsigned int data_out;
   unsigned int data_in;
} diosix_ioport_request;

/* reason codes for memory management */
#define DIOSIX_MEMORY_CREATE         (0)
#define DIOSIX_MEMORY_DESTROY        (1)
#define DIOSIX_MEMORY_RESIZE         (2)
#define DIOSIX_MEMORY_ACCESS         (3)
#define DIOSIX_MEMORY_LOCATE         (4)

/* size of a page in bytes */
#define DIOSIX_MEMORY_PAGESIZE       (4096)

/* vmm_area access flags */
#define VMA_READABLE    (0 << 0)
#define VMA_WRITEABLE   (1 << 0)
#define VMA_MEMSOURCE   (1 << 1) /* on fault, map in a physical page if set, or bump the userspace manager if not */
                                 /* this is overridden by page bit 9 */
#define VMA_NOCACHE     (1 << 2) /* disable caching on pages in this area */
#define VMA_FIXED       (1 << 3) /* do not swap out physical pages in this VMA */
#define VMA_EXECUTABLE  (1 << 4) /* code can be executed in this vma */
#define VMA_SHARED      (1 << 5) /* inhibit copy-on-write and share the area with other processes */
#define VMA_HASPHYS     (1 << 7) /* hint to the vmm that a page has physical memory assigned to it */
#define VMA_ACCESS_MASK (VMA_WRITEABLE | VMA_EXECUTABLE | VMA_NOCACHE | VMA_SHARED)
#define VMA_GENERIC     (0 << 8) /* vma has no pre-defined purpose */
#define VMA_TEXT        (1 << 8) /* vma contains the process's executable image */
#define VMA_DATA        (2 << 8) /* vma contains the process's data area */
#define VMA_STACK       (3 << 8) /* vma contains a thread's stack */
#define VMA_TYPE_MASK   (VMA_GENERIC | VMA_TEXT | VMA_DATA | VMA_STACK)

/* describe a physical memory request */
typedef struct
{
   void *paddr; /* the physical base address to map, must be page aligned */
   void *vaddr;  /* the virtual base address to map into, must be page aligned */
   unsigned int size;   /* number of bytes to map in, must be in whole number of pages */
   unsigned char flags; /* set the type of mapping using the above VMA flags */
} diosix_phys_request;

/* thread information block */
typedef struct
{
   /* describe this thread */
   unsigned int tid, cpu;
   unsigned char priority;
} diosix_thread_info;
   
typedef struct
{
   /* describe the owning process */
   unsigned int pid, parentpid;
   unsigned char flags, privlayer;
   unsigned int role;
   
   /* POSIX-conformant real, effective and saved-set user and group ids */
   unsigned int ruid, euid, ssuid;
   unsigned int rgid, egid, ssgid;
   
   /* POSIX-conformant process group and session ids */
   unsigned int proc_group_id, session_id;
} diosix_process_info;

typedef struct
{
   /* all about the kernel build */
   char identifier[64];
   unsigned char release_major, release_minor;
   unsigned char kernel_api_revision;
} diosix_kernel_info;

typedef struct
{
   unsigned int kernel_uptime; /* rough uptime in msec */
} diosix_kernel_stats;

typedef struct
{
   union
   {
      diosix_thread_info  t;
      diosix_process_info p;
      diosix_kernel_info  k;
      diosix_kernel_stats s;
   } data;
} diosix_info_block;

#endif
