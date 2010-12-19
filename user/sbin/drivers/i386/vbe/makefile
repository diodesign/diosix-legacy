#
# Qemu/VirtualBox VBE driver process
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/root

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libc/sys/diosix-i386/
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= objs/main.o objs/vbe.o objs/lowlevel.o

# targets
all: vbe

# dependencies
objs/main.o:	main.c	makefile vbe.h
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

objs/lowlevel.o:	lowlevel.c	makefile vbe.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

objs/vbe.o:	vbe.c	makefile vbe.h
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

# explicit rules

vbe:	$(OBJS)
	$(WRITE) '==> LINK: vbe'
	$(Q)$(LD) $^ -o $@
	$(Q)$(OBJDUMPbin) --source $@ >vbe.lst
	$(Q)cp vbe $(PATHTOROOT)/sbin/drivers/i386/

