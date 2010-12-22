/* user/lib/newlib/libgloss/i386/cygmon-crt0.S
 * i386-specific start-up routines for software linked with libc
 * Author : Chris Williams
 * Date   : Tues,21 Dec 2010.22:31:00
 
 diosix-specific crt0 code:
 Copyright (c) Chris Williams and individual contributors

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

*/

/*
 * includes crt0 startup code for user programs running under Cygmon
 *
 * Copyright (c) 1998, 2000 Cygnus Support
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */

#define _S2(P,X) P ## X
#define _S1(P,X) _S2(P,X)
#define SYM(X) _S1(__USER_LABEL_PREFIX__,X)

.data
        .align  8
SYM(environ):
        .long   0

SYM(argc):
	.long	0

	.text
	.align 4

	.globl __start
   .globl _start
_start:
__start:

   /* clear the bss section */
	movl	$__bss_start, %edi
	movl	$_end, %ecx
	subl	%edi, %ecx
	xorl	%eax, %eax
	rep; stosb

   /* not implemented on diosix yet
	pushl	$SYM(__do_global_dtors)
	call	SYM(atexit)
	popl	%eax

	call	SYM(__do_global_ctors)

	pushl	$SYM(argc)
	call	SYM(__get_program_arguments)
	popl	%ecx

	movl	SYM(argc), %ecx
	pushl	%eax
	pushl	%ecx */
   
	call	SYM(main)
	/* popl	%ecx
	popl	%edx */

	/* call exit from the C library so atexit gets called, and the
	 * C++ destructors get run. This calls our exit routine below	 
	 * when it's done.
	 */
	/* pushl	%eax */
	call	SYM(exit)
3:
	jmp	3b
