#!/bin/bash -e
#
# symlinks.sh - Generate convenience symbolic links
#
# Written 2001,2002,2004 by Werner Almesberger
#
# Copyright 2001,2002 Network Robots, Bivio Networks
# Copyright 2002,2004 Werner Almesberger
#

if [ \( "$1" != list -a "$1" != link \) -o -z "$2" -o ! -z "$3" ]; then
    echo "usage: $0 mode target_dir" 1>&2
    echo '  mode is "list" or "link"' 1>&2
    exit 1
fi
mode=$1

if [ $mode = link ]; then
    . ./config
fi

link()
{
    dest="$1"
    shift
    if [ $mode != list ]; then
	mkdir -p "$dest"
    fi
    for n in "$@"; do
#--- for sanity checks only ---------------------------------------------------
#	if [ ! -e "$n" ]; then
#	    echo "$n does not exist" 1>&2
#	    exit 1
#	fi
#------------------------------------------------------------------------------
	if [ $mode = list ]; then
	    echo "$dest/`basename \"$n\"`" | sed 's|^\./||'
	else
	    ln -sf $TOPDIR/"$n" "$dest"
	fi
    done
}


#
# bin/tcng is special
#

if [ $mode = list ]; then
    echo $2/bin/tcng | sed 's|^\./||'
else
    mkdir -p $2/bin
    ln -sf $TOPDIR/tcc/tcc $2/bin/tcng
fi

#
# bin
#
link $2/bin \
  tcc/tcc tcc/tcc_var2fix.pl \
  tcsim/tcsim tcsim/tcsim_filter tcsim/tcsim_plot tcsim/tcsim_pretty

#
# lib/tcng/bin
#
link $2/lib/tcng/bin \
  tcc/tcc-module tcc/ext/tcc-ext-err tcc/ext/tcc-ext-null \
  tcc/ext/tcc-ext-file \
  tcsim/modules/kmod_cc tcsim/modules/tcmod_cc

#
# lib/tcng/include
#
link $2/lib/tcng/include \
  tcc/default.tc tcc/meta.tc tcc/fields.tc tcc/fields4.tc tcc/fields6.tc \
  tcc/values.tc tcc/meters.tc tcc/ports.tc tcc/idiomatic.tc \
  tcc/ext/tccext.h tcc/ext/echoh.h \
  tcc/tccmeta.h \
  tcsim/default.tcsim \
  tcsim/ip.def tcsim/packet.def tcsim/packet4.def tcsim/packet6.def \
  tcsim/tcngreg.def \
  tcsim/klib tcsim/ulib

#
# lib/tcng/lib
#
link $2/lib/tcng/lib \
  tcc/tcm_cls.c tcc/tcm_f.c \
  tcc/ext/libtccext.a

#
# lib/tcng/tests
#
if [ $mode == link ]; then
    ln -sfn ../.. lib/tcng/tests
    mkdir -p "$dest"
fi

exit 0
