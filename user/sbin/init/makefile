#
# first user process 'init'
#


.SUFFIXES: .c
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-
LIBDIOSIX = ../../lib/i386/libdiosix
OBJDUMPbin = $(PREFIX)objdump 

# defines
FLAGS		= -O2 -std=c99 -Wall -static -nostdlib -nostdinc -fno-builtin -nodefaultlibs -I$(LIBDIOSIX)/include/
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS) -Xlinker --entry="main"
# LD		= $(PREFIX)gcc $(FLAGS) -Xlinker
OBJS	 	= init.o

# targets
all: init

# implicit rules
.c.o:
	$(CC) -c -o $@ $<

# dependencies
init.o:		init.c	makefile

# explicit rules

init:	$(OBJS)
	$(LD) $^ $(LIBDIOSIX)/libdiosix.a -o $@
	$(OBJDUMPbin) --source $@ >init.lst
	cp init /Volumes/GRUB/sbin/

