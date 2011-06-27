#
# Qemu/VirtualBox/i386 rtl8139 ethernet card driver
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/$(ARCH)_$(ARCH_TARGET)/root

# set the build output dir
OBJSDIR = ../../../../../build/$(ARCH)_$(ARCH_TARGET)/drivers/rtl8139

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I. -I../../../../../user/lib/newlib/libgloss/libnosys -I../common -I../../common
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/main.o $(OBJSDIR)/pci.o $(OBJSDIR)/lowlevel.o $(OBJSDIR)/nic.o

# targets
all: rtl8139

# dependencies
$(OBJSDIR)/main.o:	main.c			makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/pci.o:	../../common/pci.c		makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/nic.o:	../../common/nic.c		makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/lowlevel.o:	../common/lowlevel.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

rtl8139:	$(OBJS)
		$(WRITE) '==> LINK: rtl8139'
		$(Q)$(LD) $^ -o $(OBJSDIR)/$@
		$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/rtl8139.lst
		$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/sbin/drivers/$(ARCH)/

