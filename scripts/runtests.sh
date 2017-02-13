#!/bin/sh
#
# runtests.sh - Run regression tests from standard input
#
# Written 2001-2004 by Werner Almesberger
# Copyright 2001 EPFL-ICA
# Copyright 2001,2002 Bivio Networks, Network Robots
# Copyright 2002-2004 Werner Almesberger
#

#
# The input file(s) has/have the following structure:
#
# FILE <file_name>
# # <test_title>
# <command_line>
# <input>
# ...
# EOF
# ERROR
# <output>
# ...
# (EOF or next test)
#
# The directives FILE and ERROR are optional. <input> and <output> are zero or
# more lines. <command_line> is any valid shell command line (which may contain
# multiple commands), and may be spread over multiple lines by continuing lines
# with a backslash.
#
# The only comments allowed are the test titles, but <input> may contain lines
# beginning with a hash sign (#). runtests.sh defines the commands tcc, tcsim,
# trinity, and tcsim_filter, so that no path names have to be specified to
# access them.
#
# runtests.sh executes each command with standard input from the <input>
# section. Normally, the command is expected to return with exit status zero
# (success), and to produce exactly the output defined in the <output> section.
# If the command writes anything to standard error, runtests.sh issues a
# warning, but does not count this as a problem.
#
# If the ERROR directive is present, the command is expected to exit with
# non-zero exit status, and runtests.sh only issues a warning if the command's
# output does not correspond exactly to the <output> section. (Rationale: in
# this case, the output is probably an error message, and error messages are
# frequently changes or improved, so a difference does not necessarily indicate
# a problem.)
#
# The FILE directive indicates from which file the following tests originate,
# and is printed when runtests.sh encounters a problem.
#

#
# Test cases can also be conditionally executed (e.g. is using a subsystem that
# may not be available in all possible installations). In this case, the
# following structure is used:
#
# BEGIN CONDITIONAL
# test case without title
# conditional test case(s)
# ...
# END CONDITIONAL
#
# Note that the word "CONDITIONAL" must be immediately followed by a newline -
# trailing spaces are now allowed. Conditional tests cannot be nested.
#

#
# The defaulting rules below are a little complicated. There's what they do:
#
#   Environment   | config ||     Result	Comment
# TOPDIR | TCNG_  | TOPDIR || TOPDIR | TCNG_
#        | TOPDIR |        ||        | TOPDIR
# -        -      | -      || .        .
# -        B      | -      || B        B
# -        -      | C      || C        C
# -        B      | C      || C        B	probably undesirable
# A        -      | -      || A        A
# A        -      | C      || C        C
# A        B      | -      || A        B	probably undesirable
# A        B      | C      || C        A	probably undesirable
#

[ -z "$TOPDIR" ] && TOPDIR=$TCNG_TOPDIR
[ -z "$TOPDIR" ] && TOPDIR=.


find_in_path()
{
    for n in `echo $2 | tr : ' '`; do
	if [ -x $n/$1 ]; then
	    echo $n/$1
	    return
	fi
    done
    echo "$1 not found" 1>&2
    exit 1
}


[ -z "$TCC" ] && TCC="`find_in_path tcc $TOPDIR/bin:$PATH`"
TCSIM_CMD="`find_in_path tcsim $TOPDIR/bin:$PATH`"
TRINITY="`find_in_path trinity.sh $TOPDIR/scripts:$PATH`"
TCSIM_FILTER="`find_in_path tcsim_filter $TOPDIR/bin:$PATH`"
TCSIM_PLOT="`find_in_path tcsim_plot $TOPDIR/bin:$PATH`"
VALGRIND=

set -a
[ -r config ] && . ./config
set +a
set -o noglob

[ -z "$TCNG_TOPDIR" ] && TCNG_TOPDIR=$TOPDIR

export TOPDIR TCNG_TOPDIR


usage()
{
    echo "usage: $0 [-c] [-d] [-g] [-i] [-t] [-u] [-Xphase,arg] [file ...]" 1>&2
    echo "  -c           only count tests but don't run them" 1>&2
    echo "  -d           print test output, don't compare with reference" 1>&2
    echo "  -g           use Valgrind" 1>&2
    echo "  -i           continue after errors" 1>&2
    echo "  -t           run only tests that probably use tcsim" 1>&2
    echo "  -u           print output mismatches as unified context diffs" 1>&2
    echo "  -Xphase,arg  verbatim argument for specific build phase" 1>&2
    echo "               phases: c=tcc, m=tcc-module, k=kmod_cc, s=tcsim," 1>&2
    echo "                       t=tcmod_cc,  x=external (all)," 1>&2
    echo "                       xelem=external (element)" 1>&2
    exit 1
}


phase_arg()
{
    phase=`echo "$1" | sed 's/^-X\(.\).*/\1/'`;
    arg=`echo "$1" | sed 's/^-X.,//'`;
    case "$phase" in
        c)      tcc_args="$tcc_args $arg";;
        s)      tcsim_args="$tcsim_args $arg";;
	[mktx])	phase_args="$phase_args -X$phase,$arg";;
        *)      usage;;
    esac
}

tcc_args=
tcsim_args=
phase_args=

debug=false
stop_on_error=true
diff=false
tcsim_only=false

if [ -z "$TCNG_TESTS_COUNT" ]; then
    count=false
else
    count=true
fi

while [ ! -z "$1" ]; do
    case "$1" in
	-c) count=true;;
	-d) debug=true;;
	-g) VALGRIND="valgrind -q --trace-children=yes"
	    VALGRIND="$VALGRIND --suppressions=$TOPDIR/build/valgrind.supp";;
	-i) stop_on_error=false;;
	-t) tcsim_only=true;;
	-u) diff=true;;
	-X[cmkst],*) phase_arg "$1";;
	-Xx*,*) phase_arg "$1";;
	-X) shift; phase_arg "-X$1";;
	-*) usage;;
	*)  break;;
    esac
    shift
done

for n in "$@"; do
    <$n || exit 1
done


tcsim_unadorned()
{
    $VALGRIND $TCSIM_CMD "$@" 2>_tmp_err.$$
    retval=$?
    sed '/^ *$/d;/^ *Electric Fence/d' <_tmp_err.$$ 1>&2
    rm -rf _tmp_err.$$
    return $retval
}


tcsim()
{
    tcsim_unadorned -g $tcsim_args $phase_args "$@"
}


tcc_unadorned()
{
    $VALGRIND $TCC "$@" 2>_tmp_err.$$
    retval=$?
    sed '/^ *$/d;/^ *Electric Fence/d' <_tmp_err.$$ 1>&2
    rm -rf _tmp_err.$$
    return $retval
}


tcc()
{
    tcc_unadorned -q $tcc_args $phase_args "$@"
}


trinity()
{
    $TRINITY "$@" 2>_tmp_err.$$
    retval=$?
    sed '/^ *$/d;/^ *Electric Fence/d' <_tmp_err.$$ 1>&2
    rm -rf _tmp_err.$$
    return $retval
}


tcsim_filter()
{
    $TCSIM_FILTER "$@"
}


tcsim_plot()
{
    $TCSIM_PLOT "$@"
}


runtests()
{
    sed 's/^|//' | scripts/runtests.sh
}


comtc()
{
    PATH=$TOPDIR/bin:$PATH toys/comtc -q "$@" 2>_tmp_err.$$
    retval=$?
    sed '/^ *$/d;/^ *Electric Fence/d' <_tmp_err.$$ 1>&2
    rm -rf _tmp_err.$$
    return $retval
}


stderr()
{
    if [ -s _err.$$ ]; then
	echo "  Standard error:"
	sed 's/^/  | /' <_err.$$
    fi
    rm -f _err.$$
}


stdout()
{
    echo "  Standard output:"
    sed 's/^/  | /' <_out.$$
}


refout()
{
    echo "  Reference output:"
    sed 's/^/  | /' <_ref.$$
}


failed()
{
    echo "$1"
    stderr
    [ -z "$file" ] || echo "File:      $file"
    echo "Command:   $cmd"
    echo "Input:     _in.$$"
    echo "Output:    _out.$$"
    echo "Reference: _ref.$$"
    if $diff; then
	echo "----- diff -u _ref.$$ _out.$$ -----"
	diff -u _ref.$$ _out.$$
    fi
    $stop_on_error && exit 1
    echo
    failed=`expr $failed + 1`
}

first_file()
{
    file=`echo "$l" | sed '/^FILE /s///p;d'`
    if [ -z "$file" ]; then
	file="$next_file"
    else
	read l
    fi
    next_file="$file"
}

#
# Input/output;
#   Variable:
#     l			next line in input
#     next_file		next file name (empty if unknown)
#
# Output:
#   Variables:
#     file		name of test case file (empty if unknown)
#     next_file		next file name (empty if unknown)
#     title		test title
#     cmd		test command
#     expect_error	true if test succeeds if command fails
#
#   Files:
#     _in.$$		standard input for test
#     _ref.$$		expected test output
#

read_test()
{
    first_file
    title="`echo "$l" | sed 's/# //;s/ -*$//'`"
    read cmd
    >_ref.$$
    >_in.$$
    expect_error=false
    while read l; do
	echo "$l" | grep -s '^\(# \|BEGIN CONDITIONAL$\|END CONDITIONAL$\)' \
	  >/dev/null && break
	if [ "$l" = ERROR ]; then
	    expect_error=true
	elif [ "$l" = EOF ]; then
	    mv _ref.$$ _in.$$
	    >_ref.$$
	else
	    _file=`echo "$l" | sed '/^FILE /s///p;d'`
	    if [ -z "$_file" ]; then
		echo "$l" >>_ref.$$
	    else
		next_file="$_file"
	    fi
	fi
    done
}


run_tests()
{
    conditional=false
    condition=false
    skipping=false
    c=0
    w=0
    skipped=0
    read l
    first_file
    while [ ! -z "$l" ]; do
	if [ "$l" = "BEGIN CONDITIONAL" ]; then
	    if $conditional; then
		echo "BEGIN CONDITIONAL in conditional block" 1>&2
		exit 1
	    fi
	    conditional=true
	    condition=true
	elif [ "$l" = "END CONDITIONAL" ]; then
	    if $conditional; then
		conditional=false
		skipping=false
	    else
		echo "END CONDITIONAL without condition" 1>&2
		exit 1
	    fi
	    read l
	    first_file
	    continue
	fi
	read_test
	if $skipping || ( $tcsim_only && ! $condition && \
	  [ "${cmd##*tcsim*}" = "$cmd" -a \
	    "${cmd##*trinity*}" = "$cmd" ]; ); then
	    skipped=`expr $skipped + 1`
	    rm -f _in.$$ _ref.$$
	    continue
	fi
	if $condition; then :; else
	    echo -n "$title: "
	fi
	LANG=C eval "$cmd" <_in.$$ >_tmp_out.$$ 2>_err.$$
	result=$?
	sed 's/ *$//'"`$expect_error && echo ';s/ parse / syntax /'`" \
	  <_tmp_out.$$ >_out.$$
	rm -f _tmp_out.$$
	cmp -s _out.$$ _ref.$$
	equal=$?
	if $expect_error; then
	    if [ $result = 0 ]; then
		if $condition; then
		    skipping=true
		else
		    failed "FAILED (expected error)"
		fi
	    else
		if [ $equal = 0 ]; then
		    if $condition; then
			skipping=false
		    else
			echo PASSED
			rm -f _err.$$
			c=`expr $c + 1`
		    fi
		else
		    if $condition; then
			echo "Warning: output differs in condition"
			skipping=false
		    else
			echo "PASSED (warning: output differs)"
			c=`expr $c + 1`
		    fi
		    stderr
		    stdout
		    refout
		    w=`expr $w + 1`
		fi
	    fi
	else
	    if [ $result != 0 ]; then
		if $condition; then
		    skipping=true
		else
		    failed "FAILED (got error)"
		fi
	    else
		if [ $equal = 0 ]; then
		    if $condition; then
			skipping=false
		    else
			echo PASSED
			stderr
			c=`expr $c + 1`
		    fi
		else
		    if $condition; then
			skipping=true
		    else
			failed FAILED
		    fi
		fi
	    fi
	fi
	rm -f _in.$$ _out.$$ _ref.$$
	condition=false
    done
    if $conditional; then
	echo "EOF in conditional block" 1>&2
	exit 1
    fi
    if [ "$c" = 1 ]; then
	echo -n "Passed the test"
    else
	echo -n "Passed all $c tests"
    fi
    if [ `expr $w  + $skipped` = 0 ]; then
	echo
    else
	echo -n " ("
	if [ $w != 0 ]; then
	    if [ $w = 1 ]; then
		echo -n "1 warning"
	    else
		echo -n "$w warnings"
	    fi
	    if [ $skipped != 0 ]; then
		echo -n ", "
	    fi
	fi
	if [ $skipped != 0 ]; then
	    if [ $skipped = 1 ]; then
		echo -n "1 conditional test skipped"
	    else
		echo -n "$skipped conditional tests skipped"
	    fi
	fi
	echo ")"
    fi
    if [ $failed -gt 0 ]; then
	echo "$failed test(s) FAILED !"
    fi
}


fill()
{
    # superbly inefficient, but hey, CPU cycles are cheap :-)
    s="$2$1$2"
    while [ `echo "$s" | wc -c` -lt 79 ]; do
	s="$2$s$2"
    done
    echo $s
}


debug_tests()
{
    read l
    while [ ! -z "$l" ]; do
	read_test
	fill " $title " =
	echo "$cmd"
	fill "" -
	eval "$cmd" <_in.$$
	rm -f _in.$$ _ref.$$
	echo
    done
}


count_tests()
{
    c=0
    read l
    while [ ! -z "$l" ]; do
	read_test
	rm -f _in.$$ _ref.$$
	c=`expr $c + 1`
	echo -en "$c\r"
    done
    echo "Counted $c test(s)"
}


cat_files()
{
    if [ -z "$*" ]; then
	cat
    else
	for n in "$@"; do
	    echo FILE "$n"
	    cat "$n"
	done
    fi
}


failed=0
next_file=

if $count; then
    cat_files "$@" | count_tests
elif $debug; then
    cat_files "$@" | debug_tests
else
    cat_files "$@" | run_tests
fi
