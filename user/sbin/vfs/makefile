#
# virtual filesystem process 'vfs'
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set the build output dir
OBJSDIR = ../../../build/$(ARCH)_$(ARCH_TARGET)/vfs

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../release/$(ARCH)_$(ARCH_TARGET)/root
PATHTOMKFS = ../../../release/$(ARCH)_$(ARCH_TARGET)/makeiso.sh

# defines
FLAGS		= -g -O2 -std=c99 -Wall -static -I../../lib/newlib/libgloss/libnosys
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/main.o $(OBJSDIR)/vfstree.o $(OBJSDIR)/files.o

# targets
all: vfs

# dependencies
$(OBJSDIR)/main.o:	main.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/vfstree.o:	vfstree.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/files.o:	files.c	makefile
			$(WRITE) '==> COMPILE: $<'
			$(Q)$(CC) -c -o $@ $<

# explicit rules

vfs:	$(OBJS)
	$(WRITE) '==> LINK: vfs'
	$(Q)$(LD) $^ -o $(OBJSDIR)/$@
	$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/vfs.lst
	$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/sbin/
