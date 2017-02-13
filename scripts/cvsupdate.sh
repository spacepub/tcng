#!/bin/sh
#
# cvsupdate.sh - Run "cvs update" and eliminate emphemeral files from list
#
# Written 2001 by Werner Almesberger
#
# Copyright 2001 Network Robots
#

vim_files()
{
    find . -name '.*.sw?' -print | sed 's|\./|? |'
}


emacs_files()
{
    find . -name '*~' -print | sed 's|\./|? |'
}


#
# cvs update outputs
# stdout: list of files
# stderr: directories
#

#
# Note: "make ephemeral" outputs more files than CVS knows about. E.g. it
# includes also symbolic links, and object files CVS would ignore anyway.
# This is not a problem, and it may be handy when implementing a global
# "make spotless" in some distant future.
#

(
    cvs -n update | sort -u
    make -s ephemeral | tr ' ' '\012' | sort -u | sed 's/^/? /p'
    vim_files
    emacs_files
) | sort | uniq -u

exit 0
