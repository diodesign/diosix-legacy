/* user/lib/i386/libdiosix/include/diosix.h
 * prototypes and structures for the i386 port of the kernel relevant to userspace
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

/* message passing - control flags bits high */
#define DIOSIX_MSG_REPLY       (1 << 31) /* message is a reply */
#define DIOSIX_MSG_MULTIPART   (1 << 30) /* message split into multiple blocks */
#define DIOSIX_MSG_RECVONREPLY (1 << 29) /* block on recv after replying */
#define DIOSIX_MSG_SENDASUSR   (1 << 28) /* send message as an unpriv'd user process */
#define DIOSIX_MSG_KERNELONLY  (1 << 27) /* accept signals from the kernel only */
/* simple type bits low (bits 0-3) */
#define DIOSIX_MSG_GENERIC     (1)
#define DIOSIX_MSG_SIGNAL      (2)
#define DIOSIX_MSG_TYPEMASK    ((1 << 3) - 1)
/* zero is a reserved tid/pid */
#define DIOSIX_MSG_ANY_THREAD  (0)
#define DIOSIX_MSG_ANY_PROCESS (0)

/* the kernel will refuse to deliver individual messages greater than this size in bytes */
#define DIOSIX_MSG_MAX_SIZE    (4096 * 4)

/* describe a queued asynchronous message */
typedef struct
{
   unsigned int number; /* the signal number */
   unsigned int extra;  /* an extra word of information */
} diosix_signal;

/* message passing - describe an outgoing multipart message */
typedef struct
{
   unsigned int size;
   void *data;
} diosix_msg_multipart;

/* message passing - describe an outgoing message and params for the reply */
typedef struct
{
   unsigned int pid;        /* process pid sending to/receiving from, or 0 for any */
   unsigned int tid;        /* threasd tid sending to/receiving from, or 0 for any */
   unsigned int flags;      /* status flags and type for this message */

   unsigned int send_size;  /* size in bytes of the send message data, or number of multipart entries to send */
   void *send;              /* pointer to the send message data as a block or multipart */
   diosix_signal signal;    /* signal info block */
   
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

/* reason codes for requesting info from the kernel */
#define DIOSIX_THREAD_INFO  (0)
#define DIOSIX_PROCESS_INFO (1)
#define DIOSIX_KERNEL_INFO  (2)

/* reason codes for driver management */
#define DIOSIX_DRIVER_REGISTER       (0)
#define DIOSIX_DRIVER_DEREGISTER     (1)
#define DIOSIX_DRIVER_MAP_PHYS       (2)
#define DIOSIX_DRIVER_UNMAP_PHYS     (3)
#define DIOSIX_DRIVER_REGISTER_IRQ   (4)
#define DIOSIX_DRIVER_DEREGISTER_IRQ (5)

/* vmm_area flags */
#define VMA_READABLE    (0 << 0)
#define VMA_WRITEABLE   (1 << 0)
#define VMA_MEMSOURCE   (1 << 1) /* on fault, map in a physical page if set, or bump the userspace manager if not */
                                 /* this is overridden by page bit 9 */
#define VMA_NOCACHE     (1 << 2) /* disable caching on pages in this area */
#define VMA_FIXED       (1 << 3) /* do not swap out physical pages in this VMA */
#define VMA_HASPHYS     (1 << 7) /* hint to the vmm that a page has physical memory assigned to it */

/* describe a physical memory request */
typedef struct
{
   void *paddr; /* the physical base address to map, must be page aligned */
   void *vaddr;  /* the virtual base address to map into, must be page aligned */
   unsigned int size;   /* number of bytes to map in, must be in whole number of pages */
   unsigned char flags; /* set the type of mapping */
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
   union
   {
      diosix_thread_info  t;
      diosix_process_info p;
      diosix_kernel_info  k;
   } data;
} diosix_info_block;

#endif
