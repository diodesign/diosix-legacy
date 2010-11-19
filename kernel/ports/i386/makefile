#
# diosix hyatt
#
# makefile for generic i386 port
#
#

# define the build version
VERSION		= -DKERNEL_IDENTIFIER="\"diosix-hyatt i386 SMP pre-Release1.0\"" \
			-DKERNEL_RELEASE_MAJOR=0 -DKERNEL_RELEASE_MINOR=1 \
			-DKERNEL_API_REVISION=1

# set where the compiler is
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../release/disc
PATHTOMKFS = ../../../../release/makeiso.sh

.SUFFIXES: .s .c

CCbin = $(PREFIX)gcc
LDbin = $(PREFIX)ld 
OBJDUMPbin = $(PREFIX)objdump 
STRIPbin = $(PREFIX)strip 
READELFbin = $(PREFIX)readelf

# defines
PORTDIR  = kernel/ports/i386
COREDIR  = ../../core
LIBDIOSIXDIR = ../../../user/lib/i386/libdiosix
SVNDEF := -D'SVN_REV="$(shell svnversion -n .)"'

MAKEFILE	= makefile
MAKEDEP		= $(MAKEFILE) include/portdefs.h include/processes.h include/boot.h include/cpu.h include/buses.h include/debug.h include/elf.h include/interrupts.h include/ipc.h include/locks.h include/lowlevel.h include/memory.h include/multiboot.h include/sched.h include/sglib.h include/syscalls.h

INCDIR		= include
LDSCRIPT	= diosix.ld

NASM		= nasm -f elf -i$(INCDIR)/
FLAGS		= -g -std=c99 -Wall -nostdlib -nostartfiles \
			-nostdinc -fno-builtin -nodefaultlibs \
			-fomit-frame-pointer 

# all debugging flags possible
# DEBUGFLAGS	= -DDEBUG -DMSG_DEBUG -DBUS_DEBUG -DPROC_DEBUG -DSCHED_DEBUG -DTHREAD_DEBUG -DVMM_DEBUG -DXPT_DEBUG -DINT_DEBUG -DIRQ_DEBUG -DMP_DEBUG -DPAGE_DEBUG -DKSYM_DEBUG -DIOAPIC_DEBUG -DLAPIC_DEBUG -DLOCK_DEBUG -DLOLVL_DEBUG -DPIC_DEBUG -DSYSCALL_DEBUG -DPERFORMANCE_DEBUG -DLOCK_TIME_CHECK

# basic debugging
DEBUGFLAGS	= -DDEBUG -DPROC_DEBUG -DLOCK_TIME_CHECK -DPIC_DEBUG

CC		= $(CCbin) $(SVNDEF) $(FLAGS) -I$(INCDIR) -I$(COREDIR) -I$(LIBDIOSIXDIR)/include $(DEBUGFLAGS) $(VERSION)
LD		= $(CCbin) $(FLAGS) -Xlinker --script=$(LDSCRIPT) -Xlinker


# the order of code in the kernel binary is quite important...
# try to keep often-used functions together for cache performance
OBJS	 = objs/locore.o \
	   objs/lowlevel.o objs/exceptions.o objs/irqs.o objs/syscalls.o objs/msg.o objs/sched.o \
	   objs/vmm.o objs/page.o objs/thread.o objs/proc.o objs/ioapic.o objs/pic.o objs/lapic.o objs/buses.o \
	   objs/intcontrol.o objs/mp.o objs/boot.o objs/payload.o \
	   objs/serial.o objs/debug.o


# targets
all: kernel

# standard cmd lines
COMPILE.s 	= $(NASM) -f elf $< -o $@
COMPILE.c 	= $(CC) -c -o $@ $<

# per-source rules
objs/boot.o:		$(COREDIR)/boot.c	$(MAKEDEP)
			$(COMPILE.c)

objs/payload.o:		$(COREDIR)/payload.c	$(MAKEDEP)
			$(COMPILE.c)

objs/vmm.o:		$(COREDIR)/vmm.c	$(MAKEDEP)
			$(COMPILE.c)

objs/proc.o:		$(COREDIR)/proc.c	$(MAKEDEP)
			$(COMPILE.c)

objs/sched.o:		$(COREDIR)/sched.c	$(MAKEDEP)
			$(COMPILE.c)

objs/thread.o:		$(COREDIR)/thread.c	$(MAKEDEP)
			$(COMPILE.c)

objs/msg.o:		$(COREDIR)/msg.c	$(MAKEDEP)
			$(COMPILE.c)

objs/debug.o:		debug.c			$(MAKEDEP)
			$(COMPILE.c)

objs/serial.o:		hw/serial.c		$(MAKEDEP)
			$(COMPILE.c)

objs/lapic.o:		hw/lapic.c		$(MAKEDEP)
			$(COMPILE.c)

objs/ioapic.o:		hw/ioapic.c		$(MAKEDEP)
			$(COMPILE.c)

objs/pic.o:		hw/pic.c		$(MAKEDEP)
			$(COMPILE.c)

objs/lowlevel.o:	hw/lowlevel.c		$(MAKEDEP)
			$(COMPILE.c)

objs/locore.o:		hw/locore.s		$(MAKEDEP)
			$(COMPILE.s)

objs/buses.o:		buses.c			$(MAKEDEP)
			$(COMPILE.c)

objs/page.o:		cpu/page.c		$(MAKEDEP)
			$(COMPILE.c)

objs/mp.o:		cpu/mp.c		$(MAKEDEP)
			$(COMPILE.c)

objs/intcontrol.o:	cpu/intcontrol.c	$(MAKEDEP)
			$(COMPILE.c)

objs/exceptions.o:	cpu/exceptions.c	$(MAKEDEP)
			$(COMPILE.c)

objs/irqs.o:		cpu/irqs.c	$(MAKEDEP)
			$(COMPILE.c)

objs/syscalls.o:	syscalls.c		$(MAKEDEP)
			$(COMPILE.c)

# explicit rules

kernel: $(OBJS) $(LDSCRIPT)
	$(LD) $^ -o $@
	$(OBJDUMPbin) --source $@ >diosix.lst
	echo 'KSYM' > $(PATHTOROOT)/boot/kernel.sym
	$(READELFbin) -Ws kernel >diosix.ksym
	cat diosix.ksym | awk '/FUNC/ { print $$2 " " $$3 " " $$8 }' >> $(PATHTOROOT)/boot/kernel.sym
	$(STRIPbin) -s kernel
	cp kernel $(PATHTOROOT)/boot/kernel
	sh $(PATHTOMKFS)
