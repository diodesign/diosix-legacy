#
# PCI device manager
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/root

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libgloss/libnosys -I../common/
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= objs/main.o objs/lowlevel.o

# targets
all: pcimngr

# dependencies
objs/main.o:	main.c	makefile
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

objs/lowlevel.o:	../common/lowlevel.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

pcimngr:	$(OBJS)
		$(WRITE) '==> LINK: pcimngr'
		$(Q)$(LD) $^ -o $@
		$(Q)$(OBJDUMPbin) --source $@ > pcimngr.lst
		$(Q)cp pcimngr $(PATHTOROOT)/sbin/drivers/i386/

