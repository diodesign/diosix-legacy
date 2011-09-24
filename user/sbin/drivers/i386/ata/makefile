#
# ATA device driver
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/$(ARCH)_$(ARCH_TARGET)/root

# set the build output dir
OBJSDIR = ../../../../../build/$(ARCH)_$(ARCH_TARGET)/drivers/ata

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libgloss/libnosys -I../common -I ../../common
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/main.o $(OBJSDIR)/ata.o $(OBJSDIR)/fs.o $(OBJSDIR)/lowlevel.o $(OBJSDIR)/pci.o

# targets
all: ata

# dependencies
$(OBJSDIR)/main.o:	main.c	makefile ata.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/ata.o:	ata.c	makefile ata.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/fs.o:	fs.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/lowlevel.o:	../common/lowlevel.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/pci.o:	../../common/pci.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

ata:	$(OBJS)
	$(WRITE) '==> LINK: ata'
	$(Q)$(LD) $^ -o $(OBJSDIR)/$@
	$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/ata.lst
	$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/sbin/drivers/$(ARCH)/

