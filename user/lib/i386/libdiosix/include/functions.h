/* user/lib/i386/libdiosix/include/functions.h
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

/* veneers to syscalls */

/* basic process management */
unsigned int diosix_exit(unsigned int code);
int diosix_fork(void);
unsigned int diosix_kill(unsigned int pid);

/* multitasking support */
void diosix_yield(void);

/* thread management */
unsigned int diosix_thread_exit(unsigned int code);
int diosix_thread_fork(void);
unsigned int diosix_thread_kill(unsigned int tid);

/* message sending */
unsigned int diosix_msg_send(diosix_msg_info *info);
unsigned int diosix_msg_receive(diosix_msg_info *info);
unsigned int diosix_msg_reply(diosix_msg_info *info);

#endif