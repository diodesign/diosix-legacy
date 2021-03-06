; * kernel/ports/i386/hw/locore.s
; * i386-specific low-level startup and general purpose routines
; * Author : Chris Williams and code from OsFaqWiki higher half barebones
; * Date   : Mon,26 Mar 2007.23:09:39
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

global _loader                           ; Make entry point visible to linker.
global KernelPageDirectory               ; Expose this structure to the upper kernel
global KernelPageDirectoryVirtStart
global KernelBootStackBase
global KernelGDT
global KernelGDTEnd
global KernelGDTPtr
global TSS_Selector
global APStack
global KernelFPPresent
global KernelSIMDPresent
global KernelFPConfig
extern _main                             ; _main is defined elsewhere
extern _mp_catch_ap
extern exception_handler                 ; common exception handler
extern irq_handler                       ; common irq handler

; setting up the Multiboot header - see GRUB docs for details
MODULEALIGN equ  1<<0                    ; align loaded modules on page boundaries
MEMINFO     equ  1<<1                    ; provide memory map
FLAGS       equ  MODULEALIGN | MEMINFO   ; this is the Multiboot 'flag' field
MAGIC       equ  0x1BADB002              ; 'magic number' lets bootloader find the header
CHECKSUM    equ -(MAGIC + FLAGS)         ; checksum required

ATAGBASE    equ 0x500                    ; generate ATAG list at this phys addr
LOADERMAGIC equ 0x2BADB002               ; magic number used by the loader (see mutliboot.h)

CR0_TS      equ (1 << 3)                 ; FP task switches, cleared when FP is used
CR0_EM      equ (1 << 2)                 ; Route all FP through undef instr exception
CR0_MP      equ (1 << 1)                 ; set to allow fwait to side-step the TS bit
CR0FPBITS   equ (-1) - (CR0_TS + CR0_EM) ; clear TS and EM in CR0

CR4_OSFXSR     equ (1 << 9)              ; enable SSE support with FXSAVE and FXRSTOR
CR4_OSXMMEXCPT equ (1 << 10)             ; enable XF exceptions

; SIMD presence bits in edx/ecx returned from CPUID func 1
CPUID_MMX   equ (1 << 23) ; in edx
CPUID_SSE   equ (1 << 25) ; in edx
CPUID_SSE2  equ (1 << 26) ; in edx
CPUID_SSE3  equ (1 << 0)  ; in ecx

; This is the virtual base address of kernel space. It must be used to
; convert virtual addresses into physical addresses until paging is enabled.
; Note that this is not the virtual address where the kernel image itself is
; loaded -- just the amount that must be subtracted from a virtual address to
; get a physical address.
KERNEL_VIRTUAL_BASE   equ 0xC0000000      ; 3GB high water mark
KERNEL_PHYSICAL_BASE  equ 0x00400000      ; 4M mark
; Page directory index of kernel's 4MB PTE.
KERNEL_PAGE_NUMBER    equ (KERNEL_VIRTUAL_BASE >> 22)
; Page directory index of the per-cpu-LAPIC + per-system-IOAPIC's 4MB PTE
PIC_BASE              equ 0xFEC00000
PIC_PAGE_NUMBER       equ (PIC_BASE >> 22)

; reserve initial kernel stack space - 8K should be adequate for the bootstrap cpu
STACKSIZE   equ 0x2000

section .data
align 4096
KernelPageDirectory:
%ifdef ARCH_NO4MPAGES
   ; use this structure on processors that only support 4K pages...
   ; This page directory entry identity-maps the first 16MB of the 32-bit
   ; physical address space into the kernel's virtual address space by
   ; using four page tables that each map in 4M of memory.
   ; All control bits clear except:
   ; bit 1: RW The kernel page is read/write.
   ; bit 0: P  The kernel page is present.

   dd KernelPageTable0 - KERNEL_VIRTUAL_BASE + 3
   dd KernelPageTable1 - KERNEL_VIRTUAL_BASE + 3
   dd KernelPageTable2 - KERNEL_VIRTUAL_BASE + 3
   dd KernelPageTable3 - KERNEL_VIRTUAL_BASE + 3 
   times (KERNEL_PAGE_NUMBER - 4) dd 0 ; Pages before kernel space.
KernelPageDirectoryVirtStart:
   dd KernelPageTable0 - KERNEL_VIRTUAL_BASE + 3
   dd KernelPageTable1 - KERNEL_VIRTUAL_BASE + 3
   dd KernelPageTable2 - KERNEL_VIRTUAL_BASE + 3
   dd KernelPageTable3 - KERNEL_VIRTUAL_BASE + 3
   times (1024 - KERNEL_PAGE_NUMBER - 4) dd 0  ; Pages to end of address space

; build a set of page tables mapping in the first 12M of physical memory
; 4K at a time, with the P and RW bits set, usermode bit clear
align 4096
KernelPageTable0:
%assign i 0
%rep 1024
   dd i+3
%assign i i+4096
%endrep

align 4096
KernelPageTable1:
%rep 1024
   dd i+3
%assign i i+4096
%endrep

align 4096
KernelPageTable2:
%rep 1024
   dd i+3
%assign i i+4096
%endrep

align 4096
KernelPageTable3:
%rep 1024
dd i+3
%assign i i+4096
%endrep


%else
   ; use this structure on processors that support 4M pages...
   ; This page directory entry identity-maps the first 3x4MB pages of the 32-bit
   ; physical address space and the 4MB page containing the APIC registers.
    
   ; All bits are clear except the following:
   ; bit 7: PS The kernel page is 4MB.
   ; bit 1: RW The kernel page is read/write.
   ; bit 0: P  The kernel page is present.
    
   ; NOTE: It's not generally a good idea to have the first 4M of physical
   ; space tied up in one CPU page for caching reasons. So redo this in 4K
   ; blocks as soon as possible.
    
   dd 0x00000083
   dd 0x00400083
   dd 0x00800083
   times (KERNEL_PAGE_NUMBER - 3) dd 0 ; Pages before kernel space.
KernelPageDirectoryVirtStart:
   ; This page directory entry defines the first 3x4MB pages in kernel space.
   dd 0x00000083
   dd 0x00400083
   dd 0x00800083
   times (PIC_PAGE_NUMBER - KERNEL_PAGE_NUMBER - 3) dd 0  ; Pages after the kernel image to the PIC area
   ; This page directory entry defines a 4MB page containing the memory-mapped APIC architecture.
   ; NB: cache disable and write-through bits (4 & 3 respectively) are set
   dd 0xFEC0009B
   times (1024 - PIC_PAGE_NUMBER - 1) dd 0                ; Pages to the end of address space
%endif

   ; the diosix GDT
align 16
gdt:
KernelGDT:
   ; NULL descriptor
   dw 0      ; limit 15:0
   dw 0      ; base 15:0
   db 0      ; base 23:16
   db 0      ; type
   db 0      ; limit 19:16, flags
   db 0      ; base 31:24

   ; unused descriptor - useful for forcing a fault
   dw 0
   dw 0
   db 0
   db 0
   db 0
   db 0

KERNEL_DATA_SEL   equ   $-gdt
   dw 0FFFFh
   dw 0
   db 0
   db 92h      ; present, ring 0, data, expand-up, writable
   db 0CFh      ; page-granular (4 gig limit), 32-bit
   db 0

KERNEL_CODE_SEL   equ   $-gdt
   dw 0FFFFh
   dw 0
   db 0
   db 9Ah      ; present,ring 0,code, non-conforming, readable
   db 0CFh      ; page-granular (4 gig limit), 32-bit
   db 0

USER_DATA_SEL   equ   $-gdt
   dw 0FFFFh
   dw 0
   db 0
   db 0F2h      ; present, ring 3, data, expand-up, writable
   db 0CFh      ; page-granular (4 gig limit), 32-bit
   db 0

USER_CODE_SEL   equ   $-gdt
   dw 0FFFFh
   dw 0
   db 0
   db 0FAh      ; present, ring 3, code, non-conforming, readable
   db 0CFh      ; page-granular (4 gig limit), 32-bit
   db 0
   
; segment to describe boot cpu's tss
TSS_Selector:
   dw 0         
   dw 0
   db 0
   db 0
   db 0
   db 0
gdt_end:
KernelGDTEnd:

KernelGDTPtr:
gdt_ptr:
   dw gdt_end - gdt - 1
   dd gdt

align 16
KernelFPConfig:
   dw 037ah      ; default FNINIT value with division by zero and invalid operands enabled
KernelFPPresent:
   dw 0F0F0h     ; arbitrary garbage, this short is zero'd when an FPU is detected by the kernel
KernelSIMDPresent:
   db 0          ; 0 = vanilla FPU, 1 = MMX, 2 = SSE, 3 = SSE2, 4 = SSE3 detected by the kernel

align 4
APStack:
   dd 0
   
; -----------------------------------------------------------------------------

section .text
align 4
MultiBootHeader:
%ifndef ARCH_NOMULTIBOOT
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
%endif

; ------------------- Bootstrap processor entry point -----------------------
; if loaded by a multiboot loader:
;    => eax = multiboot magic number, ebx = phys addr of multiboot data
;
; if loaded by a Linux loader:
;    => eax = amount of RAM fitted in bytes, ebx = initrd size
; 
; the default is multiboot loader, define ARCH_NOMULTIBOOT if kernel is to
; be loaded by a Linux loader. we should be in protected mode with the
; bootlaoder's GDT loaded by this point
bits 32
_loader:
; NOTE: Until paging is set up, the code must be position-independent
; and use physical addresses, not virtual ones.
protectedmode:
   mov ecx, (KernelPageDirectory - KERNEL_VIRTUAL_BASE)
   mov cr3, ecx          ; Load page directory base register.
    
   mov ecx, cr4
   or ecx, 0x00000010    ; Set PSE bit in CR4 to enable 4MB pages.
   mov cr4, ecx
   
   mov ecx, cr0
   or ecx, 0x80000000    ; Set PG bit in CR0 to enable paging.
   mov cr0, ecx

   ; Start fetching instructions in kernel space.
   lea ecx, [StartInHigherHalf]
   jmp ecx  ; NOTE: Must be absolute jump!

StartInHigherHalf:
   ; stop using bootloader GDT, and load our GDT. this should be all the GDT
   ; we need from now on, descriptors for usr and knl modes.
   lgdt [gdt_ptr]

   mov dx, KERNEL_DATA_SEL
   mov ds, dx
   mov es, dx
   mov ss, dx
   mov fs, dx
   mov gs, dx
   jmp KERNEL_CODE_SEL:FixupStack

FixupStack:
   ; NOTE: From now on, paging should be enabled. The first 12MB of physical
   ; address space is mapped starting at KERNEL_VIRTUAL_BASE. Everything is
   ; linked to this address, so no more position-independent code or funny
   ; business with virtual-to-physical address translation should be
   ; necessary. We now have a higher-half kernel.
   mov esp, stack+STACKSIZE           ; set up the stack

%ifdef ARCH_NOMULTIBOOT
   extern atag_build_list
   extern atag_process

   ; if we have no multiboot structure then it's easier to craft a simple
   ; ATAG list and then use a core routine to convert this into multiboot.
   ; call atag_build_list(RAM fitted in bytes, initrd location)
   ; and then pass the list to the multiboot structure generator
   mov ecx, ATAGBASE + KERNEL_VIRTUAL_BASE
   push ecx
   push ebx
   push eax
   call atag_build_list
   sub esp, 12

   push ATAGBASE + KERNEL_VIRTUAL_BASE
   call atag_process
   sub esp, 4

   ; save the multiboot structure pointer into ebx, put the right magic into
   ; eax and we're now in a good known state to enter the microkernel proper
   mov ebx, eax
   mov eax, LOADERMAGIC
%endif

; --------------------------------------------------------------------------------------------------
   ; detect FPU - cribbed from http://wiki.osdev.org/FPU
   ; preserve eax and ebx registers
   push eax
   push ebx

   mov edx, cr0
   and edx, CR0FPBITS                      ; clear TS and EM to force fpu access
   mov cr0, edx
   fninit                                  ; load defaults to fpu
   fnstsw [KernelFPPresent]                ; store status word
   cmp word [KernelFPPresent], 0           ; compare the written status with the expected fpu state
   jne SkipFPInit                          ; no FPU installed??

   ; set the MP bit in CR0
   or edx, CR0_MP
   mov cr0, edx
   ; enable exceptions for division by zero and invalid operands
   fldcw [KernelFPConfig]  

   ; detect the presence of SSE
   mov eax, 00000001                       ; read core features
   cpuid                                   ; we're only interested in edx and ecx
   ; test for MMX (stored in edx)
   mov eax, edx
   and eax, CPUID_MMX
   cmp eax, 0
   jz SkipFPInit
   mov byte [KernelSIMDPresent], 1         ; 1 = our internal representation for MMX
   ; test for SSE (stored in edx)
   mov eax, edx
   and eax, CPUID_SSE
   cmp eax, 0
   jz SkipFPInit
   mov byte [KernelSIMDPresent], 2         ; 2 = our internal representation for SSE1

   ; enable SSE in the control register 4
   mov eax, cr4
   or eax, (CR4_OSFXSR + CR4_OSXMMEXCPT)   ; enable SSE, FP save/restore and XF exceptions
   mov cr4, eax 

   ; test for SSE2 (stored in edx)
   mov eax, edx
   and eax, CPUID_SSE2
   cmp eax, 0
   jz SkipFPInit
   mov byte [KernelSIMDPresent], 3         ; 3 = our internal representation for SSE2
   ; test for SSE3 (stored in ecx)
   mov eax, ecx
   and eax, CPUID_SSE3
   cmp eax, 0
   jz SkipFPInit
   mov byte [KernelSIMDPresent], 4         ; 4 = our internal representation for SSE3

   ; restore eax and ebx
   SkipFPInit:
   pop ebx
   pop eax
; --------------------------------------------------------------------------------------------------


   push eax                           ; pass Multiboot magic number

   ; pass Multiboot info structure -- WARNING: This is a physical address
   ; and may not be in the first 12MB!
   push ebx

   call  _main                  ; call kernel proper
   jmp   $                      ; halt machine should kernel return
; ---------------------- Elvis has left the building --------------------------

; ------------------- Application processor entry point -----------------------
; --------------------- (executed at 0x80000 physical) ------------------------
[global x86_start_ap]
align 4
bits 16
x86_start_ap:
   ; NOTE: we're running in god-awful real mode too, so we need to break
   ; out of that asap. we've copied the basic kernel gdtpr, gdt and page
   ; directory into the pages above the trampoline code too (at 0x81000,
   ; 0x82000 and 0x83000 respectively)
   cli                    ; ensure interrupts are off
   mov eax, 0x81000
   lgdt [eax]             ; load the low-memory GDT ptr

   mov eax, cr0
   or eax, 0x1
   mov cr0, eax           ; enable protected mode and jmp into 32bit mode

   jmp dword KERNEL_CODE_SEL:((APEnterPMode - x86_start_ap) + 0x80000)
        
bits 32
APEnterPMode:
   mov ecx, 0x83000       ; pointer to the base of the page directory
   mov [ecx], dword 0x83  ; ensure the lowest 4MB is identity mapped in
   mov cr3, ecx           ; load page directory base register.
    
   mov ecx, cr4
   or ecx, 0x00000010     ; Set PSE bit in CR4 to enable 4MB pages.
   mov cr4, ecx

   mov ecx, cr0
   or ecx, 0x80000000     ; Set PG bit in CR0 to enable paging.
   mov cr0, ecx

   ; Start fetching instructions in higher kernel space.
   lea ecx, [APMoveToHigherHalf]
   jmp ecx  ; NOTE: Must be absolute jump!

APMoveToHigherHalf:
   ; switch to the GDT table in the higher-half of the memory map
   mov eax, gdt_ptr
   lgdt [eax]
    
   ; get the right segment selectors
   mov ax, KERNEL_DATA_SEL
   mov ds, ax
   mov es, ax
   mov ss, ax
   mov fs, ax
   mov gs, ax
   jmp KERNEL_CODE_SEL:APFixupStack
    
APFixupStack:
   mov esp, [APStack]   ; set up the stack
   call  _mp_catch_ap  ; bounce into the kernel
   jmp   $             ; halt should this processor return

[global x86_start_ap_end]
x86_start_ap_end:
; ------------------- We came, we saw, we kicked its ass ------------------------

; ---------------------- INTERRUPT TABLES AND HANDLERS --------------------------
; hat-tip: www.jamesmolloy.co.uk/tutorial_html/

; cpu exception entry point - with no error code
%macro ISR_NOERRCODE 1  ; define a macro, taking one parameter
  [global isr%1]        ; %1 accesses the first parameter.
  isr%1:
    push byte 0         ; stack a dummy error code
    push byte %1        ; stack the interrupt number
    jmp int_enter_knl   ; begin to connect asm to C world
%endmacro

; cpu exception entry point - with error code
%macro ISR_ERRCODE 1
  [global isr%1]
  isr%1:
    push byte %1        ; stack int number, error code already stacked
    jmp int_enter_knl   ; begin to connect asm to C world
%endmacro

; device interrupt request entry point
%macro IRQ 2            ; two parameters: IRQ number and ISR number
  [global irq%1]
  irq%1:
    push byte 0         ; stack a zero
    push byte %2        ; stack the interrupt number
    jmp irq_enter_knl   ; begin to connect asm to C world
%endmacro

; cpu exception handlers -- comments for the implemented exceptions
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14    ; Page fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18  
ISR_NOERRCODE 19    ; reserved (19 - 31)
ISR_NOERRCODE 20    ; .
ISR_NOERRCODE 21    ; .
ISR_NOERRCODE 22    ; .
ISR_NOERRCODE 23    ; .
ISR_NOERRCODE 24    ; .
ISR_NOERRCODE 25    ; .
ISR_NOERRCODE 26    ; .
ISR_NOERRCODE 27    ; .
ISR_NOERRCODE 28    ; .
ISR_NOERRCODE 29    ; .
ISR_NOERRCODE 30    ; .
ISR_NOERRCODE 31    ; .

; basic device interrupts
; format is: hardware_irq_number, kernel_irq_number
IRQ        0, 32    ; 100Hz PIT for the scheduler
IRQ        1, 33    ; ps/2-compatible keyboard interrupt
IRQ        2, 34
IRQ        3, 35
IRQ        4, 36
IRQ        5, 37
IRQ        6, 38
IRQ        7, 39
IRQ        8, 40
IRQ        9, 41
IRQ       10, 42
IRQ       11, 43
IRQ       12, 44
IRQ       13, 45
IRQ       14, 46
IRQ       15, 47

; APIC interrupt handlers
IRQ       16, 48  ; TIMER 
IRQ       17, 49  ; LINT0
IRQ       18, 50  ; LINT1
IRQ       19, 51  ; PCINT
IRQ       20, 63  ; SPURIOUS ; bits 0-3 must be set
IRQ       21, 53  ; THERMAL
IRQ       22, 54  ; ERROR
; IRQ     xx, 63  ; taken by SPURIOUS

; IOAPIC's 24 interrupt handlers (0 to 23)
IRQ       23, 64
IRQ       24, 65   
IRQ       25, 66   ; 100Hz PIT for the scheduler
IRQ       26, 67
IRQ       27, 68
IRQ       28, 69
IRQ       29, 70
IRQ       30, 71
IRQ       31, 72
IRQ       32, 73
IRQ       33, 74
IRQ       34, 75
IRQ       35, 76
IRQ       36, 77
IRQ       37, 78
IRQ       38, 79
IRQ       39, 80
IRQ       40, 81
IRQ       41, 82
IRQ       42, 83
IRQ       43, 84
IRQ       44, 85
IRQ       45, 86
IRQ       46, 87

; inter-processor interrupts (142-143)
ISR_NOERRCODE   142
ISR_NOERRCODE   143

; diosix SWI handler at 0x90 (144)
ISR_NOERRCODE   144


; cpu exception handler stub
; routine to jump from this crude asm world into our lovely C kernel
int_enter_knl:
   pusha                    ; stacks edi, esi, ebp, esp, ebx, edx, ecx, eax
   mov eax, ds              ; lower 16-bits of eax = ds
   push eax                 ; stacks the data segment descriptor

   mov ax, 0x10             ; kernel data segment is 3rd GDT entry
   mov ds, ax               ;   thus: (3 - 1) * sizeof(GDT entry)
   mov es, ax               ;         = 2 * 8 = 16 = 0x10
   mov fs, ax               ;
   mov gs, ax               ;   so set up the correct segment

   call exception_handler   ; bounce into the kernel, all regs preserved on exit

   pop eax                  ; restore the original data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa                     ; restore edi, esi, ebp et al
   add esp, 8               ; fix-up the stacked error code and intr number
   iret                     ; restore CS, EIP, EFLAGS, SS, and ESP
   
; device interrupt handler stub
; routine to jump from this crude asm world into our lovely C kernel
irq_enter_knl:
   pusha                    ; stacks edi, esi, ebp, esp, ebx, edx, ecx, eax
   mov eax, ds              ; lower 16-bits of eax = ds
   push eax                 ; stacks the data segment descriptor

   mov ax, 0x10             ; kernel data segment is 3rd GDT entry
   mov ds, ax               ;   thus: (3 - 1) * sizeof(GDT entry)
   mov es, ax               ;         = 2 * 8 = 16 = 0x10
   mov fs, ax               ;
   mov gs, ax               ;   so set up the correct segment
   
   call irq_handler         ; bounce into the kernel, all regs preserved on exit

   pop eax                  ; restore the original data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa                     ; restore edi, esi, ebp et al
   add esp, 8               ; fix-up the stacked error code and intr number
   iret                     ; restore CS, EIP, EFLAGS, SS, and ESP

; ---------------- assembler to load the lidt ---------------------------------
[global x86_load_idtr]     ; what it says on the tin
x86_load_idtr:
   mov eax, [esp+4]        ; get the pointer to the idt from the stack 
   lidt [eax]              ; load the idt pointer
   ret                     ; and bounce back to the caller
   
; ---------------- assembler to load the task state register ------------------
[global x86_load_tss]
x86_load_tss:
   mov ax, 0x33  ; load the index of our TSS structure - the index is
                 ; 0x30, as it is the 7th selector and each is 8 bytes
                 ; long, but we set the bottom two bits (making 0x33)
                 ; so that it has an RPL of 3, not zero.
   ltr ax        ; move 0x33 into the byte-wide task state register
   ret
   
; ---------------- assembler to reload the gdt --------------------------------
[global x86_load_gdtr]
x86_load_gdtr:
   mov  eax, [esp+4]  ; get the pointer to the gdtptr from the stack
   lgdt [eax]         ; fixed address of gdt table
   ret

; ---------------- assembler to atomically test and set -----------------------
[global x86_test_and_set]
x86_test_and_set:
    mov eax, [esp+4]  ; copy the set value into eax
    mov edx, [esp+8]  ; copy the lock pointer into edx
    lock              ; lock the memory bus for the next instruction
    xchg [edx], eax   ; swap eax with what is stored in [edx]
    ret               ; return the old value that's in eax 

; ---------------- assembler to put this CPU to sleep -------------------------
; going from ring3->ring0 stacks an error code, eip, cs, eflags, esp and ss
; plus ds, edi, esi, ebp, esp, ebx, edx, ecx, eax and an interrupt/exception code
; but this code is running in ring0 and ring0-ring0 stacks ds, edi, esi, ebp,
; esp, ebx, edx, ecx, eax, an error code, eip, cs, eflags - ie: r0->r0 doesn't
; stack the usermode esp and ss. so flatten the stack but leave 2 x 4 bytes
[global x86_cpu_sleep]
x86_cpu_sleep:
   mov eax, [esp+4]   ; get the stack ptr immediately after last interrupt
   add eax, 14 * 4    ; skip over the 9 registers and irq/exception number stacked
                      ; by the interrupt/exception handler and the error code, eip,
                      ; cs and eflags stacked by the processor
   mov esp, eax       ; update the stack
   mov ecx, eax
   sti                ; enable interrupts
x86_cpu_halt:
   hlt                ; sleep
   jmp x86_cpu_halt   ; do not return

; ---------------- filthy dump of hex to COM1 -----------------
x86_dump_hex_and_halt:
; => ecx = value to write - will halt after writing
   mov ebx, 8
   mov dx, 0x3f8
x86_dump_hex_and_halt_hexloop:
   mov eax, ecx
   shr eax, 28
   and eax, 0xf
   cmp eax, 9
   jle x86_dump_hex_and_halt_isanumber
   add eax, 0x41 - 10     ; character code for 'A'
   jmp x86_dump_hex_and_halt_hexoutput
x86_dump_hex_and_halt_isanumber:
   add eax, 0x30          ; character code for '0'
x86_dump_hex_and_halt_hexoutput:
   out dx, al
   shl ecx, 4
   sub ebx, 1
   cmp ebx, 0
   jne x86_dump_hex_and_halt_hexloop
   hlt
   jmp $

x86_print_hex:
; => ecx = value to write plus a trailing space
   push eax
   push ebx
   push edx

   mov ebx, 8
   mov dx, 0x3f8
x86_print_hex_hexloop:
   mov eax, ecx
   shr eax, 28
   and eax, 0xf
   cmp eax, 9
   jle x86_print_hex_isanumber
   add eax, 0x41 - 10     ; character code for 'A'
   jmp x86_print_hex_hexoutput
x86_print_hex_isanumber:
   add eax, 0x30          ; character code for '0'
x86_print_hex_hexoutput:
   out dx, al
   shl ecx, 4
   sub ebx, 1
   cmp ebx, 0
   jne x86_print_hex_hexloop

   mov al, 32             ; character code for ' '
   out dx, al             ; print trailing space

   pop edx
   pop ebx
   pop eax
   ret

; ------------------------- stack space ---------------------------------------
section .bss
align 32
stack:
    resb STACKSIZE      ; reserve stack on a quadword boundary
KernelBootStackBase:
