#!/usr/bin/perl -w
# Generate SVG for rendering firework frames.

use strict;
use constant PI => 3.14159265358979323846264338327950288419716939937510;

# Animate this many particles in each firework.
use constant PARTICLE_COUNT => 777;

# Number of frames to draw at full opacity.
use constant LIVE_FRAMES => 20;

# Number of frames to fade toward zero opacity.
use constant FADE_FRAMES => 10;

# Total number of animation frames.
use constant TOTAL_FRAMES => (LIVE_FRAMES + FADE_FRAMES);

# Firework radius.
use constant RADIUS => 50;

# Size of each output image tile in pixels.
use constant TILE_SIZE => 100;


# Compute coordinates for a single particle.
sub point($$$)
{
   my ($final_x, $final_y, $frame) = @_;

   # Start out fast and decelerate toward end position.
   my $r = $frame / TOTAL_FRAMES - 1;
   $r = 1 - ($r * $r);

   # Actual firework should also fall a bit near the end, but we don't
   # want to bake that into the sprites.  Instead, the vertical
   # adjustment will be done at run time.  This allows us to flip the
   # sprites around for more variations.
   my $x = $final_x * $r;
   my $y = $final_y * $r;
   $x > -TILE_SIZE / 2 or die;
   $y > -TILE_SIZE / 2 or die;
   $x < TILE_SIZE / 2 or die;
   $y < TILE_SIZE / 2 or die;
   return ($x, $y);
}


# Use deterministic seed.
srand(1);

# Output header.
my $width = TILE_SIZE * TOTAL_FRAMES;
my $height = TILE_SIZE;
print <<"EOT";
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg
   width="$width" height="$height"
   viewBox="0 0 $width $height"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
EOT

# Generate final locations for each particle.
my @particles = ();
for(my $i = 0; $i < PARTICLE_COUNT; $i++)
{
   my $r = RADIUS * cos(rand(0.5 * PI));

   my $a = rand(2 * PI);
   push @particles, [$r * cos($a), $r * sin($a)];
}

# Draw lines for live particles.
for(my $f = 0; $f < TOTAL_FRAMES; $f++)
{
   for(my $i = 0; $i < PARTICLE_COUNT; $i++)
   {
      my ($x0, $y0) = point($particles[$i][0], $particles[$i][1], $f);
      my ($x1, $y1) = point($particles[$i][0], $particles[$i][1], $f + 1);

      $x0 += TILE_SIZE / 2 + TILE_SIZE * $f;
      $y0 += TILE_SIZE / 2;
      $x1 += TILE_SIZE / 2 + TILE_SIZE * $f;
      $y1 += TILE_SIZE / 2;
      my $style = "fill:none;stroke:#ffffff;stroke-linecap:round";
      if( $f >= LIVE_FRAMES )
      {
         $style .= ";stroke-opacity:" . (1 - ($f - LIVE_FRAMES) / FADE_FRAMES);
      }
      print <<"EOT";
<path id="F${f}_${i}" style="$style" d="M $x0,$y0 $x1,$y1" />
EOT
   }
}

# Output footer.
print "</svg>\n";
