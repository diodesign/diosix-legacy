/* kernel/ports/i386/include/memory.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/


#ifndef _MEMORY_H
#define   _MEMORY_H

/* assumed to be generated by linker script, used by KERNEL_ macros */
extern unsigned int _knlstart, _ebss;

/* defined in start.s */
extern unsigned int KernelPageDirectoryVirtStart;
extern unsigned int KernelPageDirectory;
extern unsigned int KernelGDT, KernelGDTEnd, KernelGDTPtr, TSS_Selector;
extern unsigned int KernelBootStackBase, APStack;

/* the start of our virtual memory at the top 1GB. note: we map as much of
   physical memory into our kernel's virtual space */
#define KERNEL_START         (&_knlstart)
#define KERNEL_END           (&_ebss)
#define KERNEL_SIZE          (KERNEL_END - KERNEL_START)
#define KERNEL_SPACE_BASE    (0xC0000000)
#define KERNEL_PHYSICAL_BASE (0x00400000)
#define KERNEL_PHYSICAL_TOP  (0xffffffff)
#define KERNEL_VIRTUAL_BASE  (KERNEL_SPACE_BASE + KERNEL_PHYSICAL_BASE)
#define KERNEL_VIRTUAL_END   (KERNEL_VIRTUAL_BASE + KERNEL_SIZE)
#define KERNEL_PHYSICAL_END  (KERNEL_PHYSICAL_BASE + KERNEL_SIZE)
#define KERNEL_PHYSICAL_END_ALIGNED (MEM_PGALIGN(KERNEL_PHYSICAL_END + MEM_PGSIZE))
#define KERNEL_LOG2PHYS(a)   ((void *)((unsigned int)(a) - KERNEL_SPACE_BASE))
#define KERNEL_PHYS2LOG(a)   ((void *)((unsigned int)(a) + KERNEL_SPACE_BASE))

/* the kernel is loaded at the 4M mark and the physical page stack
   descends from the 12M mark. these are critical kernel areas */
#define KERNEL_CRITICAL_BASE KERNEL_PHYSICAL_BASE
#define KERNEL_CRITICAL_END  MEM_PHYS_STACK_BASE

#define MEM_PGSIZE           (4 * 1024)
#define MEM_4M_PGSIZE        (4 * 1024 * 1024)
#define MEM_PGSHIFT          (12)
#define MEM_PGMASK           ((1 << MEM_PGSHIFT) - 1)
#define MEM_PGALIGN(a)       ((void *)((unsigned int)(a) & ~MEM_PGMASK))
#define MEM_PHYS_STACK_BASE  (12 * 1024 * 1024)
#define MEM_DMA_REGION_MARK  (16 * 1024 * 1024)
#define MEM_HIGH_PG          (2)
#define MEM_ANY_PG           (1)
#define MEM_LOW_PG           (0)

/* MEM_CLIP(base, size) - return size clipped to base+size =< KERNEL_PHYSICAL_TOP */
#define MEM_CLIP(a, b)       ( (unsigned int)(b) > (KERNEL_PHYSICAL_TOP - (unsigned int)(a)) ? (KERNEL_PHYSICAL_TOP - (unsigned int)(a)) : (b) )
/* MEM_IS_PG_ALIGNED(a) - return 1 for a page-aligned address or 0 for not */
#define MEM_IS_PG_ALIGNED(a) ( ((unsigned int)(a) & PG_4K_MASK) == (unsigned int)(a) ? 1 : 0 )
/* MEM_PG_ROUND_UP - round up to the nearest page-aligned address */
#define MEM_PG_ROUND_UP(a)   ( ((unsigned int)(a) + MEM_PGSIZE) & ~(MEM_PGMASK) )

/* vmm magic */
#define KHEAP_FREE           (0xdeaddead)
#define KHEAP_INUSE          (0xd105d105)
#define KPOOL_FREE           (0xd33dd33d)
#define KPOOL_INUSE          (0xd106d106)

/* heap management */
/* define the minimum block size in bytes to mitigate fragmentation */
#define KHEAP_BLOCK_MULTIPLE (128)

/* round the number of bytes in a kernel heap block up to a set multiple including 
   the metadata header */
#define KHEAP_ROUND_UP(a)  (((unsigned int)(a) + KHEAP_BLOCK_MULTIPLE + sizeof(kheap_block)) & ~(KHEAP_BLOCK_MULTIPLE - 1))

/* describe a kernel heap block */
typedef struct kheap_block kheap_block;
struct kheap_block
{
   unsigned int magic;    /* indicate either free or in use. any other value will indicate corruption */
   unsigned int inuse;    /* number of bytes allocated within the block, not including the header */
   unsigned int capacity; /* total size of the block, which will be a multiple of KHEAP_BLOCK_MULTIPLE, 
                             including the block header */
   kheap_block *previous, *next;  /* linked list pointers */
   /* ... data follows ... */
};

/* describe a kernel pool */
struct kpool_block
{
   unsigned int magic;           /* indicate either free or in use. any other value will indicate corruption */
   kpool_block *previous, *next; /* double-linked list of blocks in the pool */
   /* ... data follows ... */
};

struct kpool
{
   rw_gate lock;              /* to serialise updates to the pool */
   
   unsigned int block_size;   /* size of each block in the pool, not including the kpool_block header */
   unsigned int free_blocks;  /* number of free blocks */
   unsigned int inuse_blocks; /* number of inuse blocks */
   unsigned int total_blocks; /* number of max vailable blocks */
   kpool_block *head, *tail;  /* double linked list of inuse blocks */
   kpool_block *free;         /* double linked list of free blocks */

   void *pool;                /* pointer to pool's heap block */
};

/* limit pools and their blocks to sensible sizes */
#define KPOOL_MAX_BLOCKSIZE   (KHEAP_BLOCK_MULTIPLE * 2)
#define KPOOL_MAX_INITCOUNT   (256)
#define KPOOL_BLOCK_TOTALSIZE(a) (sizeof(kpool_block) + (unsigned int)a)

/* virtual memory management */
/* the kernel page management looks to the vmm for help.
   on a fault, these are the possible instructions the vmm will 
   ask the page code to carry out */
typedef enum
{
   clonepage,     /* grab a blank phys page, map it in at same virtual addr, copy the data, mark writeable */
   makewriteable, /* it's safe to mark the page as writeable and continue */
   newpage,       /* grab a blank phys page, map it in and continue */
   newsharedpage, /* find an existing mapping or map in a blank page for all processes */
   external,      /* get the external page manage to fix up this access */
   badaccess      /* the fault can't be handled */
} vmm_decision;

/* link a vma to processes through one or more of these mappings */
typedef struct
{
   process *proc;     /* process linked to this vma */
   unsigned int base; /* base address of the mapping in the linked process */
} vmm_area_mapping;

/* a virtual memory area - arranged as a tree. flags are defined in <diosix.h> */
typedef struct
{
   rw_gate lock; /* per-vma locking mechanism */
   
   unsigned int flags; /* control aspects of this memory area */
   unsigned int size;  /* size of the area in bytes */
   unsigned int token; /* a cookie for the userspace page manager's reference */
   
   kpool *mappings; /* pool of vmm_area_mapping structures for this vma */
} vmm_area;

struct vmm_tree
{
   /* pointer to the potentially shared area */
   vmm_area *area;
   
   unsigned int base; /* base address of the vma in the process's virtual memory space */
   
   /* metadata to place this area in a per-process tree */
   vmm_tree *left, *right;
   unsigned char colour;
};

kresult vmm_initialise(multiboot_info_t *mbd);
kresult vmm_req_phys_pg(void **addr, int pref);
kresult vmm_return_phys_pg(void *addr);
kresult vmm_enough_pgs(unsigned int size);
kresult vmm_ensure_pgs(unsigned int size, int type);
kresult vmm_free(void *addr);
kresult vmm_malloc(void **addr, unsigned int size);
kresult vmm_trim(kheap_block *block);
void vmm_heap_add_to_free(kheap_block *block);
unsigned int vmm_nullbufferlen(char *buffer);
void vmm_memset(void *addr, unsigned char value, unsigned int count);
void vmm_memcpy(void *target, void *source, unsigned int count);
kresult vmm_memcpyuser(void *target, process *tproc, void *source, process *sproc, unsigned int count);
kresult vmm_link_vma(process *proc, unsigned int baseaddr, vmm_area *vma);
kresult vmm_unlink_vma(process *owner, vmm_tree *victim);
vmm_area_mapping *vmm_find_vma_mapping(vmm_area *vma, process *tofind);
kresult vmm_add_vma(process *proc, unsigned int base, unsigned int size, unsigned char flags, unsigned int cookie);
kresult vmm_duplicate_vmas(process *new, process *source);
kresult vmm_destroy_vmas(process *victim);
vmm_decision vmm_fault(process *proc, unsigned int addr, unsigned char flags, unsigned char *rw_flag);
vmm_tree *vmm_find_vma(process *proc, unsigned int addr, unsigned int size);
kpool *vmm_create_pool(unsigned int block_size, unsigned int init_count);
kresult vmm_destroy_pool(kpool *pool);
kresult vmm_alloc_pool(void **ptr, kpool *pool);
kresult vmm_free_pool(void *ptr, kpool *pool);
void *vmm_next_in_pool(void *ptr, kpool *pool);
kresult vmm_create_free_blocks_in_pool(kpool *pool, unsigned int start, unsigned int end);
kresult vmm_fixup_moved_pool(kpool *pool, void *prev, void *new);
unsigned int vmm_count_pool_inuse(kpool *pool);
kresult vmm_alter_vma(process *owner, vmm_tree *node, unsigned int flags);
kresult vmm_resize_vma(process *owner, vmm_tree *node, signed int change);

/* kernel global variables for managing physical pages */
extern unsigned int *phys_pg_stack_low_base;
extern unsigned int *phys_pg_stack_low_ptr;
extern unsigned int *phys_pg_stack_high_base;
extern unsigned int *phys_pg_stack_high_ptr;

/* paging */
/* page fault codes */
#define PG_FAULT_P    (1<<0)  /* 0 = not present,   1 = access violation */
#define PG_FAULT_W    (1<<1)  /* 0 = tried to read, 1 = tried to write */
#define PG_FAULT_U    (1<<2)  /* 0 = in svc mode,   1 = in usr mode */
#define PG_FAULT_R    (1<<3)  /* 1 = reserved bits set in dir entry */
#define PG_FAULT_I    (1<<4)  /* 0 = not instruction fetch, 1 = instr fetch */

/* intel-specific page flags (see 3-25 pg 87) */
#define PG_PRESENT    (1<<0)  /* page present in memory */
#define PG_RW         (1<<1)  /* set for read+write, clear for read only */
#define PG_PRIVLVL    (1<<2)  /* set for usr, clear for knl */
#define PG_WRITETHRU  (1<<3)  /* enable write-through caching when set */
#define PG_CACHEDIS   (1<<4)  /* don't cache when set */
#define PG_ACCESSED   (1<<5)  /* page has been read from or written to */
#define PG_DIRTY      (1<<6)  /* page has been written to */
#define PG_SIZE       (1<<7)  /* set for 4MB page dir entries, 4KB otherwise */
#define PG_GLOBAL     (1<<8)  /* set to map in globally */

#define PG_EXTERNAL   (1<<9)  /* set to bump the page manager on fault, unset to use the vma's setting */
#define PG_PRIVATE    (1<<10) /* set to indicate this page is private to the process and can be released */

#define PG_DIR_BASE   (22)    /* physical addr in bits 22-31 */
#define PG_TBL_BASE   (12)    /* table index in bits 12-21 */
#define PG_TBL_MASK   (~((4 * 1024) - 1))
#define PG_INDEX_MASK (1023)  /* 10 bits set */

#define PG_4M_MASK    (~((4 * 1024 * 1024) - 1))
#define PG_4K_MASK    (~((4 * 1024       ) - 1))

void pg_init(void);
void pg_dump_pagedir(unsigned int *pgdir);
kresult pg_new_process(process *new, process *current);
kresult pg_destroy_process(process *victim);
kresult pg_add_4K_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int physical, unsigned int flags);
kresult pg_add_4M_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int physical, unsigned int flags);
kresult pg_fault(int_registers_block *regs);
kresult pg_preempt_fault(thread *test, unsigned int virtualaddr, unsigned int size, unsigned char flags);
kresult pg_do_fault(thread *target, unsigned int addr, unsigned int cpuflags);
void pg_postmortem(int_registers_block *regs);
kresult pg_user2phys(unsigned int *paddr, unsigned int **pgdir, unsigned int vaddr);
kresult pg_user2kernel(unsigned int *kaddr, unsigned int uaddr, process *proc);
kresult pg_remove_4K_mapping(unsigned int **pgdir, unsigned int virtual, unsigned int release_flag);

#endif
