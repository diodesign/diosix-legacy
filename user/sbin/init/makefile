#
# first user process 'init'
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../release/root
PATHTOMKFS = ../../../release/makeiso.sh

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../user/lib/newlib/libc/sys/diosix-i386/
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
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
	$(Q)$(LD) $^ -o $@
	$(Q)$(OBJDUMPbin) --source $@ >init.lst
	$(Q)cp init $(PATHTOROOT)/sbin/
