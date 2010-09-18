/* kernel/ports/i386/include/cpu.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _CPU_H
#define   _CPU_H
         
/* find this cpu's unique id */
#define CPU_ID                 ((mp_cpus > 1) ? (*(LAPIC_ID_REG) >> 24) : mp_boot_cpu)

/* keep track of available processor resources */
extern unsigned char mp_cpus;
extern unsigned char mp_ioapics;
extern unsigned char mp_boot_cpu;

#define MP_MAGIC_SIG      (0x5f504d5f)   /* _MP_ */
#define MP_RSDT_SIG      (0x52534454)   /* RSDT */

/* areas of memory to search for the magic _MP_ word */
#define MP_EBDA_START   (0x80000)
#define MP_EBDA_END      (0x90000)
#define MP_LASTK_START   ((640 * 1024) - 1024)
#define MP_LASTK_END      (640 * 1024)
#define MP_ROM_START      (0xf0000)
#define MP_ROM_END      (0xfffff)
/* MP structure bits */
#define MP_IS_BSP              (1 << 1)
#define MP_IS_IOAPIC_ENABLED (1 << 0)

/* various multiproc info structures */
typedef struct
{
   unsigned int signature;
   void *configptr;
   unsigned char length;
   unsigned char version;
   unsigned char checksum;
   unsigned char features1;
   unsigned char features2;
} __attribute__((packed)) mp_header_block;

typedef struct
{
   unsigned int signature;
   unsigned short   base_table_length;
   unsigned char spec_rev;
   unsigned char checksum;
   unsigned char oemid[8];
   unsigned char productid[12];
   void *oem_table_pointer;
   unsigned short   oem_table_size;
   unsigned short   entry_count;
   void *apic_address;
   unsigned short   extended_table_length;
   unsigned char extended_table_checksum;
   unsigned char reserved;
} __attribute__((packed)) mp_config_block;


typedef struct
{
   unsigned char type;
   unsigned char id;
   
   union
   {
      /* IOAPIC interrupt routing information */
      struct
      {
         unsigned short flags;
         unsigned char source_bus_id; 
         unsigned char source_bus_irq;
         unsigned char dest_ioapic_id;
         unsigned char dest_ioapic_irq;
      } interrupt;
      
      /* system bus name */
      struct
      {
         char name[6];
      } bus;
      
      /* IOAPIC chip information */
      struct
      {
         unsigned char version;
         unsigned char flags;
         void *physaddr;
      } ioapic;
      
      /* CPU chip information */
      struct
      {
         unsigned char version;
         unsigned char flags;
         unsigned int signature;
         unsigned int features;
      } cpu;
   } entry;
   
} __attribute__((packed)) mp_entry_block;

typedef struct
{
   unsigned char signature[4];
   unsigned int  size;
   unsigned char version;
   unsigned char checksum;
   unsigned char oemid[6];
   unsigned char oemtid[8];
   unsigned int  oemversion;
   unsigned int  creatorid;
   unsigned int  creatorversion;
} __attribute__((packed)) mp_common_sdt_header;

typedef struct
{
   mp_common_sdt_header   header;
   mp_common_sdt_header **blocks;
} __attribute__((packed)) mp_rsdt_block;

typedef struct
{
   unsigned char signature[8];
   unsigned char checksum;
   unsigned char oem[6];
   unsigned char version;
   mp_rsdt_block *rsdt;
} __attribute__((packed)) mp_rsdp_header;

/* describe an mp core */
typedef struct
{
   chip_state state;
   thread *current;          /* must point to the thread being run */
   rw_gate lock;             /* lock for the cpu metadata */
   
   /* prioritised run queues */
   thread *queue_head, *queue_tail;
   thread *queue_marker[SCHED_PRIORITY_LEVELS]; /* priority levels */
   unsigned int queued; /* how much workload this processor has */
   
   /* pointers to this CPU's gdt table and into its TSS selector */
   gdtptr_descr gdtptr;
   gdt_entry *tssentry;
   
} mp_core;

/* multiprocessing support */
#define MP_AP_START_STACK_SIZE   (1024)
#define MP_AP_START_VECTOR         (0x80)
#define MP_AP_START_VECTOR_SHIFT (12)

extern mp_core *cpu_table;

kresult mp_initialise(void); /* if this fails then the machine is probably toast */
kresult mp_post_initialise(void);
void mp_catch_ap(void);

#endif
