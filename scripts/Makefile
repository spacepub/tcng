#
# Makefile - Build trinity.sh
#
# Written 2001,2002 by Werner Almesberger
# Copyright 2001 Network Robots
# Copyright 2002 Werner Almesberger
#

.PHONY:		all dep depend clean spotless immaculate ephemeral

all:		trinity.sh

trinity.sh:	trinity.sh.in topdir.sh ../config
		./topdir.sh .. trinity.sh

dep depend:

ephemeral:
		echo trinity.sh

clean:

spotless:	clean
		rm -f trinity.sh

immaculate:	spotless
