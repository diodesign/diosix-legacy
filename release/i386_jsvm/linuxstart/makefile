#
# makefile for the linuxstart glue to boot diosix on Bellard.org's JavaScript PC Emulator
#
#

LD =		ld -m elf_i386
CC =		gcc -m32
CFLAGS =	-O2 -Wall -fno-builtin #-march=i386 
HOST_CFLAGS =	-O2 -Wall

BUILDdir = 	../../../build/i386_jsvm/linuxstart

all: linuxstart.bin

$(BUILDdir)/linuxstart.out:	$(BUILDdir)/linuxstart_head.o $(BUILDdir)/linuxstart.o
				$(LD) -o $(BUILDdir)/$@ -N -Ttext 0x00010000 $^

linuxstart.bin:			$(BUILDdir)/linuxstart.out
				objcopy -O binary $(BUILDdir)/$< $@

$(BUILDdir)/linuxstart.o:	linuxstart.c
				$(CC) $(CFLAGS)  -c -o $@ $<

$(BUILDdir)/linuxstart_head.o:	linuxstart_head.S
				$(CC) -m32 -D__ASSEMBLY__ -c -o $@ $<
