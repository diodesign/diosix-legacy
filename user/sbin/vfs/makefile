#
# virtual filesystem process 'vfs'
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
FLAGS		= -O2 -std=c99 -Wall -static -I../../lib/newlib/libgloss/libnosys
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= objs/main.o objs/vfstree.o objs/files.o

# targets
all: vfs

# dependencies
objs/main.o:		main.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

objs/vfstree.o:		vfstree.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

objs/files.o:		files.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

vfs:	$(OBJS)
	$(WRITE) '==> LINK: vfs'
	$(Q)$(LD) $^ -o $@
	$(Q)$(OBJDUMPbin) --source $@ >vfs.lst
	$(Q)cp vfs $(PATHTOROOT)/sbin/
