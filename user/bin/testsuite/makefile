#
# portable test suite for the diosix kernel API
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../release/$(ARCH)_$(ARCH_TARGET)/root
PATHTOMKFS = ../../../release/$(ARCH)_$(ARCH_TARGET)/makeiso.sh

# set the build output dir
OBJSDIR = ../../../build/$(ARCH)_$(ARCH_TARGET)/testsuite

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../lib/newlib/libgloss/libnosys
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/main.o $(OBJSDIR)/posix.o $(OBJSDIR)/fp.o $(OBJSDIR)/msg.o

# targets
all: testsuite

# dependencies
$(OBJSDIR)/main.o:	main.c	defs.h makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/posix.o:	posix.c	defs.h makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/fp.o:	fp.c	defs.h makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/msg.o:	msg.c	defs.h makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

testsuite:	$(OBJS)
		$(WRITE) '==> LINK: testsuite'
		$(Q)$(LD) $^ -o $(OBJSDIR)/$@
		$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/testsuite.lst
		$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/bin/

