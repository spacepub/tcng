#!/usr/bin/perl
#
# meta.pl - Extract meta field definitions from meta.tc
#
# Written 2002 by Werner Almesberger
# Copyright 2002 Werner Almesberger
#

$size{"b"} = 1;
$size{"ns"} = 2;
$size{"nl"} = 4;
$size{"ipv4"} = 4;
$size{"ipv6"} = 16;

open(FILE,"meta.tc") || die "meta.tc: $!";
print "/* MACHINE-GENERATED. DO NOT EDIT ! */\n\n" || die "$!";
print "#ifndef META_H\n#define META_H\n\n" || die "$!";
print "#define META_FIELD_ROOT 1\n\n" || die "$!";
while (<FILE>) {
    chop;
    next unless /^field (meta_\S+)\s*=\s*__meta\[([^]]+)\].(\S+);/;
    ($name = $1) =~ tr/a-z/A-Z/;
    print "#define ${name}_OFFSET ($2)\n" || die "$!";
    print "#define ${name}_SIZE $size{$3}\n" || die "$!";
}
print "\n#endif /* META_H */\n" || die "$!";
close(STDOUT) || die "$!";
