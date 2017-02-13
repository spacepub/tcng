#!/bin/sh -e
#
# compatibility.sh - Run regression tests for all supported kernel versions
#
# Written 2003,2004 by Werner Almesberger
# Copyright 2003,2004 Werner Almesberger
#

#
# Usage:
# - put all the kernel sources (as .tar.bz2 tarballs) in $KSRC_TBZ_DIR
# - extract a version of iproute2 without HTB support in $ISRC_WITHOUT_HTB
# - extract a version of iproute2 _with_ HTB support in $ISRC_WITH_HTB
# - scripts/compatibility from the tcng top-level directory
#
# WARNING: running this script will clobber any existing "config" file !
#

#
# Change the following variables for local system
#

KSRC_TBZ_DIR=/mnt/2/k
ISRC_WITH_HTB=/home/k/iproute2htb
ISRC_WITHOUT_HTB=/home/k/iproute2


extract()
{
    echo ENTER_$1

    #
    # Ugly: since not all files may be present, we must ignore all errors from
    # tar (including "disk full", etc.)
    #
    echo "Extracting files from $KSRC_TBZ_DIR/linux-$1.tar.bz2 ..."
    scripts/minksrc.sh -q $KSRC_TBZ_DIR/linux-$1.tar.bz2
}

#
# "old" kernels extract into linux/, "new" kernels extract into
# linux-<version>/
#

try_old()
{
    extract $1
    ./configure --no-defaults -k linux -i $ISRC_WITHOUT_HTB --no-manual
    make RUNTESTS_FLAGS=-t tests
    rm -rf linux
}

try_new_without_htb()
{
    extract $1
    ./configure --no-defaults -k linux-$1 -i $ISRC_WITHOUT_HTB --no-manual
    make RUNTESTS_FLAGS=-t tests
    rm -rf linux-$1
}

try_new_with_htb()
{
    extract $1
    ./configure --no-defaults -k linux-$1 -i $ISRC_WITH_HTB --no-manual
    make RUNTESTS_FLAGS=-t tests
    rm -rf linux-$1
}


if [ -z "$1" ]; then
    $0 magic 2>&1 | {
	v=
	while read l; do
	    if [ "${l#ENTER_}" != "$l" ]; then
		v="${l#ENTER_}"
		continue
	    fi
	    echo "$v $l"
	done
    }
    exit
fi

if [ "$1" != magic ]; then
    echo "usage: $0" 1>&2
    exit 1
fi

# 2.4.3 to 2.4.18
for n in 3 4 5 6 7 8 9 10 12 13 14 15 16 17 18; do
    try_old 2.4.$n
done

# 2.4.19
for n in 19; do
    try_new_without_htb 2.4.$n
done

# 2.4.20 to 2.4.27
for n in 20 21 22 23 24 25 26 27; do
    try_new_with_htb 2.4.$n
done

# 2.5.0 to 2.5.3
for n in 0 1 2 3; do
    try_old 2.5.$n
done

# 2.5.4
for n in 4; do
    try_new_without_htb 2.5.$n
done
