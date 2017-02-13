#!/bin/sh -e
#
# minisrc.sh - Extract files needed to build tcsim from an iproute2 tarball
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
    echo "usage: $0 [-q] path_to/iproute2-x.tar.gz" 1>&2
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

id=`basename "$1" .tar.gz`
[ "${id#iproute2-}" != "$id" -o "${id#iproute_20}" != "$id" ] || usage

if [ "${id#iproute2-2.6}" == "$id" ]; then
    dir=iproute2
else
    dir=./`echo $id | sed 's/-[^-]*$//'`
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

if grep 'ISRC.*ISRC' tcsim/setup.ulib >/dev/null; then
    echo 'setup.ulib must not contain lines with more than one use of $ISRC' \
      2>&1
    exit 1
fi

#
# The 1st substititution extracts anything that is prefixed by $ISRC
# The 2nd substititution removes wildcards
# The 3rd substititution removes empty entries (from the 2nd substitution)
#

IFILES=`sed '/^#/d;/.*$ISRC\/\([][a-zA-Z0-9_\/{},.*-]*\).*/ \
  {s//\1/;s/[a-zA-Z0-9_\/.-]*[][*][][a-zA-Z0-9_\/.*-]*[][*]//g;p;};d' \
    <tcsim/setup.ulib |
    sed 's/,,,*/,/g;s/{,,*/{/g;s/,,*}/}/g;s/{}//g;s/{\([^,]*\)}/\1/g'`
IFILES=`eval echo $IFILES`      # expand {...,...}
IFILES="Makefile $IFILES"
IFILES=`echo $IFILES | tr ' ' '\012' | sort | uniq`

#
# Ugly: since not all files may be present, we ignore all errors from tar
# (unfortunately, including ENOSPC, etc.)
#

$quiet || echo "Extracting files from $1 ..."
$quiet || echo '(may yield a few "Not found in archive" messages)'

#
# Since we have wildcards, we need to remove individual files or tar will
# needlessly complain.
#

tar xfz "$1" $dir/include/SNAPSHOT.h \
  $dir/'tc/*.[ch]' $dir/'lib/*.[ch]' \
  `for n in $IFILES; do \
    [ "${n#tc/*.[ch]}" = "" -o "${n#lib/*.[ch]}" = "" ] || echo $dir/$n; \
    done` || true

echo "$1" >$dir/.tarball

$quiet || echo "Extracted to ./$dir/"
