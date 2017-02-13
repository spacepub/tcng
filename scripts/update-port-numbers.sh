#!/bin/sh
#
# update-port-numbers.sh - Quarterly update of port-numbers file from IANA
#
# Written 2001,2002 by Werner Almesberger
# Copyright 2001 Network Robots
# Copyright 2002 Werner Almesberger
#

FILE=port-numbers
URL=http://www.iana.org/assignments/port-numbers
DAYS=92

if [ -z "`find . -name $FILE -mtime -$DAYS`" ]; then
    wget --passive "$URL" -O $FILE.tmp || \
      { rm -f $FILE.tmp; exit 1; }
    mv $FILE.tmp $FILE || exit 1
fi
exit 0
