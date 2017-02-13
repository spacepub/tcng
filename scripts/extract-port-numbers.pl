#!/usr/bin/perl
#
# extract-port-numbers.pl - Extract port numbers from IANA port-numbers file
#
# Written 2001 by Werner Almesberger
# Copyright 2001 Network Robots
#

while (<>) {
    chop;
    next if /^\s*#/;
    next unless m#\s+(\d+)(-\d+)?/(udp|tcp)\s+#;
    $name = $`;
    $port = $1;
    $range = $2;
    $proto = $3;
    $dsc = $';
    next if $name eq "";
    next if $dsc =~ /\s*(Unassigned|Reserved)\s*$/;
    $name =~ s/\+\+/_PP/;  # whois++
    if ($name =~ /[^-A-Za-z0-9\.\*\/_]/) {
	print STDERR "ERROR: invalid character \"$&\" in $name ".
	  "($port/$proto)\n";
	next;
    }
    print STDERR "warning: using only port $port of $port$range\n"
      if defined $range;
    $name =~ tr/a-z/A-Z/;
    $name =~ tr#-.*/#_#;
    if (!defined $port{$name}) {
	$port{$name} = $port;
	$proto{$name} = $proto;
	next;
    }
    if ($port{$name} != $port) {
	print STDERR "warning: $name changed port from $port{$name} to $port\n";
	$port{$name} = $port;
	print STDERR "ERROR: protocol $name changed from $proto{$name} to ".
	  "$proto\n"
	    if $proto{$name} ne $proto;
    }
}

print "/* MACHINE-GENERATED - DO NOT EDIT ! */\n\n" || die;
print "/* Extracted from IANA port-numbers database, available from */\n".
  "/* http://www.iana.org/assignments/port-numbers */\n\n" || die;
print "#ifndef PORTS_TC\n\n" || die;
for (sort keys %port) {
    print "#define PORT_$_ $port{$_}\n" || die;
}
print "\n#endif /* PORTS_TC */\n" || die;
