#!/usr/bin/perl
# Real Perl: references, regexes, hashes, subroutines, file handling.
use strict;
use warnings;
use feature qw(say signatures);

my %counts;
my @failures;

sub read_lines ($path) {
    open(my $fh, '<', $path) or die "cannot open $path: $!";
    my @lines = <$fh>;
    close($fh);
    return \@lines;
}

sub tally ($lines, $pattern) {
    my $total = 0;
    foreach my $line (@$lines) {
        chomp $line;
        next if $line =~ /^\s*#/;
        if ($line =~ m/$pattern/x) {
            $counts{$1}++ if defined $1;
            $total++;
        }
    }
    return $total;
}

my $ref = read_lines($ARGV[0] // 'input.txt');
my $found = tally($ref, qr/(\w+)\s*=\s*(\d+)/);

foreach my $key (sort { $counts{$b} <=> $counts{$a} } keys %counts) {
    printf("%-20s %d\n", $key, $counts{$key});
}

say "total: $found";
