#
# Qemu/VirtualBox VBE driver process
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-
LIBDIOSIX = ../../../../lib/libdiosix
OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/root

# defines
FLAGS		= -O2 -std=c99 -Wall -static -nostdlib -nostdinc -fno-builtin -nodefaultlibs -I$(LIBDIOSIX)/include/
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS) -Xlinker --entry="main"
# LD		= $(PREFIX)gcc $(FLAGS) -Xlinker
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
	$(Q)$(LD) $^ $(LIBDIOSIX)/libdiosix.a -o $@
	$(Q)$(OBJDUMPbin) --source $@ >vbe.lst
	$(Q)cp vbe $(PATHTOROOT)/sbin/drivers/i386/

