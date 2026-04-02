#!/usr/bin/perl -w
# Distill the song structure for Sor Opus 6 no 7 into a table indexed
# by measures and quarter notes:
#
#  uint8_t kSorOp6No7[measure][index]
#
# Where "measure" is in the range of 0..79 and "index" is in the range
# of 0..3.  Values contain one of the following:
#  0 = silence.
#  1 = at least one note on first tick, zero notes on next two ticks.
#  2 = notes on first three ticks.

use strict;
use constant STRING_COUNT => 6;
use constant MEASURE_COUNT => 80;
use constant TICKS_PER_MEASURE => 6 * 4;

print "static const int kS07MeasureCount = ", MEASURE_COUNT, ";\n",
      "static const uint8_t kS07MeasureOverview[", MEASURE_COUNT, "][4] =\n",
      "{\n";
my $measure_number = 1;
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
   unless( scalar(@ruler) == TICKS_PER_MEASURE )
   {
      die "Unexpected measure length at line $.\n";
   }

   # Load measure.
   my @measure = ();
   for(my $i = 0; $i < STRING_COUNT; $i++)
   {
      $line = <>;
      defined($line) or die;
      push @measure, $line;
   }

   # Characterize this measure into 4 groups.
   my @overview = ();
   for(my $group = 0; $group < 4; $group++)
   {
      my $nonempty_tick1 = 0;
      my $nonempty_tick2 = 0;
      my $tick_index = $group * TICKS_PER_MEASURE / 4;
      for(my $i = 0; $i < STRING_COUNT; $i++)
      {
         if( substr($measure[$i], $ruler[$tick_index], 1) =~ /\d/ )
         {
            $nonempty_tick1 = 1;
         }
         if( substr($measure[$i], $ruler[$tick_index + 1], 1) =~ /\d/ )
         {
            $nonempty_tick2 = 1;
         }
      }

      if( $nonempty_tick1 )
      {
         if( $nonempty_tick2 )
         {
            push @overview, 2;
         }
         else
         {
            push @overview, 1;
         }
      }
      else
      {
         push @overview, 0;
      }
   }
   print "\t{", (join ", ", @overview), "},  // $measure_number\n";
   $measure_number++;
}

print "};\n";
