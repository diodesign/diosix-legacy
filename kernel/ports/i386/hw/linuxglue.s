; * kernel/ports/i386/hw/linuxglue.s
; * i386-specific glue code to boot multiboot diosix with a primitive Linux bootloader
; * Author : Chris Williams
; * Date   : Tue,24 May 2011.22:58:00
;
; Copyright (c) Chris Williams and individual contributors
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
; SOFTWARE.
;
; Contact: chris@diodesign.co.uk / http://www.diodesign.co.uk/

global _loaderglue                      ; Make entry point visible to linker.
extern _loader                          ; The multiboot loader entry point
   
; -----------------------------------------------------------------------------

section .text
align 4

; ------------------- Bootstrap processor entry point -----------------------
;  A Linux-aware bootloader (such as the one built into the JavaScript PC
;  emulator) loads us here with the following registers set.
;  => eax = physical memory size
;     ebx = physical address of the initrd image

_loaderglue:
    mov eax, 0x2BADB002
    push eax                     ; pass Multiboot magic number

    mov ebx, 0x0
    push ebx                     ; pass Multiboot info structure

    call  _loader                ; call the multiboot loader
    jmp   $                      ; halt machine should kernel return
; ---------------------- Elvis has left the building --------------------------
