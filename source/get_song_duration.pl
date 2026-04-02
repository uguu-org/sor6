#!/usr/bin/perl -w
# Parse the output of convert_tab_to_c.pl to collect duration stats.

use strict;

sub output_duration($$)
{
   my ($name, $duration) = @_;

   printf '%s: %dms = %d:%02d.%03d'."\n",
          $name,
          $duration,
          int($duration / 60000),
          int(($duration % 60000) / 1000),
          $duration % 1000;
}

my $current_file = undef;
my $duration = 0;
my $file_count = 0;
my $total_duration = 0;
while( my $line = <> )
{
   # Output duration when we have moved on to a new file.
   unless( defined($current_file) && $current_file eq $ARGV )
   {
      if( defined($current_file) )
      {
         output_duration($current_file, $duration);
         $total_duration += $duration;
      }
      $current_file = $ARGV;
      $duration = 0;
      $file_count++;
   }

   if( $line =~ /^\s*\{(\d+),\s*(?:-)?\d+,\s*(?:-)?\d+\}/ )
   {
      if( $duration < $1 )
      {
         $duration = $1;
      }
   }
}

# Output duration for last file.
if( defined($current_file) )
{
   output_duration($current_file, $duration);
   $total_duration += $duration;
}
if( $file_count > 1 )
{
   output_duration("total", $total_duration);
}
