#
# Generic VGA text mode process
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../../../release/$(ARCH)_$(ARCH_TARGET)/root

# set the build output dir
OBJSDIR = ../../../../../build/$(ARCH)_$(ARCH_TARGET)/drivers/textconsole

# defines
FLAGS		= -g -O2 -std=c99 -Wall -static -I../../../../../user/lib/newlib/libgloss/libnosys -I../common
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/main.o $(OBJSDIR)/lowlevel.o

# targets
all: textconsole

# dependencies
$(OBJSDIR)/main.o:	main.c	makefile textconsole.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/lowlevel.o:	../common/lowlevel.c	makefile textconsole.h
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<


# explicit rules

textconsole:	$(OBJS)
	$(WRITE) '==> LINK: textconsole'
	$(Q)$(LD) $^ -o $(OBJSDIR)/$@
	$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/textconsole.lst
	$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/sbin/drivers/$(ARCH)/

