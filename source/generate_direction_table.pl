#!/usr/bin/perl -w
# Generate various rotation related tables, following playdate's angle
# convention where 0 degrees is up and 90 degrees is right.

use strict;
use constant PI => 3.14159265358979323846264338327950288419716939937510;
use constant PROJECTILE_VELOCITY => 13;
use constant CURSOR_RADIUS => 50;
use constant CURSOR_HEIGHT => 10;
use constant CURSOR_BASE_HALF_WIDTH => 6;
use constant SCREEN_CENTER_X => 400 / 2;
use constant SCREEN_CENTER_Y => 240 / 2;

# Generate velocity table.
print "static const short kProjectileVelocity[361][2] =\n{\n";
for(my $i = 0; $i <= 360; $i++)
{
   my $a = $i * PI / 180.0 - PI / 2;
   print "\t{", int(PROJECTILE_VELOCITY * cos($a)),
         ", ", int(PROJECTILE_VELOCITY * sin($a)),
         "},\n";
}
print "};\n";

# Generate table of cursor coordinates.
print "static const short kCursorCoordinates[361][6] =\n{\n";
for(my $i = 0; $i <= 360; $i++)
{
   my $a = $i * PI / 180.0 - PI / 2;

   # Compute center of triangle base.
   my $dx = cos($a);
   my $dy = sin($a);
   my $base_x = CURSOR_RADIUS * $dx + SCREEN_CENTER_X;
   my $base_y = CURSOR_RADIUS * $dy + SCREEN_CENTER_Y;

   # Compute coordinate of triangle tip.
   my $ax = int($base_x + CURSOR_HEIGHT * $dx);
   my $ay = int($base_y + CURSOR_HEIGHT * $dy);

   # Compute coordinates of the two base points.
   my $bx = int($base_x + CURSOR_BASE_HALF_WIDTH * -$dy);
   my $by = int($base_y + CURSOR_BASE_HALF_WIDTH * $dx);
   my $cx = int($base_x + CURSOR_BASE_HALF_WIDTH * $dy);
   my $cy = int($base_y + CURSOR_BASE_HALF_WIDTH * -$dx);

   print "\t{$ax, $ay, $bx, $by, $cx, $cy},\n";
}
print "};\n";
