#
# lightweight TCP/IP stack process 'lwip'
#  -- using lwIP 1.40 
#

# prettify the output
Q=@
WRITE = $(Q)echo 

.SUFFIXES: .c

OBJDUMPbin = $(PREFIX)objdump 

# set the build output dir
OBJSDIR = ../../../build/$(ARCH)_$(ARCH_TARGET)/lwip

# set where the root fs to store the kernel + OS os
PATHTOROOT = ../../../release/$(ARCH)_$(ARCH_TARGET)/root
PATHTOMKFS = ../../../release/$(ARCH)_$(ARCH_TARGET)/makeiso.sh

# defines
FLAGS		= -O2 -std=c99 -Wall -static -I../../lib/newlib/libgloss/libnosys -I. \
			-Iinclude -Iinclude/ipv4
CC		= $(PREFIX)gcc $(FLAGS)
LD		= $(PREFIX)gcc $(FLAGS)
OBJS	 	= $(OBJSDIR)/core/dhcp.o $(OBJSDIR)/core/init.o $(OBJSDIR)/core/mem.o $(OBJSDIR)/core/def.o \
			$(OBJSDIR)/core/memp.o $(OBJSDIR)/core/netif.o $(OBJSDIR)/core/pbuf.o \
			$(OBJSDIR)/core/raw.o $(OBJSDIR)/core/stats.o $(OBJSDIR)/core/sys.o $(OBJSDIR)/core/timers.o \
			$(OBJSDIR)/core/tcp.o $(OBJSDIR)/core/tcp_in.o $(OBJSDIR)/core/tcp_out.o \
			$(OBJSDIR)/core/udp.o $(OBJSDIR)/core/ipv4/autoip.o $(OBJSDIR)/core/ipv4/icmp.o \
			$(OBJSDIR)/core/ipv4/igmp.o $(OBJSDIR)/core/ipv4/ip.o $(OBJSDIR)/core/ipv4/ip_addr.o \
			$(OBJSDIR)/core/ipv4/ip_frag.o $(OBJSDIR)/core/ipv4/inet_chksum.o $(OBJSDIR)/netif/etharp.o \
			$(OBJSDIR)/api/tcpip.o $(OBJSDIR)/api/err.o $(OBJSDIR)/api/netifapi.o $(OBJSDIR)/api/api_lib.o \
			$(OBJSDIR)/api/api_msg.o $(OBJSDIR)/api/sockets.o $(OBJSDIR)/sys_arch.o

# targets
all: lwip

# dependencies
$(OBJSDIR)/core/def.o:		core/def.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/dhcp.o:		core/dhcp.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/init.o:		core/init.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/mem.o:		core/mem.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/memp.o:		core/memp.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/netif.o:	core/netif.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/pbuf.o:		core/pbuf.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/raw.o:		core/raw.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/stats.o:	core/stats.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/sys.o:		core/sys.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/timers.o:	core/timers.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/tcp.o:		core/tcp.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/tcp_in.o:	core/tcp_in.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/tcp_out.o:	core/tcp_out.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/udp.o:		core/udp.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<


$(OBJSDIR)/core/ipv4/autoip.o:	core/ipv4/autoip.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/ipv4/icmp.o:	core/ipv4/icmp.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/ipv4/igmp.o:	core/ipv4/igmp.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/ipv4/ip.o:	core/ipv4/ip.c		makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/ipv4/ip_addr.o:	core/ipv4/ip_addr.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/ipv4/ip_frag.o:	core/ipv4/ip_frag.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/core/ipv4/inet_chksum.o:	core/ipv4/inet_chksum.c		makefile
					$(WRITE) '==> COMPILE: $<'
					$(Q)$(CC) -c -o $@ $<



$(OBJSDIR)/netif/etharp.o:	netif/etharp.c		makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<


$(OBJSDIR)/api/tcpip.o:		api/tcpip.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/api/err.o:		api/err.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/api/netifapi.o:	api/netifapi.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/api/api_lib.o:	api/api_lib.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/api/api_msg.o:	api/api_msg.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/api/sockets.o:	api/sockets.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

$(OBJSDIR)/sys_arch.o:		sys_arch.c	makefile
				$(WRITE) '==> COMPILE: $<'
				$(Q)$(CC) -c -o $@ $<

# explicit rules

lwip:	$(OBJS)
	$(WRITE) '==> LINK: lwip'
	$(Q)$(LD) $^ -o $(OBJSDIR)/$@
	$(Q)$(OBJDUMPbin) --source $(OBJSDIR)/$@ >$(OBJSDIR)/lwip.lst
	$(Q)cp $(OBJSDIR)/$@ $(PATHTOROOT)/sbin/
