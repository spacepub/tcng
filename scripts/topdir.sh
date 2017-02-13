#!/bin/sh
#
# topdir.sh - Replace TOPDIR in foo.in, generating foo
#
# Written 2001 by Werner Almesberger
# Copyright 2001 Network Robots
#

usage()
{
    echo "usage: $0 config-file-dir out-file" 2>&1
    exit 1
}


[ ! -d "$1" -o ! -r "$1/config" -o ! -r "$2.in" -o ! -z "$3" ] && usage

sed 's/.*modified by.*/# MACHINE-GENERATED - DO NOT EDIT !/' <$2.in | \
  sed "s|\$TOPDIR|`sed '/^TOPDIR=/s///p;d' <$1/config`|g" >$2 || \
  { rm -f $2; exit 1; }
chmod 755 $2
