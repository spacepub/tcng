#!/bin/sh
#
# localize.sh - Complete installation of binary distribution
#
# Written 2001,2004 by Werner Almesberger
#
# Copyright 2001 Network Robots
# Copyright 2004 Werner Almesberger
#

if [ ! -d bin ]; then
    echo "$0: directory ./bin not found" 1>&2
    exit 1
fi

TCNG_INSTALL_CWD=/usr/lib/tcng
DESTDIR=$1

wrap()
{
    if [ -z "$2" ]; then
	if [ -f $1 ] && file $1 | grep ELF >/dev/null; then : ; else
	    return 0
	fi
	[ ! -x ${2:-$1} ] || mv ${2:-$1} ${2:-$1}.bin
    fi
    echo Creating link for $1
    ln -s $TCNG_INSTALL_CWD/${2:-$1} $1
}


fix()
{
    [ -f $1 -a -x $1 ] || return 0
    perl -pi -e \
     'BEGIN { $pwd = "'$TCNG_INSTALL_CWD'" } s/topdir=[^\$].*/topdir=$pwd/;' $1
}


wrap bin/tcc
wrap bin/tcng bin/tcc
wrap bin/tcsim
fix lib/tcng/bin/tcc-module
fix lib/tcng/bin/kmod_cc
fix lib/tcng/bin/tcmod_cc

exit 0
