#!/usr/bin/perl
# Last modified by Derrick Sund, 2014-02-18
use utf8;     # this source file is UTF-8
use warnings;
use strict;

# savemap: Reads save.c and maps byte offsets to variable names, dumping
# the results to savemap.txt for debugging purposes.
# Copyright © 2014 Derrick Sund.
open SAVESOURCE, "<", "../src/save.c" or die "Couldn't read save.c!";
open MAPFILE, ">", "savemap.txt" or die "Couldn't write to savemap.txt!";

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
