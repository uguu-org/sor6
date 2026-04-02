#!/usr/bin/perl -w
# Distill the song structure for Sor Opus 6 no 3 into a table indexed by ticks:
#
#  uint8_t kSorOp6No3[tick]
#
# Where "tick" is the dick index in the song data.  Each entry will contain
# a value in the range of 0..4 that selects the sprite indices that should
# be used for sprites in that tick.

use strict;
use constant STRING_COUNT => 6;
use constant TICKS_PER_BEAT => 4;


# Parse input into ticks, and mark all ticks that has at least one note playing.
my @ticks = ();
while( my $line = <> )
{
   next unless( $line =~ /^\d+[ +]*/ );

   # Get position of all ticks within this measure.
   my @ruler = ();
   for(my $i = 0; $i < length($line); $i++)
   {
      if( substr($line, $i, 1) eq "+" )
      {
         push @ruler, $i;
      }
   }

   # Load measure.
   my @measure = ();
   for(my $i = 0; $i < STRING_COUNT; $i++)
   {
      $line = <>;
      defined($line) or die;
      push @measure, $line;
   }

   # Check for notes playing at each tick.
   foreach my $i (@ruler)
   {
      my $has_notes = 0;
      for(my $j = 0; $j < STRING_COUNT; $j++)
      {
         if( substr($measure[$j], $i, 1) =~ /\d/ )
         {
            $has_notes = 1;
            last;
         }
      }
      push @ticks, $has_notes;
   }
}
(scalar @ticks) % TICKS_PER_BEAT == 0 or die;

print "#define SOR_OP6_NO3_TICK_COUNT ", (scalar @ticks), "\n",
      "static const uint8_t kSorOp6No3[", (scalar @ticks), "] =\n",
      "{\n";
for(my $t = 0; $t < (scalar @ticks); $t += TICKS_PER_BEAT)
{
   print "\t";
   if( $ticks[$t] && $ticks[$t + 1] && $ticks[$t + 2] )
   {
      # Got 3 consecutive notes in this beat.
      # We will use sprite indices 1..3 for these.
      for(my $i = 0; $i < TICKS_PER_BEAT - 1; $i++)
      {
         print $i + 1, ", ";
      }
   }
   else
   {
      # Got scattered notes or no notes in this beat.
      # Each tick will get assigned sprite 4, everything else gets 0.
      for(my $i = 0; $i < TICKS_PER_BEAT - 1; $i++)
      {
         print ($ticks[$t + $i] ? "4, " : "0, ");
      }
   }

   # Output index for last tick in this beat.
   print ($ticks[$t + TICKS_PER_BEAT - 1] ? "4,\n" : "0,\n");
}
print "};\n";
