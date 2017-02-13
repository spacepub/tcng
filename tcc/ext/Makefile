#
# Makefile - Build libtccext.a and related files
#
# Written 2001-2004 by Werner Almesberger
# Copyright 2001 Network Robots
# Copyright 2002-2004 Werner Almesberger
#

all:		libtccext.a tcc-ext-test tcc-ext-echo tcc-ext-echoh

include ../../Common.make

C_SRC=tccext.c echo.c echoh_main.c
OBJS=tccext.o echoh.o

CLEAN=$(OBJS) .depend

SPOTLESS=libtccext.a tcc-ext-test tcc-ext-echo tcc-ext-echoh

CFLAGS=$(CFLAGS_WARN) -g -fPIC
KMOD_CFLAGS=-D__KERNEL__ -DMODULE -O -fomit-frame-pointer -fno-strict-aliasing
KINCDIR=$(shell sed '/^KSRC=/s///p;d' <../../config)/include

.PHONY:		all depend dep clean spotless immaculate

libtccext.a:	$(OBJS)
		$(AR) cr libtccext.a $(OBJS)

tcc-ext-test:	tcc-ext-test.in ../../config
		../../scripts/topdir.sh ../.. tcc-ext-test

tcc-ext-echo:	echo.c libtccext.a
		$(CC) $(CFLAGS) -o tcc-ext-echo echo.c -L. -ltccext

tcc-ext-echoh:	echoh_main.c libtccext.a
		$(CC) $(CFLAGS) -o tcc-ext-echoh echoh_main.c -L. -ltccext

$(OBJS):	.depend

dep depend .depend:
		$(CPP) -MM -MG \
		  `echo $(C_SRC) | \
		   sed 's/\<cls_ext_test\.c\>//g'` >.depend || \
		  { rm -f .depend; exit 1; }

ephemeral:
		echo $(CLEAN) $(SPOTLESS)

clean:
		rm -f $(CLEAN)

spotless:	clean
		rm -f $(SPOTLESS)

immaculate:	spotless

ifeq (.depend,$(wildcard .depend))
include .depend
endif
