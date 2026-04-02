#!/usr/bin/perl -w
# Generate SVG for drawing rotating gears.

use strict;
use constant PI => 3.14159265358979323846264338327950288419716939937510;

# Radii for each gear:
# R = base radius.
# R0 = inner circle.
# R1 = teeth inner radius.
# R2 = teeth outer radius.
use constant LARGE_GEAR_R => 50;
use constant LARGE_GEAR_R0 => LARGE_GEAR_R - 20;
use constant LARGE_GEAR_R1 => LARGE_GEAR_R - 5;
use constant LARGE_GEAR_R2 => LARGE_GEAR_R + 3;
use constant SMALL_GEAR_R => LARGE_GEAR_R / 2;
use constant SMALL_GEAR_R0 => SMALL_GEAR_R - 20;
use constant SMALL_GEAR_R1 => SMALL_GEAR_R - 5;
use constant SMALL_GEAR_R2 => SMALL_GEAR_R + 3;

# Number of teeth in each gear.
use constant LARGE_GEAR_TEETH_COUNT => 12;
use constant SMALL_GEAR_TEETH_COUNT => 6;

# Thickness of large gear spoke, specified as number of degrees.
# See "k" angle in gear_test.svg
use constant LARGE_GEAR_SPOKE_ANGLE => 9;

# Length of each square tile edge in pixels.
use constant TILE_SIZE => 128;

# Number of animation frames.
use constant FRAME_COUNT => 60;

# Number of path steps.
use constant GEAR_STEP_COUNT => 720;
use constant INNER_CIRCLE_STEP_COUNT => 90;

# Gear style.
use constant STYLE => "fill:#000000;" .
                      "stroke:#ffffff;" .
                      "stroke-width:1;" .
                      "stroke-linecap:round;" .
                      "stroke-linejoin:round";

# Output teeth path outline for a single gear.
sub GenerateGear($$$$$$)
{
   my ($cx, $cy, $angle, $r2, $r3, $teeth) = @_;

   print "M ";
   for(my $i = 0; $i < GEAR_STEP_COUNT; $i++)
   {
      my $a = $angle + 2 * PI * $i / GEAR_STEP_COUNT;

      my $t = ($i / GEAR_STEP_COUNT) * $teeth;
      $t -= int($t);
      $t >= 0 or die;
      $t < 1 or die;

      #  0   0.2           0.8   1
      #  _____              _____
      #       \            /
      #        \__________/
      #       0.3        0.7

      if( $t < 0.2 )
      {
         print $cx + $r3 * cos($a), ",",
               $cy + $r3 * sin($a), " ";
         next;
      }
      if( $t < 0.3 ) { next; }
      if( $t < 0.7 )
      {
         print $cx + $r2 * cos($a), ",",
               $cy + $r2 * sin($a), " ";
         next;
      }
      if( $t < 0.8 ) { next; }
      print $cx + $r3 * cos($a), ",",
            $cy + $r3 * sin($a), " ";
   }
   print "z";
}


# Output header.
my $width = TILE_SIZE * FRAME_COUNT;
my $height = TILE_SIZE * 2;
print <<"EOT";
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg
   width="$width" height="$height"
   viewBox="0 0 $width $height"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
EOT

# Generate large gear.
for(my $i = 0; $i < FRAME_COUNT; $i++)
{
   my $cx = $i * TILE_SIZE + TILE_SIZE / 2;
   my $cy = TILE_SIZE / 2;

   print "<path id=\"L$i\" style=\"", STYLE, "\" d=\"";

   # Generate teeth.
   my $a = $i * (PI / 2) / FRAME_COUNT;
   GenerateGear($cx,
                $cy,
                $a,
                LARGE_GEAR_R1,
                LARGE_GEAR_R2,
                LARGE_GEAR_TEETH_COUNT);

   # Generate circular hole with cross.
   for(my $j = 0; $j < 4; $j++)
   {
      print " M ";
      for(my $k = 90 - LARGE_GEAR_SPOKE_ANGLE;
          $k >= LARGE_GEAR_SPOKE_ANGLE; $k--)
      {
         my $ca = $a + ($j + $k / 90) * (PI / 2);
         print $cx + LARGE_GEAR_R0 * cos($ca), ",",
               $cy + LARGE_GEAR_R0 * sin($ca), " ";
      }

      my $spoke_radius =
         LARGE_GEAR_R0 * sin(LARGE_GEAR_SPOKE_ANGLE * PI / 180) * sqrt(2);

      print $cx + $spoke_radius * cos($a + ($j + 0.5) * PI / 2), ",",
            $cy + $spoke_radius * sin($a + ($j + 0.5) * PI / 2), " z";
   }

   print "\" />\n";
}

# Generate small gear.
for(my $i = 0; $i < FRAME_COUNT; $i++)
{
   my $a = $i * PI / FRAME_COUNT;
   my $cx = $i * TILE_SIZE + TILE_SIZE / 2;
   my $cy = TILE_SIZE + TILE_SIZE / 2;

   print "<path id=\"S$i\" style=\"", STYLE, "\" d=\"";

   # Generate teeth.
   GenerateGear($cx,
                $cy,
                $a,
                SMALL_GEAR_R1,
                SMALL_GEAR_R2,
                SMALL_GEAR_TEETH_COUNT);

   # Generate path for inner circle.
   print " M ";
   for(my $j = 0; $j < INNER_CIRCLE_STEP_COUNT; $j++)
   {
      my $aa = -($j * 2 * PI / INNER_CIRCLE_STEP_COUNT);
      print $cx + SMALL_GEAR_R0 * cos($aa), ",",
            $cy + SMALL_GEAR_R0 * sin($aa), " ";
   }
   print "z";

   print "\" />\n";
}

# Output footer.
print "</svg>\n";
