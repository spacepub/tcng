#
# Makefile - Create build support files
#
# Written 2001-2003 by Werner Almesberger
# Copyright 2001,2002 Bivio Networks, Network Robots
# Copyright 2002,2003 Werner Almesberger
#

SPOTLESS=bytesex tcng.spec tcsim.spec rpmrc rpmmacros valgrind.supp

.PHONY:		all rpmstuff dep depend clean spotless immaculate ephemeral

all:		rpmstuff valgrind.supp

bytesex:	bytesex.c
		$(CC) -Wall -o bytesex bytesex.c

rpmstuff:	tcng.spec tcsim.spec rpmrc rpmmacros

tcng.spec:	tcng.spec.in ../VERSION Makefile
		sed 's/\$$VERSION/'`cat ../VERSION`'/g;s|\$$PWD|'`pwd`\|g \
		  <tcng.spec.in >tcng.spec || { rm -f tcng.spec; exit 1; }

#
# Similar to the variables in ../Makefile, but not identical
#
SUGGESTED_KERNEL=`sed '/^$$ wget \(.*\/linux-.*\)/s//\1/p;d' <../README`
SUGGESTED_IPROUTE =`sed '/^$$ wget \(.*\/iproute2-.*\)/s//\1/p;d' <../README`

tcsim.spec:	tcsim.spec.in ../VERSION Makefile ../README
		sed 's/\$$VERSION/'`cat ../VERSION`'/g;s|\$$PWD|g'`pwd`\| \
		  <tcsim.spec.in | \
		  sed 's|\$$SUGGESTED_KERNEL|'$(SUGGESTED_KERNEL)\|g | \
		  sed 's|\$$SUGGESTED_IPROUTE|'$(SUGGESTED_IPROUTE)\|g \
		  >tcsim.spec || { rm -f tcsim.spec; exit 1; }

rpmrc:		Makefile
		echo "macrofiles: `rpm --showrc | \
		  sed '/^macrofiles .*: /s///p;d'`:`pwd`/rpmmacros" >rpmrc

rpmmacros:	Makefile
		echo "%_topdir `pwd`/../rpm" >rpmmacros

valgrind.supp:	valgrind.supp.in Makefile ../config
		{ . ../config; while read l; do eval echo $$l; done; } \
		  <valgrind.supp.in >valgrind.supp

dep depend:

ephemeral:
		echo $(SPOTLESS)

clean:

spotless:	clean
		rm -f $(SPOTLESS)

immaculate:	spotless
