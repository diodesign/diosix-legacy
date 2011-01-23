#
# ATA device driver
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/root

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libgloss/libnosys -I../common
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= objs/main.o objs/fs.o objs/lowlevel.o objs/pci.o

# targets
all: ata

# dependencies
objs/main.o:	main.c	makefile atapi.h
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

objs/fs.o:	fs.c	makefile
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

objs/lowlevel.o:	../common/lowlevel.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

objs/pci.o:		../common/pci.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

ata:	$(OBJS)
	$(WRITE) '==> LINK: ata'
	$(Q)$(LD) $^ -o $@
	$(Q)$(OBJDUMPbin) --source $@ >ata.lst
	$(Q)cp ata $(PATHTOROOT)/sbin/drivers/i386/

