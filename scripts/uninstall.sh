#!/bin/sh
#
# uninstall.sh - Helper script for de-installation
#
# Written 2002 by Werner Almesberger
# Copyright 2002 Bivio Networks
#

dir="$1"
shift

if [ -z "$dir" -o ! -d "$dir" -o -z "$1" ]; then
    echo "usage: $0 install_dir file ..." 1>&2
    exit 1
fi

removed=
skipdir=
for n in $*; do
    if [ -e "$dir/$n" ]; then
	removed="$removed $dir/$n"
	# check local copy because installed copy may be gone if running
	# "make uninstall" twice, but we still want to get similar output
	if [ -d "$n" ]; then
	    rm -rf "$dir/$n"
	    skipdir="$skipdir $dir/$n"
	else
	    rm "$dir/$n"
	fi
    fi
done
[ -z "$removed" ] || echo "Removed:$removed"

skip=NOTHING
for n in `for n in $*; do echo "$dir/$n" | sed 's|/[^/]*$||'; done | \
  sort | uniq | ( echo $skipdir $skipdir | tr ' ' '\012'; cat; ) | \
  sort -r | uniq -u`; do
    echo $skip | grep "^$n" >/dev/null && continue
    rmdir $n || skip=$n/
done

exit 0
