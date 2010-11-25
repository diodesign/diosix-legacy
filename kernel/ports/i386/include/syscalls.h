/* kernel/ports/i386/include/syscalls.h
 * prototypes and structures for the i386 port of the kernel 
 * Author : Chris Williams
 * Date   : Mon,26 Mar 2007.23:09:39

Copyright (c) Chris Williams and individual contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

#ifndef _SYSCALLS_H
#define   _SYSCALLS_H

/* define a standard way of returning an error code */
#define SYSCALL_RETURN(a) { regs->eax = (unsigned int)(a); return; }

/* software interrupt handling */
void syscall_do_exit(int_registers_block *regs);
void syscall_do_fork(int_registers_block *regs);
void syscall_do_kill(int_registers_block *regs);
void syscall_do_yield(int_registers_block *regs);
void syscall_do_thread_exit(int_registers_block *regs);
void syscall_do_thread_fork(int_registers_block *regs);
void syscall_do_thread_kill(int_registers_block *regs);
void syscall_do_msg_send(int_registers_block *regs);
void syscall_post_msg_send(thread *sender, kresult result);
void syscall_do_msg_recv(int_registers_block *regs);
void syscall_post_msg_recv(thread *receiver, kresult result);
void syscall_do_privs(int_registers_block *regs);
void syscall_do_info(int_registers_block *regs);
void syscall_do_driver(int_registers_block *regs);

#endif
