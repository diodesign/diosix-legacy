#
# first user process 'init'
#


.SUFFIXES: .c
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-
LIBDIOSIX = ../../lib/libdiosix
OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../release/root
PATHTOMKFS = ../../../release/makeiso.sh

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
	cp init $(PATHTOROOT)/sbin/
