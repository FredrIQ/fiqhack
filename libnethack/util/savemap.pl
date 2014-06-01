#!/usr/bin/perl
# savemap: Reads save.c and maps byte offsets to variable names, dumping
# the results to savemap.txt for debugging purposes.
# Copyright © 2014 Derrick Sund.
# Last modified by Derrick Sund, 2014-06-01
use utf8;     # this source file is UTF-8
use warnings;
use strict;

if ($#ARGV < 1) {
    print "Usage: savemap.pl input output\n";
    print "input: the C file to scan, such as ../src/save.c\n";
    print "output: the file to write the savemap to\n\n";
    exit;
}

open SAVESOURCE, "<", $ARGV[0] or die "Couldn't read input file!";
open MAPFILE, ">", $ARGV[1] or die "Couldn't write to output file!";

#The current tag.  Empty string if we don't have one.
my $tag = "";
my $tag_was_written = 0;
my $offset = 0;

while (my $line = <SAVESOURCE>) {
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
    #Skip whitespace lines.
    if (!$line) {
        next;
    }

    if (index($line, "mwrite") == -1) {
        #It's a non-whitespace line that isn't mwrite; our tag has expired.
        $tag = "";
        print MAPFILE "\n";
        next;
    }

    if (!($line =~ m/\d+\(/)) {
        #It's an mwrite that depends on sizeof.  Doing this up right would
        #be way more trouble than it's worth, so such mwrites go at the end
        #and we ignore them.
        $tag = "";
        print MAPFILE "\n";
        next;
    }
    
    $line =~ m/(\d+)/;
    my $offset_increase = $1 / 8;

    if(!$tag_was_written) {
        print MAPFILE "Current tag: ".$tag."\n";
        $tag_was_written = 1;
    }

    $line =~ m/(\s[\w.\->]+)/;
    print MAPFILE $1." ".$offset."\n";

    $offset += $offset_increase;
}
