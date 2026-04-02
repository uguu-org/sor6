#!/usr/bin/perl -w
# Generate table of (distance, angle) values for song 11.  This gives us
# a fast way to convert from cartesian coordinates to polar coordinates,
# albeit at a lower resolution.
#
# This is done as a table since playdate barely has enough CPU power to
# do 1500 sqrt+atan2 operations at 30 FPS.  It can do it if we cache all
# entries in a table and only update the edges on scroll, but that
# brings in some extra complexity.  Since there aren't that many entries
# for the distance we want to cover, and we still have a fair bit of
# memory left to spare, we will just precompute the whole table.

use strict;
use constant PI => 3.14159265358979323846264338327950288419716939937510;

# Horizontal radius covered by the table in pixels.  We are most interested
# in distances up to 671 since that's the orbital radius of the special target.
# 1024 is the next power of 2 up from 671.
use constant TABLE_RADIUS => 1024;

# Number of pixels covered by each table entry.
#
# 8 pixels is a good size, and would result in 1500 rectangles being drawn
# at every frame.  We do have enough memory to support 4 pixel blocks, but
# we don't have the CPU to draw 6000 rectangles at 30fps.
use constant BLOCK_SIZE => 8;
(BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0 or die;

# Number of table entries across one dimension.
use constant TABLE_SIZE => TABLE_RADIUS * 2 / BLOCK_SIZE;

# Scaling factor for distances.
use constant DISTANCE_SCALE => 64;

# Output header.
print "#define BG11_BLOCK_SIZE ", BLOCK_SIZE, "\n",
      "#define BG11_BLOCK_MASK ", BLOCK_SIZE - 1, "\n",
      "#define BG11_TABLE_SIZE ", TABLE_SIZE, "\n",
      "#define BG11_DISTANCE_SCALE ", DISTANCE_SCALE, "\n",
      "static const BG11Cell kBG11Grid[", TABLE_SIZE, "][", TABLE_SIZE, "] =\n",
      "{\n";

for(my $y = -TABLE_RADIUS; $y < TABLE_RADIUS; $y += BLOCK_SIZE)
{
   print "\t{\n";
   for(my $x = -TABLE_RADIUS; $x < TABLE_RADIUS; $x += BLOCK_SIZE)
   {
      my $d = int(sqrt($x * $x + $y * $y) * DISTANCE_SCALE);
      if( $d > 0xffff )
      {
         # For distances outside of maximum radius, we will use a constant
         # 0xffff for the angle.  This is because we weren't going to draw
         # anything at those distances anyways, so we might as well use
         # a constant to improve compression.
         print "\t\t{0xffff, 0xffff},\n";
      }
      else
      {
         my $a = ($x != 0 || $y != 0) ? atan2($y, $x) : 0;
         if( $a < 0 ) { $a += 2 * PI; }
         $a = int($a * 0x10000 / (2 * PI));
         $a >= 0 or die;
         $a < 0x10000 or die;
         print "\t\t{$d, $a},\n";
      }
   }
   print "\t},\n";
}

print "};\n";
