# Makefile for CH374HFT

MACHINE	      = m68k
ARCH	      = m68knommu
CROSS_COMPILE = m68k-elf-
CROSS	      = $(CROSS_COMPILE)

CC	  = $(CROSS_COMPILE)gcc -m68000 -D__ELF__
AS	  = $(CROSS_COMPILE)as -m68000 -D__ELF__
CXX	  = $(CROSS_COMPILE)g++
AR	  = $(CROSS_COMPILE)ar
LD	  = $(CROSS_COMPILE)ld
OBJCOPY	  = $(CROSS_COMPILE)objcopy
RANLIB	  = $(CROSS_COMPILE)ranlib
ELF2FLT	  = elf2flt
STRIPTOOL = $(CROSS_COMPILE)strip
STRIP	  = $(STRIPTOOL)

GCC_DIR	    = $(shell $(CC) -v 2>&1|grep specs|sed -e 's/.* \(.*\)specs/\1\./')
GCC_LIB	    = /usr/local/m68k-elf/lib

LIBC	    = -lc
LIBM	    = -lm
LIBNET	    = -lnet
LIBDES	    = -ldes
LIBPCAP	    = -lpcap
LIBSSL	    = -lssl
LIBCRYPTO   = -lcrypto
LIBCRYPT    = -lcrypt
LIBGCC	    = -lgcc
LIBIBERTY   = -liberty
LIBIO	    = -lio
LIBIOSTREAM = -liostream
LIBSTDCPP   = -lstdc++

LDPATH	    = \
		-L/usr/local/lib/libc \
		-L/usr/local/lib/libm \
		-L/usr/local/lib/libnet

INCLIBC	    = -I/usr/local/lib/libc/include
INCLIBM	    = -I/usr/local/lib/libm
INCNET	    = -I/usr/local/lib/libnet

CFLAGS  := -Os -g -fomit-frame-pointer
CFLAGS  += -Dlinux -D__linux__ -Dunix -D__uClinux__ -DEMBED
CFLAGS  += $(INCLIBC)
CFLAGS  += -fno-builtin
CFLAGS  += -msep-data -malign-int

CFLAGS  += -I/root/mp30/uclinux/include

LDFLAGS  = $(CFLAGS) -Wl,-elf2flt
LDLIBS   = $(LDPATH) $(LIBC)

EXEC  = ch374hft
OBJS  = ch374hft.o
LOCAL_LIBS = ../libCH374HFR.a

all: $(EXEC)

$(EXEC): $(OBJS) $(LOCAL_LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LOCAL_LIBS) $(LDLIBS)

clean:
	rm -f $(EXEC) *.elf *.gdb *.o
