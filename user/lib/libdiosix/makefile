#
# system interface library
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c
PREFIX = /Users/chris/Documents/Code/cross/i386-elf/bin/bin/i386-elf-
ARCH = i386

# defines
FLAGS		= -O2 -std=c99 -Wall -static -nostdlib -nostdinc -fno-builtin -nodefaultlibs -Iinclude
CC		= $(PREFIX)gcc $(FLAGS)
AR		= $(PREFIX)ar
OBJS	 	= objs/$(ARCH)_syscalls.o objs/io.o
COMPILE.c 	= $(CC) -c -o $@ $<

# targets
all: libdiosix.a

# dependencies
objs/$(ARCH)_syscalls.o:	arch/$(ARCH)_syscalls.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(COMPILE.c)

objs/io.o:		io.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(COMPILE.c)

# explicit rules

libdiosix.a:	$(OBJS)
		$(WRITE) '==> ARCHIVE: libdiosix.a'
		$(Q)$(AR) rcs libdiosix.a $(OBJS)


