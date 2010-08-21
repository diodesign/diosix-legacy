#
# system interface library
#


.SUFFIXES: .c
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-

# defines
FLAGS		= -O2 -std=c99 -Wall -static -nostdlib -nostdinc -fno-builtin -nodefaultlibs -Iinclude
CC		= $(PREFIX)gcc $(FLAGS)
AR		= $(PREFIX)ar
OBJS	 	= syscalls.o

# targets
all: libdiosix.a

# implicit rules
.c.o:
	$(CC) -c -o $@ $<

# dependencies
syscalls.o:	syscalls.c	makefile

# explicit rules

libdiosix.a:	$(OBJS)
		$(AR) rcs libdiosix.a $(OBJS)


