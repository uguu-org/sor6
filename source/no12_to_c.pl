#!/usr/bin/perl -w
# Consume the output of convert_tab_to_c.pl for opus 6 no 12, and output
# a summary for each measure to indicate whether the pitch appears to be
# going up (2), down (1), or about the same (0).  Only the average of
# the first chord in each measure is used to make this determination.

use strict;

# Number of milliseconds per measure.
use constant MEASURE_DURATION => 200 * 16;

# Convert note to number of semitones away from C3.
# - Returns negative number if input is below C3.
# - Returns positive number if input is above C3.
# - Returns zero if input is C3.
sub note_to_pitch($)
{
   my ($note) = @_;

   $note =~ /([A-G])(\d+)/ or die $!;
   my @offset = (0, 2, 4, 5, 7, 9, 11);
   my $pitch = $offset[index("CDEFGAB", $1)] + $2 * 12;
   return $pitch - 36;
}

# Standard EADGBE tuning.
my @tuning =
(
   note_to_pitch("E4"),
   note_to_pitch("B3"),
   note_to_pitch("G3"),
   note_to_pitch("D3"),
   note_to_pitch("A2"),
   note_to_pitch("E2")
);


# Collect notes that appear at the beginning of each measure.
my @notes = ();
my $string_index = undef;
while( my $line = <> )
{
   if( $line =~ /_track(\d)\[\]/ )
   {
      $string_index = $1;
      if( $string_index >= (scalar @tuning) )
      {
         die "Unexpected string index: $1\n";
      }
   }
   elsif( $line =~ /^\s*\{(\d+),\s*([-]?\d+),\s*(\d+)\}/ )
   {
      my ($timestamp, $velocity, $fret) = ($1, $2, $3);

      # Validate song duration.
      if( $velocity == 0 )
      {
         unless( ($timestamp % MEASURE_DURATION) == 0 )
         {
            die "Song duration is not a multiple of " . MEASURE_DURATION .
                ", need to adjust MEASURE_DURATION\n";
         }
         next;
      }

      # Add note if it happened at the start of a measure.
      if( ($timestamp % MEASURE_DURATION) == 0 )
      {
         my $measure_index = $timestamp / MEASURE_DURATION;
         push @{$notes[$measure_index]}, $tuning[$string_index] + $fret;
      }
   }
}
my $measure_count = scalar @notes;

# Output measure summaries.
print "static const int kS12MeasureCount = $measure_count;\n",
      "static const uint8_t kS12MeasureDirection[$measure_count] =\n",
      "{\n";
my $previous_pitch;
for(my $i = 0; $i < $measure_count; $i++)
{
   my @n = @{$notes[$i]};

   # Summarize pitch for this measure by taking only the two notes of the
   # highest and lowest pitches, and average those two.  This better
   # captures directionality when transitioning from a 2 note chord to
   # a 3 note chord.
   my $lowest_pitch = $n[0];
   my $highest_pitch = $n[0];
   foreach my $j (@n)
   {
      if( $lowest_pitch > $j ) { $lowest_pitch = $j; }
      if( $highest_pitch < $j ) { $highest_pitch = $j; }
   }
   my $pitch = ($lowest_pitch + $highest_pitch) / 2;

   if( $i > 0 )
   {
      if( $pitch > $previous_pitch )
      {
         # Move up.
         print "\t2";
      }
      elsif( $pitch < $previous_pitch )
      {
         # Move down.
         print "\t1";
      }
      else
      {
         print "\t0";
      }
      print ",\t// $i\n";
   }
   $previous_pitch = $pitch;
}

# Direction for the final measure is always "no change".
print "\t0\t// $measure_count\n",
      "};\n"
