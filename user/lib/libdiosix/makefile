#
# system interface library
#


.SUFFIXES: .c
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-
ARCH = i386

# defines
FLAGS		= -O2 -std=c99 -Wall -static -nostdlib -nostdinc -fno-builtin -nodefaultlibs -Iinclude
CC		= $(PREFIX)gcc $(FLAGS)
AR		= $(PREFIX)ar
OBJS	 	= objs/$(ARCH)_syscalls.o
COMPILE.c 	= $(CC) -c -o $@ $<

# targets
all: libdiosix.a

# dependencies
objs/$(ARCH)_syscalls.o:	arch/$(ARCH)_syscalls.c	makefile
				$(COMPILE.c)

# explicit rules

libdiosix.a:	$(OBJS)
		$(AR) rcs libdiosix.a $(OBJS)


