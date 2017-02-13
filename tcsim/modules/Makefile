#
# modules/Makefile - Build the test modules
#
# Written 2001-2004 by Werner Almesberger
# Copyright 2001 EPFL-ICA, Network Robots
# Copyright 2002-2004 Werner Almesberger
#

#
# This Makefile tries to mimic the compiler invocation when building for a
# "real" system as closely as possible. This is to ensure that kmod_cc and
# tcmod_cc are truly "drop in" replacements for cc.
#

all:		$(KMODS) $(TCMODS)

include ../../Common.make

KMODS=sch_discard.o cls_unspec.o
TCMODS=q_discard.so f_unspec.so
OBJS=$(KMODS) $(TCMODS)

SPOTLESS=*.o *.so kmod_cc tcmod_cc

KMOD_CC=./kmod_cc
KMOD_CFLAGS=-D__KERNEL__ -DMODULE -O -fomit-frame-pointer -fno-strict-aliasing
KINCDIR=$(shell sed '/^KSRC=/s///p;d' <../../config)/include
KVERSIONNUM=$(shell \
  sed '/KVERSION=\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)/\
  s//`printf "0x%02x%02x%02x" \1 \2 \3`/p;d' <../../config)
TCMOD_CC=./tcmod_cc
TCMOD_CFLAGS=-shared
CFLAGS=-g -Wall -DKVERSIONNUM=$(KVERSIONNUM)

.SUFFIXES:	.so
.PHONY:		all clean spotless immaculate

all:		$(KMODS) $(TCMODS)

$(OBJS):	Makefile ../../Common.make ../../config
$(KMODS):	kmod_cc
$(TCMODS):	tcmod_cc

kmod_cc:	kmod_cc.in ../../config
		../../scripts/topdir.sh ../.. kmod_cc

tcmod_cc:	tcmod_cc.in ../../config
		../../scripts/topdir.sh ../.. tcmod_cc

.c.o:
		$(KMOD_CC) -c $(CFLAGS) $(KMOD_CFLAGS) -I$(KINCDIR) -o $@ $<

.c.so:
		$(TCMOD_CC) $(CFLAGS) $(TCMOD_CFLAGS) -o $@ $<

ephemeral:
		echo $(SPOTLESS)

clean:

spotless:	clean
		rm -f $(SPOTLESS)

immaculate:	spotless
