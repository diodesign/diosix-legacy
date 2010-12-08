#
# first user process 'init'
#

# prettify the output
Q=@
WRITE = $(Q)echo 

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
OBJS	 	= objs/init.o

# targets
all: init

# dependencies
objs/init.o:		init.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

init:	$(OBJS)
	$(WRITE) '==> LINK: init'
	$(Q)$(LD) $^ $(LIBDIOSIX)/libdiosix.a -o $@
	$(Q)$(OBJDUMPbin) --source $@ >init.lst
	$(Q)cp init $(PATHTOROOT)/sbin/
