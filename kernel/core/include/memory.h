/* kernel/core/include/memory.h
 * prototypes and structures for portable aspects of kernel memory management
 * Author : Chris Williams
 * Date   : Mon,16 Mar 2011.21:43:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _MEMORY_H
#define   _MEMORY_H

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

/* system-independent page types */
typedef enum
{
   cached,
   notcached
} vmm_page_type;

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
kresult vmm_req_phys_pages(unsigned short pages, void **ptr, unsigned int pref);
kresult vmm_return_phys_pages(void *addr, unsigned int pages);
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
kresult vmm_add_vma(process *proc, unsigned int base, unsigned int size, unsigned int flags, unsigned int cookie);
kresult vmm_duplicate_vmas(process *new, process *source);
kresult vmm_destroy_vmas(process *victim);
vmm_decision vmm_fault(process *proc, unsigned int addr, unsigned int flags, unsigned char *rw_flag);
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
vmm_tree *vmm_lookup_vma(thread *caller, unsigned int type);

/* kernel global variables for managing physical pages */
extern unsigned int *phys_pg_stack_low_base;
extern unsigned int *phys_pg_stack_low_ptr;
extern unsigned int *phys_pg_stack_high_base;
extern unsigned int *phys_pg_stack_high_ptr;

#endif
