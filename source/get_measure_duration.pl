#!/usr/bin/perl -w
# Parse guitar tab files and output the starting timestamp (in milliseconds)
# of each measure set.

use strict;

my $ms_per_tick = undef;
my $timestamp = 0;
while( my $line = <> )
{
   if( $line =~ /^(\d+)((?:\s*\+)+)\s*$/s )
   {
      unless( defined($ms_per_tick) )
      {
         die "Missing ms_per_tick line before first measure\n";
      }

      print "$1\t$timestamp\n";

      my $ticks = $2;
      $ticks =~ s/\s//g;
      $timestamp += $ms_per_tick * length($ticks);
   }
   elsif( $line =~ /^ms_per_tick\s*=\s*(\d+)/ )
   {
      $ms_per_tick = $1;
   }
}
print "end\t$timestamp\n";
