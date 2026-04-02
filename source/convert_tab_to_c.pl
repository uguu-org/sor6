#!/usr/bin/perl -w
# Convert guitar tab file to C arrays.
#
# See sor_op6_no11.txt for example input format.
#
# See SongNote struct and Song struct in bgm.c for output format.

use strict;

# Number of song tracks.  This matches number of guitar strings.
use constant TRACK_COUNT => 6;

# Maximum note duration in milliseconds.
use constant MAX_DURATION_MILLISECONDS => 2500;

# Minimum delay between notes in milliseconds.  We insert a little bit of
# delay between each note such that new notes do not abruptly cut off old
# notes that are played on the same track.  This avoids some minor blips
# that would have be audible on short consecutive notes.
#
# This is now set to zero, since playing successive notes on separate
# channels sounded better.  See comments near CHANNELS_PER_STRING in bgm.c.
use constant MARGIN => 0;

# Duration multipliers.  See comments near duration_ms in bgm.c
use constant FULL_VELOCITY => -1;
use constant REDUCED_VELOCITY => +1;

# Special duration multiplier for rest notes.
use constant REST_NOTE => 0;

# Time multiplier factor, mostly used for debugging.  Setting this
# value higher than 1 will cause the song to be played slower.
use constant TIME_MULTIPLIER => 1;

# Check command line.
if( $#ARGV < 0 )
{
   die "$0 {variable_prefix} {tab_data.txt}\n";
}
my $variable_prefix = shift @ARGV;

# Initialize empty tracks.
my @tracks = ();
for(my $i = 0; $i < TRACK_COUNT; $i++)
{
   push @tracks, [];
}

# Load lines.
my $ms_per_tick = undef;
my $time = 0;
my @tuning = ();
while( my $line = <> )
{
   # Match tuning lines, e.g.:
   #   tuning = EADGBE
   if( $line =~ /^tuning\s*=\s*([a-gA-G]{6})/ )
   {
      if( (scalar @tuning) != 0 )
      {
         die "Multiple tuning definition\n";
      }
      if( uc($1) eq "EADGBE" )
      {
         @tuning = (0, 0, 0, 0, 0, 0);
      }
      elsif( uc($1) eq "DADGBE" )
      {
         @tuning = (0, 0, 0, 0, 0, -2);
      }
      else
      {
         die "Unsupported tuning: $1\n";
      }
   }

   # Match timing lines, e.g.:
   #   ms_per_tick = 200
   if( $line =~ /^ms_per_tick\s*=\s*(\d+)/ )
   {
      if( $1 <= 0 )
      {
         die "Bad ms_per_tick: $1\n";
      }
      $ms_per_tick = $1 * TIME_MULTIPLIER;
      next;
   }

   # Match measure lines, e.g.:
   #
   #   12 + + +
   #
   # Where "12" is the measure number, and '+' is a ruler for marking where
   # to look for notes.
   next unless $line =~ /^\d+[ +]*$/;

   unless( defined($ms_per_tick) )
   {
      die "Missing timing information, need ms_per_tick=<number> lines\n";
   }

   my $ruler = $line;
   my $ticks;
   for(my $i = 0; $i < TRACK_COUNT; $i++)
   {
      $line = <>;

      # Extract notes aligned to ruler marks.
      $ticks = 0;
      for(my $j = 0; $j < length($ruler); $j++)
      {
         next unless substr($ruler, $j, 1) eq '+';

         my $note = substr($line, $j);
         my $t = $time + $ms_per_tick * $ticks;
         if( $note =~ /^(\d+)!/ )
         {
            # Note at full velocity.
            push @{$tracks[$i]}, [$t, $1, FULL_VELOCITY];

            # Remove note from line.
            my $note_size = length($1) + 1;
            substr($line, $j, $note_size) = "-" x $note_size;
         }
         elsif( $note =~ /^(\d+)/ )
         {
            # Note at reduced velocity.
            push @{$tracks[$i]}, [$t, $1, REDUCED_VELOCITY];

            my $note_size = length($1);
            substr($line, $j, $note_size) = "-" x $note_size;
         }
         elsif( $note =~ /^[Xx]/ )
         {
            # Reset note.
            push @{$tracks[$i]}, [$t, 0, REST_NOTE];
            substr($line, $j, 1) = "-";
         }
         $ticks++;
      }

      # Check if there are any notes or rest notes that were not removed.
      # Those are left behind because they weren't aligned to tick marks,
      # so we will flag those as errors.
      if( $line =~ /^(.*?)[0-9Xx]/ )
      {
         die "Unaligned note at line $. column " . (length($1) + 1) . "\n";
      }
   }

   $time += $ms_per_tick * $ticks;
}

# Output tracks.
if( (scalar @tuning) != TRACK_COUNT )
{
   die "Missing tuning definition, need tuning=EADGBE or tuning=DADGBE\n";
}
for(my $i = 0; $i < TRACK_COUNT; $i++)
{
   print "static const SongNote ${variable_prefix}_track$i\[] =\n{\n";
   my @t = @{$tracks[$i]};
   for(my $j = 0; $j < scalar @t; $j++)
   {
      my $duration = MAX_DURATION_MILLISECONDS;
      if( $j + 1 <= $#t )
      {
         $duration = $t[$j + 1][0] - $t[$j][0];
         if( $duration > MARGIN )
         {
            $duration -= MARGIN;
         }
      }
      if( $duration > MAX_DURATION_MILLISECONDS )
      {
         $duration = MAX_DURATION_MILLISECONDS;
      }
      if( $t[$j][2] != REST_NOTE )
      {
         print "\t{", $t[$j][0],
               ", ", int($duration * $t[$j][2]),
               ", ", $tuning[$i] + $t[$j][1],
               "},\n";
      }
   }

   print "\t{$time, 0, 0},\n",
         "\t{-1, 0, 0}\n",
         "};\n";
}

# Collect all tracks in a single array.
print "static const Song ", $variable_prefix, " =\n{\n\t{\n";
for(my $i = 0; $i < TRACK_COUNT; $i++)
{
   print "\t\t${variable_prefix}_track$i,\n";
}
print "\t}\n};\n";
