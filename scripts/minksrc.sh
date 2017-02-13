#!/bin/sh -e
#
# minksrc.sh - Extract files needed to build tcsim from a kernel tarball
#
# Written 2003,2004 by Werner Almesberger
# Copyright 2003,2004 Werner Almesberger
#

[ -r tcsim/setup.klib ] || {
    echo "$0 must be run from the tcng top-level directory" 1>&2
    exit 1
}


usage()
{
    echo "usage: $0 [-q] path_to/linux-2.4.x.tar.bz2" 1>&2
    exit 1
}


if [ "$1" = -q ]; then
    quiet=true
    shift
else
    quiet=false
fi

[ "${1#-}" = "$1" ] || usage
[ ! -z "$1" ] || usage
[ -f "$1" ] || {
    echo "no such file: $1" 1>&2
    exit 1
}

id=`basename "$1" .tar.bz2`
[ "${id#linux-}" != "$id" ] || usage
if [ "${id#linux-2.4.}" == "$id" ]; then
    if [ "${id#linux-2.5.}" == "$id" ]; then
	echo "Error while extracting from $1:" 1>&2
	echo "$0 only supports 2.4 and 2.5 kernels" 1>&2
	exit 1
    fi
    first_new=4
else
    first_new=19
fi

v="${id#linux-2.[45].}"
[ "${v#[0-9]}" = "" -o "${v#[0-9][0-9]}" = "" ] || {
    echo "Error while extracting from $1:" 1>&2
    echo "kernel must be a regular release (no -pre, -rc, etc.)" 1>&2
    exit 1
}
if [ $v -lt $first_new ]; then
    dir=linux
else
    dir=$id
fi

#
# Check if we've already extracted the tree
#

if [ -f $dir/.tarball ] && \
  [ "`cat $dir/.tarball`" = "$1" -a "$1" -ot $dir/.tarball ]; then
    $quiet || echo "Source tree has already been extracted to $dir"
    exit 0
else
    rm -f $dir/.tarball
fi

#
# Find out which source files we really need
#

if grep 'KSRC.*KSRC' tcsim/setup.klib >/dev/null; then
    echo 'setup.klib must not contain lines with more than one use of $KSRC' \
      2>&1
    exit 1
fi

KFILES=`sed '/^#/d;/.*$KSRC\/\([a-zA-Z0-9_\/{},.-]*\).*/{s//\1/;p;};d' \
  <tcsim/setup.klib`
KFILES=`eval echo $KFILES`      # expand {...,...}
KFILES="Makefile $KFILES"
KFILES=`echo $KFILES | tr ' ' '\012' | sort | uniq`

#
# Ugly: since not all files may be present, we ignore all errors from tar
# (unfortunately, including ENOSPC, etc.)
#

$quiet || echo "Extracting files from $1 ..."
$quiet || echo '(may yield a few "Not found in archive" messages)'

tar xfj "$1" `for n in $KFILES; do echo $dir/$n; done` || true

echo "$1" >$dir/.tarball

$quiet || echo "Extracted to ./$dir/"
