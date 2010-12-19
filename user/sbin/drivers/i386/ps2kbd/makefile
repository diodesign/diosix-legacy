#
# Generic ps2 keyboard driver process
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/root

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libgloss/libnosys
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= objs/main.o objs/lowlevel.o

# targets
all: ps2kbd

# dependencies
objs/main.o:	main.c	makefile ps2kbd.h
		$(WRITE) '==> COMPILE: $<'
		$(Q)$(CC) -c -o $@ $<

objs/lowlevel.o:	lowlevel.c	makefile ps2kbd.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<


# explicit rules

ps2kbd:	$(OBJS)
	$(WRITE) '==> LINK: ps2kbd'
	$(Q)$(LD) $^ -o $@
	$(Q)$(OBJDUMPbin) --source $@ >ps2kbd.lst
	$(Q)cp ps2kbd $(PATHTOROOT)/sbin/drivers/i386/

