/* kernel/core/include/debug.h
 * prototypes and structures for the portable part of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _DEBUG_H
#define   _DEBUG_H

/* use with care - debugging is sent to the serial port by default 
   DEBUG is the master switch - define this to enable any debugging */
#ifdef DEBUG
#define LOG       (1)
#define dprintf   debug_printf
#else
#define LOG       (0)
#define dprintf   if(LOG) debug_printf
#endif

/* these are usually always on */
#define BOOT_DEBUG   dprintf  /* initialisation messages */
#define KOOPS_DEBUG   dprintf  /* potentially serious kernel failures */

/* these are conditional - define *_DEBUG to activate them */
#ifdef BUS_DEBUG
# undef BUS_DEBUG
# define BUS_DEBUG      dprintf
#else
# define BUS_DEBUG if(0) dprintf
#endif

#ifdef MSG_DEBUG
# undef MSG_DEBUG
# define MSG_DEBUG      dprintf
#else
# define MSG_DEBUG if(0) dprintf
#endif

#ifdef PROC_DEBUG
# undef PROC_DEBUG
# define PROC_DEBUG   dprintf
#else
# define PROC_DEBUG if(0) dprintf
#endif 

#ifdef SCHED_DEBUG
# undef SCHED_DEBUG
# define SCHED_DEBUG  dprintf
# define SCHED_DEBUG_QUEUES sched_list_queues()
#else
# define SCHED_DEBUG if(0) dprintf
# define SCHED_DEBUG_QUEUES if(0)
#endif 

#ifdef THREAD_DEBUG
# undef THREAD_DEBUG
# define THREAD_DEBUG   dprintf
#else
# define THREAD_DEBUG if(0) dprintf
#endif 

#ifdef VMM_DEBUG
# undef VMM_DEBUG
# define VMM_DEBUG      dprintf
#else
# define VMM_DEBUG if(0) dprintf
#endif

#ifdef XPT_DEBUG
# undef XPT_DEBUG
# define XPT_DEBUG      dprintf
#else
# define XPT_DEBUG if(0) dprintf
#endif 

#ifdef INT_DEBUG
# undef INT_DEBUG
# define INT_DEBUG      dprintf
#else
# define INT_DEBUG if(0) dprintf
#endif 

#ifdef IRQ_DEBUG
# undef IRQ_DEBUG
# define IRQ_DEBUG      dprintf
#else
# define IRQ_DEBUG if(0) dprintf
#endif 

#ifdef MP_DEBUG
# undef MP_DEBUG
# define MP_DEBUG      dprintf
#else
# define MP_DEBUG if(0) dprintf
#endif

#ifdef PAGE_DEBUG
# undef PAGE_DEBUG
# define PAGE_DEBUG   dprintf
#else
# define PAGE_DEBUG if(0) dprintf
#endif 

#ifdef KSYM_DEBUG
# undef KSYM_DEBUG
# define KSYM_DEBUG   dprintf
#else
# define KSYM_DEBUG if(0) dprintf
#endif 

#ifdef IOAPIC_DEBUG
# undef IOAPIC_DEBUG
# define IOAPIC_DEBUG   dprintf
#else
# define IOAPIC_DEBUG if(0) dprintf
#endif 

#ifdef LAPIC_DEBUG
# undef LAPIC_DEBUG
# define LAPIC_DEBUG   dprintf
#else
# define LAPIC_DEBUG if(0) dprintf
#endif

#ifdef LOCK_DEBUG
# undef LOCK_DEBUG
# define LOCK_DEBUG   dprintf
#else
# define LOCK_DEBUG if(0) dprintf
#endif 

#ifdef LOLVL_DEBUG
# undef LOLVL_DEBUG
# define LOLVL_DEBUG   dprintf
#else
# define LOLVL_DEBUG if(0) dprintf
#endif 

#ifdef PIC_DEBUG
# undef PIC_DEBUG
# define PIC_DEBUG      dprintf
#else
# define PIC_DEBUG if(0) dprintf
#endif

#ifdef SYSCALL_DEBUG
# undef SYSCALL_DEBUG
# define SYSCALL_DEBUG dprintf
#else
# define SYSCALL_DEBUG if(0) dprintf
#endif

#ifdef PERFORMANCE_DEBUG
# undef PERFORMANCE_DEBUG
# define PERFORMANCE_DEBUG dprintf
#else
# define PERFORMANCE_DEBUG if(0) dprintf
#endif

#define DEBUG_CHECKPOINT dprintf("[debug:%i] CHECKPOINT at %s:%i\n", CPU_ID, __FILE__, __LINE__)


/* debugging */
void debug_printf(const char *fmt, ...);
void debug_assert(char *exp, char *file, char *basefile, unsigned int line);
void debug_panic(const char *str);
void debug_initialise(void);
void debug_stacktrace(void);
kresult debug_lookup_symbol(unsigned int addr, char *buffer, unsigned int size, unsigned int *symbol);
kresult debug_init_sym_table(char *table, char *end);

/* assert stuff */
#ifdef DEBUG
#define assert(exp)  if (exp) ; \
else debug_assert( #exp, __FILE__, __BASE_FILE__, __LINE__ )
#else
#define assert(exp) /* do nothing */
#endif

#endif
