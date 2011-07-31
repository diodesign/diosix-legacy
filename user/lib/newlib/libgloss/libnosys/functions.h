/* user/lib/newlib/libgloss/libnosys/functions.h
 * define syscall veneers for applications to talk to the microkernel
 * Author : Chris Williams
 * Date   : Sat,14 Nov 2009.17:17:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _FUNCTIONS_H
#define   _FUNCTIONS_H

#include "io.h"

/* useful defines */

/* Find the nearest upper or lower page boundary.
   DIOSIX_PAGE_ROUNDUP() returns zero if its argument
   is zero. both assume a 4K page size :( */
#define DIOSIX_PAGE_ROUNDUP(a)   ( (unsigned int)(a) ? ((((unsigned int)(a) - 1) >> 12) + 1) << 12 : 0)
#define DIOSIX_PAGE_ROUNDDOWN(a) ( (unsigned int)(a) & ((1 << 12) - 1) )

/* --------------------------------------------
   atomic locking support
   -------------------------------------------- */
extern unsigned int diosix_atomic_exchange(volatile unsigned int *ptr);
#define DIOSIX_SPINLOCK_ACQUIRE(p) { while(diosix_atomic_exchange(p)); }
#define DIOSIX_SPINLOCK_RELEASE(p) { *((volatile unsigned int *)(p)) = 0; }

/* --------------------------------------------
   veneers to syscalls
   -------------------------------------------- */

/* basic process management */
unsigned int diosix_exit(unsigned int code);
int diosix_fork(void);
unsigned int diosix_kill(unsigned int pid);

/* multitasking support */
void diosix_thread_yield(void);

/* thread management */
unsigned int diosix_thread_exit(unsigned int code);
int diosix_thread_fork(void);
unsigned int diosix_thread_kill(unsigned int tid);
unsigned int diosix_thread_sleep(unsigned int ticks);
unsigned int diosix_alarm(unsigned int ticks);

/* message sending */
unsigned int diosix_msg_send(diosix_msg_info *info);
unsigned int diosix_msg_receive(diosix_msg_info *info);
unsigned int diosix_msg_reply(diosix_msg_info *info);

/* rights and privilege layer management */
unsigned int diosix_priv_layer_up(unsigned int count);
unsigned int diosix_priv_rights_clear(unsigned int bits);
unsigned int diosix_iorights_remove(void);
unsigned int diosix_iorights_clear(unsigned int index, unsigned int bits);
unsigned int diosix_signals_unix(unsigned int mask);
unsigned int diosix_signals_kernel(unsigned int mask);

/* user and group id and related management */
unsigned int diosix_set_pg_id(unsigned int pid, unsigned int pgid);
unsigned int diosix_set_session_id(void);
unsigned int diosix_set_eid(unsigned char flag, unsigned int eid);
unsigned int diosix_set_reid(unsigned char flag, unsigned int eid, unsigned int rid);
unsigned int diosix_set_resid(unsigned char flag, unsigned eid, unsigned int rid, unsigned sid);
unsigned int diosix_set_role(unsigned int role);

/* manage drivers */
unsigned int diosix_driver_register(void);
unsigned int diosix_driver_deregister(void);
unsigned int diosix_driver_unmap_phys(diosix_phys_request *block);
unsigned int diosix_driver_map_phys(diosix_phys_request *block);
unsigned int diosix_driver_req_phys(unsigned short pages, unsigned int *addr);
unsigned int diosix_driver_ret_phys(unsigned int addr);
unsigned int diosix_driver_register_irq(unsigned char irq);
unsigned int diosix_driver_deregister_irq(unsigned char irq);
unsigned int diosix_driver_iorequest(diosix_ioport_request *req);

/* get information */
unsigned int diosix_get_thread_info(diosix_thread_info *block);
unsigned int diosix_get_process_info(diosix_process_info *block);
unsigned int diosix_get_kernel_info(diosix_kernel_info *block);
unsigned int diosix_get_kernel_stats(diosix_kernel_stats *block);

/* manage memory */
unsigned int diosix_memory_create(void *ptr, unsigned int size);
unsigned int diosix_memory_destroy(void *ptr);
unsigned int diosix_memory_resize(void *ptr, signed int change);
unsigned int diosix_memory_access(void *ptr, unsigned int bits);
unsigned int diosix_memory_locate(void **ptr, unsigned int type);

/* debugging */
unsigned int diosix_debug_write(const char *ptr);


/* --------------------------------------------
   interface with the vfs 
   -------------------------------------------- */
   
/* send requests to the vfs */
kresult diosix_vfs_new_req(diosix_msg_multipart *array,
                           diosix_vfs_req_type type,
                           diosix_vfs_request_head *head,
                           void *request, unsigned int size);
kresult diosix_vfs_send_req(unsigned int target_pid, diosix_msg_info *msg,
                            diosix_msg_multipart *parts,
                            unsigned int parts_count,
                            void *reply, unsigned int reply_size);

/* associate file handles with fs processes */
unsigned int diosix_vfs_get_fs(int filehandle);
void diosix_vfs_disassociate_handle(int filehandle);
void diosix_vfs_associate_handle(int filehandle, unsigned int pid);

/* register a filesystem or device with the vfs */
kresult diosix_vfs_register(char *path);
kresult diosix_vfs_deregister(char *path);

#endif
