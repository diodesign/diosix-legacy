#
# diosix hyatt
#
# makefile for the kernel and libdiosix
#
#

# define the build version
VERSION		= -DKERNEL_IDENTIFIER="\"diosix-hyatt i386 SMP pre-Release1.0\"" \
		  -DKERNEL_RELEASE_MAJOR=0 -DKERNEL_RELEASE_MINOR=1 \
		  -DKERNEL_API_REVISION=1

# ------------------------------------------------------------

# set the architecture and any architecture flags
ARCH = i386
ARCH_FLAGS = -m32 -march=i386

# ------------------------------------------------------------

# prettify the output
Q=@
WRITE = $(Q)echo 

# set where the root fs to store the kernel + OS os
RELEASEDIR  = release
MKFSPROGRAM = $(RELEASEDIR)/makeiso.sh
PATHTOROOT  = $(RELEASEDIR)/root

.SUFFIXES: .s .c

CCbin =      $(Q)$(PREFIX)gcc
LDbin =      $(Q)$(PREFIX)ld 
OBJDUMPbin = $(Q)$(PREFIX)objdump 
STRIPbin =   $(Q)$(PREFIX)strip 
READELFbin = $(Q)readelf
NASMbin =    $(Q)nasm

# defines
PORTDIR  = kernel/ports/$(ARCH)
COREDIR  = kernel/core
OBJSDIR  = $(PORTDIR)/objs
LIBDIOSIXDIR = user/lib/newlib/libgloss/libnosys
SVNDEF := -D'SVN_REV="$(shell svnversion -n .)"'

MAKEFILE	= makefile
MAKEDEP		= $(MAKEFILE) $(PORTDIR)/include/portdefs.h $(PORTDIR)/include/processes.h $(PORTDIR)/include/boot.h $(PORTDIR)/include/cpu.h $(PORTDIR)/include/buses.h $(PORTDIR)/include/debug.h $(PORTDIR)/include/elf.h $(PORTDIR)/include/interrupts.h $(PORTDIR)/include/ipc.h $(PORTDIR)/include/locks.h $(PORTDIR)/include/lowlevel.h $(PORTDIR)/include/memory.h $(PORTDIR)/include/multiboot.h $(PORTDIR)/include/sched.h $(PORTDIR)/include/sglib.h $(PORTDIR)/include/syscalls.h $(LIBDIOSIXDIR)/diosix.h $(LIBDIOSIXDIR)/async.h

INCDIR		= $(PORTDIR)/include
LDSCRIPT	= $(PORTDIR)/diosix.ld

NASM		= $(NASMbin) -f elf -i$(INCDIR)/
FLAGS		= -g -std=c99 -Wall -nostdlib -nostartfiles \
		  -nostdinc -fno-builtin -nodefaultlibs \
		  -fomit-frame-pointer $(ARCH_FLAGS)

# all debugging flags possible
# DEBUGFLAGS	= -DDEBUG -DMSG_DEBUG -DBUS_DEBUG -DPROC_DEBUG -DSCHED_DEBUG -DTHREAD_DEBUG -DVMM_DEBUG -DXPT_DEBUG -DINT_DEBUG -DIRQ_DEBUG -DMP_DEBUG -DPAGE_DEBUG -DKSYM_DEBUG -DIOAPIC_DEBUG -DLAPIC_DEBUG -DLOCK_DEBUG -DLOLVL_DEBUG -DPIC_DEBUG -DSYSCALL_DEBUG -DPERFORMANCE_DEBUG -DLOCK_TIME_CHECK

# basic debugging
DEBUGFLAGS	= -DDEBUG -DLOCK_TIME_CHECK -DMSG_DEBUG -DPROC_DEBUG -DSYSCALL_DEBUG

CC		= $(CCbin) $(SVNDEF) $(FLAGS) -I$(INCDIR) -I$(COREDIR) -I$(LIBDIOSIXDIR) $(DEBUGFLAGS) $(VERSION)
LD		= $(CCbin) $(FLAGS) -Xlinker --script=$(LDSCRIPT) -Xlinker


# the order of code in the kernel binary is quite important...
# try to keep often-used functions together for cache performance
OBJS	 = $(OBJSDIR)/locore.o \
	   $(OBJSDIR)/lowlevel.o $(OBJSDIR)/exceptions.o $(OBJSDIR)/irqs.o $(OBJSDIR)/msg.o $(OBJSDIR)/sched.o \
	   $(OBJSDIR)/sys_driver.o $(OBJSDIR)/sys_info.o $(OBJSDIR)/sys_memory.o $(OBJSDIR)/sys_msg.o $(OBJSDIR)/sys_privs.o \
	   $(OBJSDIR)/sys_proc.o $(OBJSDIR)/sys_thread.o \
	   $(OBJSDIR)/vmm.o $(OBJSDIR)/page.o $(OBJSDIR)/thread.o $(OBJSDIR)/proc.o $(OBJSDIR)/ioapic.o $(OBJSDIR)/pic.o \
	   $(OBJSDIR)/lapic.o $(OBJSDIR)/buses.o \
	   $(OBJSDIR)/intcontrol.o $(OBJSDIR)/mp.o $(OBJSDIR)/boot.o $(OBJSDIR)/payload.o \
	   $(OBJSDIR)/serial.o $(OBJSDIR)/debug.o


# targets
all: kernel

# standard cmd lines
COMPILE.s 	= $(NASM) -f elf $< -o $@
COMPILE.c 	= $(CC) -c -o $@ $<

# per-source rules
# the portable source
$(OBJSDIR)/boot.o:	$(COREDIR)/boot.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/payload.o:	$(COREDIR)/payload.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/vmm.o:	$(COREDIR)/vmm.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/proc.o:	$(COREDIR)/proc.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/sched.o:	$(COREDIR)/sched.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/thread.o:	$(COREDIR)/thread.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/msg.o:	$(COREDIR)/msg.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/debug.o:	$(PORTDIR)/debug.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

# the hardware-specific source
$(OBJSDIR)/serial.o:	$(PORTDIR)/hw/serial.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/lapic.o:	$(PORTDIR)/hw/lapic.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/ioapic.o:	$(PORTDIR)/hw/ioapic.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/pic.o:	$(PORTDIR)/hw/pic.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/lowlevel.o:	$(PORTDIR)/hw/lowlevel.c	$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/locore.o:	$(PORTDIR)/hw/locore.s		$(MAKEDEP)
			$(WRITE) '==> ASSEMBLE: $<'
			$(COMPILE.s)

$(OBJSDIR)/buses.o:	$(PORTDIR)/buses.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/page.o:	$(PORTDIR)/cpu/page.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/mp.o:	$(PORTDIR)/cpu/mp.c		$(MAKEDEP)
			$(WRITE) '==> COMPILE: $<'
			$(COMPILE.c)

$(OBJSDIR)/intcontrol.o:	$(PORTDIR)/cpu/intcontrol.c		$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/exceptions.o:	$(PORTDIR)/cpu/exceptions.c		$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/irqs.o:		$(PORTDIR)/cpu/irqs.c			$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

# the syscall interfaces
$(OBJSDIR)/sys_driver.o:	$(PORTDIR)/syscalls/sys_driver.c	$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/sys_info.o:		$(PORTDIR)/syscalls/sys_info.c		$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/sys_memory.o:	$(PORTDIR)/syscalls/sys_memory.c	$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/sys_msg.o:		$(PORTDIR)/syscalls/sys_msg.c		$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/sys_privs.o:		$(PORTDIR)/syscalls/sys_privs.c		$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/sys_proc.o:		$(PORTDIR)/syscalls/sys_proc.c		$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)

$(OBJSDIR)/sys_thread.o:	$(PORTDIR)/syscalls/sys_thread.c	$(MAKEDEP)
				$(WRITE) '==> COMPILE: $<'
				$(COMPILE.c)				

# explicit rules

# link the kernel
kernel: $(OBJS) $(LDSCRIPT)
	$(WRITE) '[+] linking kernel'
	$(LD) $^ -o $(PORTDIR)/$@
# produce files to boot the system with the kernel
	$(WRITE) '[+] generating files for boot'
	$(OBJDUMPbin) --source $(PORTDIR)/$@ >$(PORTDIR)/kernel.lst
	$(Q)echo 'KSYM' > $(PATHTOROOT)/boot/kernel.sym
	$(READELFbin) -Ws $(PORTDIR)/$@ >$(PORTDIR)/kernel.ksym
	$(Q)cat $(PORTDIR)/kernel.ksym | awk '/FUNC/ { print $$2 " " $$3 " " $$8 }' >> $(PATHTOROOT)/boot/kernel.sym
	$(STRIPbin) -s $(PORTDIR)/$@
	$(Q)cp $(PORTDIR)/$@ $(PATHTOROOT)/boot/kernel
# build the FS
	$(WRITE) '[+] building arch-specific drivers'
	$(Q)make -C user/sbin/drivers/$(ARCH)
	$(WRITE) '[+] building system executive (/sbin/init)'
	$(Q)make -C user/sbin/init
	$(WRITE) '[+] building virtual filesystem manager (/sbin/vfs)'
	$(Q)make -C user/sbin/vfs
	$(WRITE) '[+] building filesystem'
	$(Q)$(MKFSPROGRAM)
