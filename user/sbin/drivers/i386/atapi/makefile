#
# ATAPI device driver
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
OBJS	 	= objs/main.o objs/lowlevel.o objs/pci.o

# targets
all: atapi

# dependencies
objs/main.o:	main.c	makefile atapi.h
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

objs/lowlevel.o:	../common/lowlevel.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

objs/pci.o:		../common/pci.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

atapi:	$(OBJS)
	$(WRITE) '==> LINK: atapi'
	$(Q)$(LD) $^ -o $@
	$(Q)$(OBJDUMPbin) --source $@ >atapi.lst
	$(Q)cp atapi $(PATHTOROOT)/sbin/drivers/i386/

