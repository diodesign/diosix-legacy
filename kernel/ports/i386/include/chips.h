/* kernel/ports/i386/include/chips.h
 * prototypes and structures to describe the system's main chips 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _CHIPS_H
#define   _CHIPS_H

/* chip types */
typedef enum 
{
   notpresent = 0,
   cpu_core,
   ioapic
} chip_type;

/* chip states */
typedef enum
{
   enabled = 0,  /* chip can be used */   
   disabled       /* chip has been disabled at boot */
} chip_state;

/* describe a cpu core */
typedef struct
{
   thread *current;          /* must point to the thread being run */
   rw_gate lock;             /* lock for the cpu metadata */
   
   /* prioritised run queues */
   thread *queue_head, *queue_tail;
   thread *queue_marker[SCHED_PRIORITY_LEVELS]; /* priority levels */
   unsigned int queued; /* how much workload this processor has */
   
   /* pointers to this CPU's gdt table and into its TSS selector */
   gdtptr_descr gdtptr;
   gdt_entry *tssentry;
} chip_core;

/* define an ioapic chip */
typedef struct
{
   unsigned int baseaddr;
} chip_ioapic;


/* define per-chip table entry */
typedef struct
{
   /* describe this chip in a generic sense */
   chip_type type;
   chip_state state;
   
   /* BIOS-assigned system ID for this chip */
   unsigned int apic_id;
   
   /* pointer to data with info specific to this chip */
   union
   {
      chip_ioapic *ioapic;
      chip_core *core;
   } data;
} chip_entry;

extern chip_entry *chip_table;

#endif
