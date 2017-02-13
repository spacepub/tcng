#
# Makefile - Build tcng libraries
#
# Written 2002 by Werner Almesberger
# Copyright 2002 Werner Almesberger
#

all:		libtcngmisc.a

include ../Common.make

OBJS=u128.o addr.o memutil.o

CLEAN=$(OBJS)

SPOTLESS=libtcngmisc.a .depend

CFLAGS=$(CC_OPTS) $(CFLAGS_WARN) $(CFLAGS_CONFIG)

.PHONY:			all dep depend clean spotless immaculate ephemeral

libtcngmisc.a:		$(OBJS)
			$(AR) crv libtcngmisc.a $(OBJS)
 
$(OBJS):		.depend

dep depend .depend:
			$(CPP) -MM -MG *.c >.depend || \
			  { rm -f .depend; exit 1; }

ephemeral:
			echo $(SPOTLESS)

clean:
			rm -f $(CLEAN)

spotless:		clean
			rm -f $(SPOTLESS)

immaculate:		spotless

ifeq (.depend,$(wildcard .depend))
include .depend
endif
