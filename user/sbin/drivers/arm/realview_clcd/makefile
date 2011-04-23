#
# Realview CLCD graphics adapter driver process
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/$(ARCH)_$(ARCH_TARGET)/root

# set the build directory
OBJSDIR	= ../../../../../build/$(ARCH)_$(ARCH_TARGET)/drivers/realview_clcd

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libgloss/libnosys -I../common
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/main.o $(OBJSDIR)/clcd.o

# targets
all: vbe

# dependencies
$(OBJSDIR)/main.o:	main.c	makefile clcd.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/clcd.o:	clcd.c	makefile clcd.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

realview_clcd:	$(OBJS)
	$(WRITE) '==> LINK: realview_clcd'
	$(Q)$(LD) $^ -o $(OBJSDIR)/$@
	$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/realview_clcd.lst
	$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/sbin/drivers/$(ARCH)/

