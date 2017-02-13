#!/usr/bin/perl
#
# rndlogtst.pl - Generate a (large) set of random logic tests cases
#
# Written 2001 by Werner Almesberger
# Copyright 2001 Network Robots
#
$FIELDS = 5;
$ACCESSES = 8;
$TESTS = 200;

for ($i = 0; $i < $TESTS; $i++) {
    $s = &gen_ops(\$ACCESSES);
    print "
    for ($j = 0; $j < (1 << $FIELDS); $j++) {
	
    }
}

sub gen_ops
{
    local ($acc} = @_;

    my $rnd = random(4+$FIELDS);
    return "("
if $rnd == 0;
    return "(".&gen_ops($acc)."&&".&gen_ops($acc).")" if $rnd == 1;
    return "(".&gen_ops($acc)."||".&gen_ops($acc).")" if $rnd == 2;
    return "(!".&gen_ops($acc).")" if $rnd == 3;
    return $rnd & 1 if !${$acc};
    return "(raw[".($rnd-2)."] == 1)";
}
