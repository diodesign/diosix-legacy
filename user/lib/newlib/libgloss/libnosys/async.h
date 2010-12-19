/* user/lib/newlib/libc/sys/diosix-i386/async.h
 * Single Unix Specification's <signal.h> - http://www.opengroup.org/onlinepubs/009695399/basedefs/signal.h.html
   With diosix-specific extensions.
 * Author : Chris Williams
 * Date   : Sat,14 Nov 2009.17:17:00

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _ASYNC_H
#define   _ASYNC_H

/* UNIX-compatible signal numbers */
/* SIGZERO (0) is invalid */
#define SIGHUP    (1)
#define SIGINT    (2)
#define SIGQUIT   (3)
#define SIGILL    (4)
#define SIGTRAP   (5)
#define SIGABRT   (6)
#define SIGFPE    (8)
#define SIGKILL   (9)
#define SIGBUS    (10)  /* bus error, normally a general protection fault */
#define SIGSEGV   (11)  /* segmentation violation, normally a page fault */
#define SIGSYS    (12)  /* bad system call attempt */
#define SIGPIPE   (13)
#define SIGALRM   (14)
#define SIGTERM   (15)
#define SIGURG    (16)
#define SIGSTOP   (17)
#define SIGTSTP   (18)
#define SIGCONT   (19)
#define SIGCHLD   (20)
#define SIGTTIN   (21)
#define SIGTTOU   (22)
#define SIGXCPU   (24)
#define SIGXFSZ   (25)
#define SIGVTALRM (26)
#define SIGPROF   (27)
#define SIGUSR1   (30)
#define SIGUSR2   (31)

#define SIG_UNIX_MAX     (31)

/* diosix-specific signals for trusted processes from the kernel */
#define SIGXPROCKILLED   (32) /* process has been killed, code = victim PID */
#define SIGXPROCCLONED   (33) /* process memory map has been cloned, code = new PID */
#define SIGXPROCEXIT     (34) /* process has called SYSCALL_EXIT and must be killed, code = victim PID */
#define SIGXTHREADKILLED (35) /* a thread has been killed, code = thread owner's TID */
#define SIGXTHREADEXIT   (36) /* a thread has called SYSCALL_THREAD_EXIT, code = thread owner's PID */
#define SIGXIRQ          (37) /* an IRQ has been raised, code = IRQ line number */

/* enable diosix kernel signal number (a) */
#define SIGX_ENABLE(a)   (1 << ((unsigned int)(a) - SIG_KERNEL_MIN))

/* range of kernel signals (inclusive) */
#define SIG_KERNEL_MIN   (32)
#define SIG_KERNEL_MAX   (63)

/* signals reserved for diosix process */
#define SIG_USER_MIN     (64)

/* macros to set bits for accepted signals */
#define SIG_ACCEPT_UNIX(a)   (1 << a)
#define SIG_ACCEPT_KERNEL(a) (1 << (a - SIG_KERNEL_MIN))

#endif
