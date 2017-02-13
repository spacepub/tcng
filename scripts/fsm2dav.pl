#!/usr/bin/perl
#
# fsm2dav.pl - Convert "raw" FSM output to daVinci term representation
#
# Written 2001 by Werner Almesberger
# Copyright 2001 Network Robots
#
# "Free" daVinci 2.1 can be found at
# http://www.tzi.de/daVinci/daVinci_get_daVinci.html

print "[\n";
$first = 1;
while (<>) {
    chop;
    if (/^(\d+): data (\d+) (\d+) (\d+) (\d+)/) {
	print ",\n" unless $first;
	print "l(\"S$1\",n(\"\",[a(\"OBJECT\",\"$2:$3\")],[\n";
	print "  l(\"E$1T\",e(\"\",[a(\"HEAD\",\"farrow\")],r(\"S$4\"))),\n";
	print "  l(\"E$1F\",e(\"\",[a(\"HEAD\",\"arrow\")],r(\"S$5\")))\n";
	print "  ]))";
    }
    elsif (/^(\d+): conform (\d+) (\d+) (\d+)/) {
	print ",\n" unless $first;
	print "l(\"S$1\",n(\"\",[a(\"OBJECT\",\"conform $2\")],[\n";
	print "  l(\"E$1T\",e(\"\",[a(\"HEAD\",\"farrow\")],r(\"S$3\"))),\n";
	print "  l(\"E$1F\",e(\"\",[a(\"HEAD\",\"arrow\")],r(\"S$4\")))\n";
	print "  ]))";
    }
    elsif (/^(\d+): count (\d+) (\d+)/) {
	print ",\n" unless $first;
	print "l(\"S$1\",n(\"\",[a(\"OBJECT\",\"count $2\")],[\n";
	print "  l(\"E$1T\",e(\"\",[a(\"HEAD\",\"farrow\")],r(\"S$3\")))\n";
	print "  ]))";
    }
    elsif (/^(\d+): <(.*)>/) {
	print ",\n" unless $first;
	print "l(\"S$1\",n(\"\",[a(\"OBJECT\",\"$2\")],[]))";
    }
    else {
	die "unrecognized input";
    }
    $first = 0;
}
print "\n]\n";
