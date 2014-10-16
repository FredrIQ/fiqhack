#!/usr/bin/perl

use Fatal qw/open close/;
use File::Temp qw/tempfile/;
use File::Find qw/find/;
use Carp;

@ARGV == 5 && $ARGV[3] eq '-o' or die
    "This script requires five arguments: dawnlike.map; a directory ".
    "containing an unpacked copy of DawnLike; tilecompile; -o; and ".
    "the file to output in.";

open my $mapfh, '<', $ARGV[0];

my %tiles_by_file;

while (my $l = <$mapfh>) {
    $l =~ /^!/ and next;
    $l =~ /\[(.*)\] / or next;
    my $f = $1;
    $l =~ s/\[(.*)\] //;

    if ($l =~ /^(.*walls \*): (0x.*)/) {
        my $p = $1;
        my $n = oct $2;
        my @o = (20, 1, 0, 2, 40, 42, 24, 44, 4, 25, 23);
        $l = "";
        for my $i (0..10) {
            my $p2 = $p;
            $p2 =~ s/walls \*/walls $i/g;
            $l .= sprintf "%s: 0x%X\n", $p2, $n+$o[$i];
        }
    }

    $tiles_by_file{$f} .= $l;
}

close $mapfh;

keys %tiles_by_file or die "No tile references found";

my %tempfile_by_file;

for my $f (keys %tiles_by_file) {
    my ($fh, $fn);
    ($fh, $fn) = tempfile("dawnlike-import-XXXXXX", TMPDIR => 1, UNLINK => 1);
    $tempfile_by_file{$f} = $fn;
    $fh->print($tiles_by_file{$f});
    close $fh;
}

my @cmdline;

for my $f (sort keys %tiles_by_file) {
    my $found = undef;
    find sub { $_ eq $f and $found = $File::Find::name }, $ARGV[1];
    defined $found or die "Could not locate image file '$f'";
    push @cmdline, $found, $tempfile_by_file{$f};
}

@cmdline = ("-z", 16, 16, "-W", "-t", "text", @cmdline, "-o", $ARGV[4]);

print "$ARGV[2] @cmdline\n";

system $ARGV[2], @cmdline;
