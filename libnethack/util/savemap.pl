#!/usr/bin/perl
# Last modified by Alex Smith, 2014-11-23
# savemap: Reads save.c and similar files and maps byte offsets to
# variable names, dumping the results (typically to savemap.txt) for
# debugging purposes.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the MIT license. See copyright for details.
#
# Copyright © 2014 Derrick Sund.
use utf8;     # this source file is UTF-8
use warnings;
use strict;

if ($#ARGV < 1) {
    print "Usage: savemap.pl file file...\n";
    print "inputs: the C files to scan, such as ../src/*.c\n";
    print "output goes to stdout\n";
    exit;
}

#The current tag.  Empty string if we don't have one.
my $tag = "";
my $tag_was_written = 0;
my $offset = 0;

while (my $line = <>) {
    #First, try to find a tag if we don't have one.
    if (!$tag) {
        if (index($line, "mtag") != -1) {
            $tag = $line;
            $tag =~ m/([A-Z]+_[A-Z]+)/;
            $tag = $1;
            $offset = 0;
            $tag_was_written = 0;
        }
        next;
    }

    #We have a tag.  Now see if the line is an mwrite.
    $line =~s/^\s+//;
    #Skip whitespace lines, lines we're told to ignore, and lines with no
    #semicolon (which are part of a longer statement that we can ignore as
    #a block).
    if (!$line || $line =~ /savemap: ignore/ || $line !~ /;|savemap:/) {
        next;
    }

    unless ($line =~ /savemap: (\d+)/ || $line =~ /mwrite(\d+)/) {
        #It's a non-whitespace line that isn't mwrite and doesn't have
        #a comment for savemap; our tag has expired.
        $tag = "";
        print "\n";
        next;
    }

    my $offset_increase = $1 / 8;

    if(!$tag_was_written) {
        print "Current tag: $tag\n";
        $tag_was_written = 1;
    }

    $line =~ m/(\s[][\w.\->]+)/;
    print "$1 $offset\n";

    $offset += $offset_increase;
}
